// game of live on gpu and cpu
// cd C:/Users/Azerty/Desktop/Программы/filescpp; g++ game_of_live.cpp -o game_of_live.exe -I"c:/Users/Azerty/Downloads/SFML-2.6.1/include" -L"c:/Users/Azerty/Downloads/SFML-2.6.1/lib" -lsfml-graphics-d -lsfml-window-d -lsfml-system-d -lopengl32 -lwinmm -lgdi32 -lcomdlg32 -lws2_32 -fconcepts-diagnostics-depth=2 -Wfatal-errors -O3 -std=c++20 -fopenmp
//#define CPU_MODE
#include <limits>
#include <optional>
#include <filesystem>
#include "libs/serialize.cpp"
#ifndef ANDROID_MODE
#include <SFML/Graphics.hpp>
#else
#include <GLES3/gl32.h>
#include <GLES3/gl3ext.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
//#include <android/asset_manager.h>
//#include <android/asset_manager_jni.h>
#endif
#include <random>
// #include <iostream>
#ifndef CPU_MODE
#include "gpu_loader.cpp"
#else
#ifdef ANDROID_MODE
#include "gpu_loader.cpp"
#endif
#endif
#include <mutex>

void print(const std::string& str) {
#ifndef ANDROID_MODE
    std::cout << str << std::endl;
#else
    SDL_Log("%s", str.c_str());
#endif
}

int width = 800;
int height = 600;
#ifdef ANDROID_MODE
const char* vertex_shader_code = "#version 310 es\n"
                                 "layout(location = 0) in vec2 pos;\n"
                                 "layout(location = 1) in lowp vec4 color;\n"
                                 "layout(location = 2) in vec2 texCoord;\n" // Принимаем UV
                                 "out lowp vec4 vColor;\n"
                                 "out vec2 vTexCoord;\n"
                                 "uniform vec2 u_res;\n"
                                 "void main() {\n"
                                 "    gl_Position = vec4((pos.x * u_res.x * 2.0) - 1.0, (pos.y * u_res.y * -2.0) + 1.0, 0.0, 1.0);\n"
                                 "    vColor = color;\n"
                                 "    vTexCoord = texCoord;\n"
                                 "}\n";

const char* fragment_shader_code = "#version 310 es\n"
                                   "precision mediump float;\n"
                                   "in lowp vec4 vColor;\n"
                                   "in vec2 vTexCoord;\n"
                                   "out lowp vec4 fragColor;\n"
                                   "uniform sampler2D u_texture;\n" // Текстура атласа
                                   "uniform uint u_useTexture;\n"   // Флаг: 1 - текст, 0 - клетки
                                   "void main() {\n"
                                   "    if (u_useTexture == 0u) {\n"
                                   "        fragColor = vColor;\n"
                                   "        return;"
                                   "    }\n"
                                   "    fragColor = texture(u_texture, vTexCoord) * vColor;\n"
                                   "}\n";
GLuint graphicsProgramID = 0;

void startAndroidSFML() {
    auto checkShader = [](GLuint shader) {
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            GLint logLength;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
            std::vector<GLchar> log(logLength);
            glGetShaderInfoLog(shader, logLength, nullptr, log.data());
            SDL_Log("Shader Error: %s\n", log.data());
        }
    };
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertex_shader_code, nullptr);
    glCompileShader(vertexShader);
    checkShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragment_shader_code, nullptr);
    glCompileShader(fragmentShader);
    checkShader(fragmentShader);

    graphicsProgramID = glCreateProgram();
    glAttachShader(graphicsProgramID, vertexShader);
    glAttachShader(graphicsProgramID, fragmentShader);
    glLinkProgram(graphicsProgramID);

    GLint linkSuccess;
    glGetProgramiv(graphicsProgramID, GL_LINK_STATUS, &linkSuccess);
    if (!linkSuccess) {
        GLint logLength;
        glGetProgramiv(graphicsProgramID, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<GLchar> log(logLength);
        glGetProgramInfoLog(graphicsProgramID, logLength, nullptr, log.data());
        SDL_Log("Program Link Error: %s\n", log.data());
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glUseProgram(graphicsProgramID);
    GLint resLoc = glGetUniformLocation(graphicsProgramID, "u_res");
    glUniform2f(resLoc, 1/(float)width, 1/(float)height);
}
GLuint loadTextureSDL3(const char* filename) {
    // 1. Открываем файл из assets (SDL3 сам знает, где они лежат в Android)
    size_t size;
    void* fileData = SDL_LoadFile(filename, &size);
    if (!fileData) return 0;

    // 2. Декодируем байты через stb_image
    int w, h, ch;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* pixels = stbi_load_from_memory((unsigned char*)fileData, (int)size, &w, &h, &ch, 4);
    SDL_free(fileData); // SDL-память больше не нужна

    if (!pixels) return 0;

    // 3. Создаем текстуру в OpenGL ES 3.2
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    // 4. Настройки фильтрации (минимум кода)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(pixels);
    return texID;
}
namespace sf {
    class Texture {
    public:
        GLuint id;
        Texture() = default;
        ~Texture() {
            if (id != 0) glDeleteTextures(1, &id);
        }
        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;

        bool loadFromFile(std::string filename) {
            id = loadTextureSDL3(filename.c_str());
            return id!=0;
        }
        bool load(std::string filename) {
            return loadFromFile(std::move(filename));
        }
        void use() {
            glUseProgram(graphicsProgramID);
            GLint useTexLoc = glGetUniformLocation(graphicsProgramID, "u_useTexture");
            glUniform1ui(useTexLoc, 1);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, id);
            GLint texLoc = glGetUniformLocation(graphicsProgramID, "u_texture");
            glUniform1i(texLoc, 0);
        }
        void unuse() {
            glUseProgram(graphicsProgramID);
            GLint useTexLoc = glGetUniformLocation(graphicsProgramID, "u_useTexture");
            glUniform1ui(useTexLoc, 0);
        }
    };
    enum PrimitiveType {
        Points,
        Lines,
        LineStrip,
        Triangles,
        TriangleStrip,
        TriangleFan,
        Quads
    };
    template <typename T>
    struct Vector2 {
        T x;
        T y;
        sf::Vector2<T> operator+(const sf::Vector2<T>& other) const {
            return sf::Vector2<T>(x+other.x, y+other.y);
        }
        sf::Vector2<T> operator-(const sf::Vector2<T>& other) const {
            return sf::Vector2<T>(x-other.x, y-other.y);
        }
        void operator+=(const sf::Vector2<T>& other) {
            x+=other.x; y+=other.y;
        }
    };
    using Vector2f = Vector2<float>;
    using Vector2i = Vector2<int>;
    class Time {
    private:
        long long direction;
    public:
        Time(long long direction) {
            this->direction = direction;
        }
        float asSeconds() {
            return direction/1000.0f;
        }
        long long get_direction() {
            return direction;
        }
    };
    class Clock {
    private:
        long long last_time;
    public:
        Clock() {
            last_time = SDL_GetTicks();
        }
        Time restart() {
            long long new_time = SDL_GetTicks();
            long long direction = new_time-last_time;
            last_time = new_time;
            return sf::Time(direction);
        }
    };
    struct Color {
        static const Color Black;
        static const Color White;
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
        Color(): r(0), g(0), b(0), a(0) {};
        Color(uint8_t r, uint8_t g, uint8_t b): r(r), g(g), b(b), a(255) {}
        Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a): r(r), g(g), b(b), a(a) {}
    };
    inline const Color Color::Black(0, 0, 0);
    inline const Color Color::White(255, 255, 255);
    struct Vertex {
        sf::Vector2f position;
        sf::Color color;
        sf::Vector2f texCoords; // Добавили для поддержки текстур (шрифтов)
        Vertex() = default;
    };

    class VertexArray {
    private:
        std::vector<Vertex> m_vertices;
        GLuint m_vbo;
        sf::PrimitiveType m_type;

    public:
        VertexArray(sf::PrimitiveType type) : m_type(type) {
            glGenBuffers(1, &m_vbo);
        }

        ~VertexArray() {
            glDeleteBuffers(1, &m_vbo);
        }

        void resize(size_t size) { m_vertices.resize(size); }
        void clear() { m_vertices.clear(); }
        size_t capacity() const { return m_vertices.capacity(); }
        size_t getVertexCount() const { return m_vertices.size(); }
        Vertex& operator[](size_t index) { return m_vertices[index]; }
        void render() {
            if (m_vertices.empty()) return;

            // УДАЛЕНО: glUniform1i(useTexLoc, 0); — теперь не сбрасываем состояние текстуры

            glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
            glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(Vertex), m_vertices.data(), GL_STREAM_DRAW);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, color));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

            if (m_type == sf::Quads) {
                for (size_t i = 0; i < m_vertices.size(); i += 4) {
                    glDrawArrays(GL_TRIANGLE_FAN, (GLint)i, 4);
                }
            } else {
                glDrawArrays(GL_TRIANGLE_FAN, 0, (GLsizei)m_vertices.size());
            }

            glDisableVertexAttribArray(0);
            glDisableVertexAttribArray(1);
            glDisableVertexAttribArray(2);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
    };
    class RectangleShape {
    private:
        VertexArray m_vertices;
        sf::Vector2f m_size;
        sf::Vector2f m_position;
        sf::Color m_color;
        void updateGeometry() {
            m_vertices[0].position = sf::Vector2f(m_position.x, m_position.y);
            m_vertices[1].position = sf::Vector2f(m_position.x + m_size.x, m_position.y);
            m_vertices[2].position = sf::Vector2f(m_position.x + m_size.x, m_position.y + m_size.y);
            m_vertices[3].position = sf::Vector2f(m_position.x, m_position.y + m_size.y);
            for (int i = 0; i < 4; ++i) m_vertices[i].color = m_color;
        }
    public:
        RectangleShape(const sf::Vector2f& size = sf::Vector2f(0, 0))
                : m_vertices(sf::Quads), m_size(size), m_color(sf::Color::White) {
            m_vertices.resize(4);
        }
        void setTexture() {
            m_vertices[0].texCoords = sf::Vector2f(0, 0);
            m_vertices[1].texCoords = sf::Vector2f(1, 0);
            m_vertices[2].texCoords = sf::Vector2f(1, 1);
            m_vertices[3].texCoords = sf::Vector2f(0, 1);
        }
        void setPosition(const sf::Vector2f& position) {
            m_position = position;
            updateGeometry();
        }
        void setSize(const sf::Vector2f& size) {
            m_size = size;
        }
        void setFillColor(const sf::Color& color) {
            m_color = color;
        }
        void setTransparency() {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        void setUnTransparency() {
            glDisable(GL_BLEND);
        }
        void render() {
            updateGeometry();
            if (m_color.a!=255) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            m_vertices.render();
            if (m_color.a != 255) {
                glDisable(GL_BLEND);
            }
        }
    };
    class Font {
    private:
        GLuint m_textureID;
        float m_glyphStep;
    public:
        float m_widths[95];
        Font() : m_textureID(0), m_glyphStep(0.0f) {}
        ~Font() {
            if (m_textureID != 0) glDeleteTextures(1, &m_textureID);
        }
        bool loadFromFile(const std::string& filename) {
            if (!TTF_Init()) return false;
            TTF_Font* ttf = TTF_OpenFont(filename.c_str(), 48);
            if (!ttf) return false;

            SDL_Color white = {255, 255, 255, 255};
            int glyphSize = 64; // Увеличим ячейку до степени двойки (так лучше для GPU)
            int texSize = 1024; // 1024x1024 влезет всё с запасом

            SDL_Surface* surf = SDL_CreateSurface(texSize, texSize, SDL_PIXELFORMAT_RGBA32);
            if (!surf) { TTF_CloseFont(ttf); return false; }

            // Заполняем полностью прозрачным цветом
            SDL_FillSurfaceRect(surf, NULL, 0x00000000);

            for (int i = 0; i < 95; ++i) {
                char c = (char)(i + 32);
                SDL_Surface* glyph = TTF_RenderGlyph_Blended(ttf, (Uint16)c, white);
                if (glyph) {
                    int realW = 0;
                    // Проходим по столбцам справа налево
                    for (int x = glyph->w - 1; x >= 0; --x) {
                        bool colHasPixels = false;
                        for (int y = 0; y < glyph->h; ++y) {
                            // Получаем адрес пикселя (RGBA32 = 4 байта на пиксель)
                            Uint32* pixels = (Uint32*)glyph->pixels;
                            Uint32 pixel = pixels[y * (glyph->pitch / 4) + x];

                            // Извлекаем альфа-канал (в RGBA32 это обычно старшие или младшие 8 бит)
                            Uint8 a = (Uint8)((pixel >> 24) & 0xFF); // Обычно в SDL_PIXELFORMAT_RGBA32 альфа в конце
                            if (pixel == 0) a = 0; // На всякий случай, если пиксель пустой

                            // Если используем Blended, смотрим на прозрачность больше порога
                            if ((pixel & 0xFF000000) != 0) { // Простая проверка на наличие любого цвета/альфы
                                colHasPixels = true;
                                break;
                            }
                        }
                        if (colHasPixels) {
                            realW = x + 1;
                            break;
                        }
                    }

                    // Если буква — пробел, даем ей фиксированную ширину
                    if (c == ' ') realW = 30;
                    m_widths[i] = realW/48.0;

                    int row = i / 10;
                    int col = i % 10;
                    // Центрируем букву в ячейке, чтобы края не лезли на соседей
                    SDL_Rect dest = { col * glyphSize + 2, row * glyphSize + 2, glyph->w, glyph->h };
                    SDL_BlitSurface(glyph, NULL, surf, &dest);
                    SDL_DestroySurface(glyph);
                }
            }

            m_glyphStep = (float)glyphSize / (float)texSize;

            if (m_textureID != 0) glDeleteTextures(1, &m_textureID);
            glGenTextures(1, &m_textureID);
            glBindTexture(GL_TEXTURE_2D, m_textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texSize, texSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, surf->pixels);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            SDL_DestroySurface(surf);
            TTF_CloseFont(ttf);
            return true;
        }
        GLuint getTextureID() const { return m_textureID; }
        float getGlyphStep() const { return m_glyphStep; }
    };
    class Text {
    private:
        VertexArray m_vertices;
        std::string m_string;
        sf::Vector2f m_position;
        sf::Color m_color;
        unsigned int m_characterSize;
        const Font* m_font;
    public:
        Text() : m_vertices(sf::Quads), m_characterSize(30), m_color(sf::Color::White), m_font(nullptr), m_position(0, 0) {}
        void setFont(const Font& font) {
            m_font = &font;
            updateGeometry();
        }
        void setString(const std::string& str) {
            m_string = str;
            updateGeometry();
        }
        void setCharacterSize(unsigned int size) { m_characterSize = size; updateGeometry(); }
        void setFillColor(const sf::Color& color) { m_color = color; updateGeometry(); }
        void setPosition(const sf::Vector2f& pos) { m_position = pos; updateGeometry(); }
        void setPosition(float x, float y) { m_position = sf::Vector2f(x, y); updateGeometry(); }
        void updateGeometry() {
            if (!m_font || m_string.empty()) return;
            m_vertices.resize(m_string.size() * 4);

            float xCursor = m_position.x;
            float stepUV = m_font->getGlyphStep();

            for (size_t i = 0; i < m_string.size(); ++i) {
                if ((char)m_string[i]-32>95) {
                    m_string[i] = '?';
                }
                size_t idx = i * 4;

                // charW — это визуальная ширина буквы на экране.
                // Сделаем её поменьше, чтобы убрать пробелы (r a n d o m -> random)
                float charW = (float)m_characterSize;
                float charH = (float)m_characterSize;

                m_vertices[idx + 0].position = sf::Vector2f(xCursor, m_position.y);
                m_vertices[idx + 1].position = sf::Vector2f(xCursor + charW, m_position.y);
                m_vertices[idx + 2].position = sf::Vector2f(xCursor + charW, m_position.y + charH);
                m_vertices[idx + 3].position = sf::Vector2f(xCursor, m_position.y + charH);

                unsigned char c = (unsigned char)m_string[i];
                int charIndex = (c >= 32 && c <= 126) ? (c - 32) : 0;

                int row = charIndex / 10;
                int col = charIndex % 10;

                // Добавляем микро-отступ (0.001f), чтобы не цеплять мусор с соседних ячеек
                float uStart = col * stepUV + 0.002f;
                float vStart = row * stepUV + 0.002f;
                // Берем чуть меньше ширины ячейки (например, 0.7 от шага),
                // так как буквы не занимают всю ячейку 64x64
                float uEnd = uStart + stepUV * 0.7f;
                float vEnd = vStart + stepUV * 0.8f;

                m_vertices[idx + 0].texCoords = sf::Vector2f(uStart, vStart);
                m_vertices[idx + 1].texCoords = sf::Vector2f(uEnd,   vStart);
                m_vertices[idx + 2].texCoords = sf::Vector2f(uEnd,   vEnd);
                m_vertices[idx + 3].texCoords = sf::Vector2f(uStart, vEnd);

                for(int j = 0; j < 4; ++j) m_vertices[idx + j].color = m_color;

                xCursor += charW*m_font->m_widths[(char)m_string[i]-32]; // Сдвигаем курсор на реальную ширину буквы
            }
        }

        void render() {
            if (!m_font || m_font->getTextureID() == 0) return;
            glUseProgram(graphicsProgramID);
            GLint useTexLoc = glGetUniformLocation(graphicsProgramID, "u_useTexture");
            glUniform1ui(useTexLoc, 1);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_font->getTextureID());
            GLint texLoc = glGetUniformLocation(graphicsProgramID, "u_texture");
            glUniform1i(texLoc, 0);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            m_vertices.render();
            glDisable(GL_BLEND);
            glUniform1ui(useTexLoc, 0);
        }
    };
    class ConvexShape {
    private:
        VertexArray m_vertices;
        sf::Color m_color;
        sf::Vector2f m_position;

    public:
        ConvexShape(size_t pointCount = 0) : m_vertices(sf::TriangleFan), m_color(sf::Color::White), m_position(sf::Vector2f(0, 0)) {
            m_vertices.resize(pointCount);
        }

        void setPointCount(size_t count) { m_vertices.resize(count); }
        void setPoint(size_t index, const sf::Vector2f& point) {
            m_vertices[index].position = m_position + point;
            m_vertices[index].color = m_color;
        }
        void setFillColor(const sf::Color& color) {
            m_color = color;
            for (size_t i = 0; i < m_vertices.getVertexCount(); ++i) m_vertices[i].color = color;
        }
        void setPosition(const sf::Vector2f& pos) {
            sf::Vector2f diff = pos - m_position;
            m_position = pos;
            for (size_t i = 0; i < m_vertices.getVertexCount(); ++i) m_vertices[i].position += diff;
        }

        void render() {
            m_vertices.render();
        }
    };
    class CircleShape {
    private:
        sf::ConvexShape cnvsh;
        sf::Vector2f pos;
        sf::Vector2f size;
        sf::Color col;
        void updateGeometry() {
            float deg = 3.1415/180*360/30;

            for (int i = 0; i!=30; i++) {
                float curr_deg = deg*i;
                cnvsh.setPoint(i, pos+sf::Vector2f(size.x*0.5+sin(curr_deg)*size.x*0.5, size.y*0.5+cos(curr_deg)*size.x*0.5));
            }
            cnvsh.setFillColor(col);
        }
    public:
        CircleShape(): cnvsh(30) {}
        void setPosition(sf::Vector2f pos) {
            this->pos = pos;
        }
        void setSize(sf::Vector2f size) {
            this->size = size;
        }
        void setFillColor(sf::Color col) {
            this->col =col;
        }
        void render() {
            updateGeometry();
            cnvsh.render();
        }
    };
};
void clearWindow(sf::Color color) {
    glClearColor(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f);
    glClear(GL_COLOR_BUFFER_BIT);
}
#endif
std::mutex mtx;
uint32_t size = 20;
double camx = 10;
double camy = 10;
// 1.0 scale=std::min(width px, height px)
double scale = 1.0;

inline double get_base_scale() {
    return (double)std::min(width, height) / size;
}

inline sf::Vector2f calc_pos(int x, int y) {
    double s = get_base_scale() * scale;
    float px = ((x - camx) * s + width / 2.0);
    float py = (height / 2.0 - (y - camy) * s);
    return sf::Vector2f(px, py);
}
inline sf::Vector2f calc_pos(int x, int y, double gbs) {
    float px = ((x - camx) * gbs + width / 2.0);
    float py = (height / 2.0 - (y - camy) * gbs);
    return sf::Vector2f(px, py);
}
long long safeDoubleToLongLong(double value) {
    constexpr double max_val = static_cast<double>(std::numeric_limits<long long>::max());
    constexpr double min_val = static_cast<double>(std::numeric_limits<long long>::min());

    if (value > max_val) [[unlikely]] {
        return std::numeric_limits<long long>::max();
    }
    if (value < min_val) [[unlikely]] {
        return std::numeric_limits<long long>::min();
    }

    return static_cast<long long>(value);
}
inline float calc_size_px() {
    return get_base_scale() * scale;
}
inline int calc_size_ssbo() {
    return (size*size + 31) / 32*4;
}
inline unsigned long long get_id_ceil(const sf::Vector2f pos) {
    double s = get_base_scale() * scale;
    long long x = safeDoubleToLongLong(std::floor((pos.x - width/2) / s + camx));
    long long y = safeDoubleToLongLong(std::floor((height/2-pos.y) / s + camy))+1;
    if (x < 0 || y < 0 || x >= size || y >= size) {
        return std::numeric_limits<unsigned long long>::max();
    }
    return y * size + x;
}
inline sf::Vector2f get_pos_ceil(const sf::Vector2f pos) {
    double s = get_base_scale() * scale;

    // Повторяем логику вычисления x и y из get_id_ceil
    float x = (pos.x - width / 2) / s + camx;
    float y = (height / 2 - pos.y) / s + camy + 1;

    return sf::Vector2f(x, y);
}
inline std::pair<sf::Vector2i, sf::Vector2i> get_view() {
    double s = get_base_scale() * scale;
    double radius_x = (width / 2.0) / s;
    double radius_y = (height / 2.0) / s;
    long long x_start = safeDoubleToLongLong(std::floor(camx - radius_x));
    long long x_end   = safeDoubleToLongLong(std::ceil(camx + radius_x));
    long long y_start = safeDoubleToLongLong(std::floor(camy - radius_y));
    long long y_end   = safeDoubleToLongLong(std::ceil(camy + radius_y));
    x_start = std::max((long long)0, x_start);
    x_start = std::min((long long)size, x_start);
    y_start = std::max((long long)0, y_start);
    y_start = std::min((long long)size, y_start);
    x_end   = std::min((long long)size - 1, x_end);
    x_end   = std::max((long long)-1, x_end);
    y_end   = std::min((long long)size - 1, y_end);
    y_end   = std::max((long long)-1, y_end);
    return {sf::Vector2i(x_start, y_start), sf::Vector2i(x_end, y_end)};
}
#ifndef CPU_MODE
inline auto resize_size_ssbo(auto ssbo, int new_size) {
    deleteSSBO(ssbo);
    ssbo = createEmptySSBO(new_size);
    return ssbo;
}
#endif
sf::Font defaultFont;
#ifndef ANDROID_MODE
std::string* keyToString(sf::Keyboard::Key keyCode) {
    if (keyCode >= sf::Keyboard::Num0 && keyCode <= sf::Keyboard::Num9) {
        return new std::string(1, static_cast<char>('0' + (keyCode - sf::Keyboard::Num0)));
    }
    return nullptr;
}
int getNumber(const std::string& title, const std::string& content, sf::RenderWindow& mainWindow) {
    sf::VideoMode vidioMode(300, 200);
    sf::RenderWindow input_window(vidioMode, title);
    input_window.setFramerateLimit(30);

    sf::Text contentText;
    contentText.setString(content);
    contentText.setCharacterSize(std::min(20, (int)(600/content.length())));
    contentText.setFont(defaultFont);
    contentText.setPosition(sf::Vector2f(10.f, 10.f));
    contentText.setFillColor(sf::Color::Black);
    sf::Text inputText;
    inputText.setString("Input: ");
    inputText.setCharacterSize(20);
    inputText.setPosition(sf::Vector2f(5, 40));
    inputText.setFont(defaultFont);
    inputText.setFillColor(sf::Color::Black);
    std::string input_string = "";
    sf::Event event;
    while (input_window.isOpen() && mainWindow.isOpen()) {
        while (input_window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                input_string = "";
                input_window.close();
            } else if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Enter) {
                    input_window.close();
                } else if (event.key.code == sf::Keyboard::BackSpace) {
                    if (input_string != "") {
                        input_string.pop_back();
                        inputText.setString("Input: "+input_string);
                    }
                } else if (event.key.code == sf::Keyboard::Subtract) {
                    input_string += "-";
                    inputText.setString("Input: "+input_string);
                }
                std::string* char_string = keyToString(event.key.code);
                if (char_string != nullptr) {
                    std::string string = *char_string;
                    input_string.append(string);
                    delete char_string;
                    inputText.setString("Input: "+input_string);
                }
            }

        }
        while (mainWindow.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                input_window.close();
                mainWindow.close();
            }
        }
        input_window.clear(sf::Color::White);
        input_window.draw(contentText);
        input_window.draw(inputText);
        input_window.display();
        //mainWindow.display();
    }
    mainWindow.setActive(true);
    if (input_string == "") {
        return -1;
    }
    try {
        return std::stoi(input_string);
    } catch (std::exception& _) {
        return -1;
    }
}
#else
int getNumber(const std::string& title, const std::string& content, SDL_Window* mainWindow) {
    // 1. ЗАХВАТ КАДРА (Front Buffer -> Texture)
    int size = std::min((int)(height/3), (int)(std::max(20, (int)(width/content.length()))*1.5));
    GLuint staticTex;
    glGenTextures(1, &staticTex);
    glBindTexture(GL_TEXTURE_2D, staticTex);

    // Используем твои глобальные width и height
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, width, height, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    std::string input_string = "";
    bool entering = true;
    SDL_Event event;

    SDL_StartTextInput(mainWindow);

    while (entering) {
        uint32_t frameStart = SDL_GetTicks();

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                glDeleteTextures(1, &staticTex);
                return -1;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_RETURN) entering = false;
                if (event.key.key == SDLK_BACKSPACE && !input_string.empty()) {
                    input_string.pop_back();
                }
            }
            if (event.type == SDL_EVENT_TEXT_INPUT) {
                char c = event.text.text[0]; // Берем первый символ из пришедшей строки
                if ((c >= '0' && c <= '9') || c == '-') {
                    input_string += c;
                }
            }
        }
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(graphicsProgramID);
        glBindTexture(GL_TEXTURE_2D, staticTex);
        glUniform1i(glGetUniformLocation(graphicsProgramID, "u_useTexture"), 1);

        // 2. Рисуем один Quad (Triangle Fan) на весь экран вручную
        float verts[] = {
                -1.0f,  1.0f,  0.0f, 0.0f, // x, y, u, v
                1.0f,  1.0f,  1.0f, 0.0f,
                1.0f, -1.0f,  1.0f, 1.0f,
                -1.0f, -1.0f,  0.0f, 1.0f
        };

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), verts);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), &verts[2]);
        glEnableVertexAttribArray(2);

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        sf::Text text;
        text.setFont(defaultFont);
        text.setString(content);
        text.setCharacterSize(size); // Или любой твой размер
        text.setPosition(sf::Vector2f(20, 20));
        text.render();
        std::string fullInputLine = "Input: " + input_string;
        sf::Text text2;
        text2.setFont(defaultFont);
        text2.setString(fullInputLine);
        text2.setCharacterSize(size); // Или любой твой размер
        text2.setPosition(sf::Vector2f(20, 40+size));
        text2.render();
        SDL_GL_SwapWindow(mainWindow);
        uint32_t frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < 33) {
            SDL_Delay(33 - frameTime);
        }
    }
    SDL_StopTextInput(mainWindow);
    glDeleteTextures(1, &staticTex);
    if (input_string.empty() || input_string == "-") return -1;
    try {
        return std::stoi(input_string);
    } catch (...) {
        return -1;
    }
}
#endif
#ifndef ANDROID_MODE
std::string getString(const std::string& title, const std::string& content, sf::RenderWindow& mainWindow) {
    sf::VideoMode vidioMode(300, 200);
    sf::RenderWindow input_window(vidioMode, title);
    input_window.setFramerateLimit(30);

    sf::Text contentText;
    contentText.setString(content);
    contentText.setCharacterSize(std::min(20, (int)(600/content.length())));
    contentText.setFont(defaultFont);
    contentText.setPosition(sf::Vector2f(10.f, 10.f));
    contentText.setFillColor(sf::Color::Black);
    sf::Text inputText;
    inputText.setString("Input: ");
    inputText.setCharacterSize(20);
    inputText.setPosition(sf::Vector2f(5, 40));
    inputText.setFont(defaultFont);
    inputText.setFillColor(sf::Color::Black);
    std::string input_string = "";
    sf::Event event;
    while (input_window.isOpen() && mainWindow.isOpen()) {
        while (input_window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                input_string = "";
                input_window.close();
            }
            else if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Enter) {
                    input_window.close();
                } else if (event.key.code == sf::Keyboard::BackSpace) {
                    if (!input_string.empty()) {
                        input_string.pop_back();
                    }
                }
            }
            else if (event.type == sf::Event::TextEntered) {
                uint32_t code = event.text.unicode;
                if (code >= 32 && code<256) {
                    input_string += (char)code;
                }
            }

            inputText.setString("Input: " + input_string);
        }
        while (mainWindow.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                input_window.close();
                mainWindow.close();
            }
        }
        input_window.clear(sf::Color::White);
        input_window.draw(contentText);
        input_window.draw(inputText);
        input_window.display();
        //mainWindow.display();
    }
    mainWindow.setActive(true);
    return input_string;
}
#else
std::string getString(const std::string& title, const std::string& content, SDL_Window* mainWindow) {
    // 1. ЗАХВАТ КАДРА (Front Buffer -> Texture)
    int size = std::min((int)(height/3), (int)(std::max(20, (int)(width/content.length()))*1.5));
    GLuint staticTex;
    glGenTextures(1, &staticTex);
    glBindTexture(GL_TEXTURE_2D, staticTex);

    // Используем твои глобальные width и height
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, width, height, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    std::string input_string = "";
    bool entering = true;
    SDL_Event event;

    SDL_StartTextInput(mainWindow);

    while (entering) {
        uint32_t frameStart = SDL_GetTicks();

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                glDeleteTextures(1, &staticTex);
                return "";
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_RETURN) entering = false;
                if (event.key.key == SDLK_BACKSPACE && !input_string.empty()) {
                    input_string.pop_back();
                }
            }
            if (event.type == SDL_EVENT_TEXT_INPUT) {
                char c = event.text.text[0]; // Берем первый символ из пришедшей строки
                input_string += c;
            }
        }
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(graphicsProgramID);
        glBindTexture(GL_TEXTURE_2D, staticTex);
        glUniform1i(glGetUniformLocation(graphicsProgramID, "u_useTexture"), 1);

        // 2. Рисуем один Quad (Triangle Fan) на весь экран вручную
        float verts[] = {
                -1.0f,  1.0f,  0.0f, 0.0f, // x, y, u, v
                1.0f,  1.0f,  1.0f, 0.0f,
                1.0f, -1.0f,  1.0f, 1.0f,
                -1.0f, -1.0f,  0.0f, 1.0f
        };

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), verts);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), &verts[2]);
        glEnableVertexAttribArray(2);

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        sf::Text text;
        text.setFont(defaultFont);
        text.setString(content);
        text.setCharacterSize(size); // Или любой твой размер
        text.setPosition(sf::Vector2f(20, 20));
        text.render();
        std::string fullInputLine = "Input: " + input_string;
        sf::Text text2;
        text2.setFont(defaultFont);
        text2.setString(fullInputLine);
        text2.setCharacterSize(size); // Или любой твой размер
        text2.setPosition(sf::Vector2f(20, 40+size));
        text2.render();
        SDL_GL_SwapWindow(mainWindow);
        uint32_t frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < 33) {
            SDL_Delay(33 - frameTime);
        }
    }
    SDL_StopTextInput(mainWindow);
    glDeleteTextures(1, &staticTex);
    return input_string;
}
#endif
#ifndef CPU_MODE
#ifdef ANDROID_MODE
const char* shader_code = R"(#version 310 es
precision highp float;
precision highp int;
#extension GL_ES_310_extensions : enable
layout(local_size_x = 16, local_size_y = 16) in;
// Тот самый буфер (binding = 0), который мы привязывали в C++
layout(std430, binding = 0) readonly buffer InBuffer {
    uint ceils_in[];
};
layout(std430, binding = 1) coherent buffer OutBuffer {
    uint ceils[];
};
)"
#else
const char* shader_code = R"(#version 430 core
#extension GL_ARB_compute_shader : enable
#extension GL_ARB_shader_storage_buffer_object : enable
layout(local_size_x = 16, local_size_y = 16) in;
// Тот самый буфер (binding = 0), который мы привязывали в C++
layout(std430, binding = 0) buffer InBuffer {
    uint ceils_in[];
};
layout(std430, binding = 1) buffer OutBuffer {
    uint ceils[];
};
)"
#endif
R"(
//uniform uint total;
uniform uint sizexy;
uint get_index(uint index) {
    return (ceils_in[index/32u]>>(index%32u)) & 1u;
}
void set_index(uint index, uint val) {
    uint bit_pos = index % 32u;
    uint array_idx = index / 32u;
    if (val == 1u) {
        atomicOr(ceils[array_idx], (1u << bit_pos));
    } else {
        atomicAnd(ceils[array_idx], ~(1u << bit_pos));
    }
}
void main() {
    uint x = gl_GlobalInvocationID.x;
    uint y = gl_GlobalInvocationID.y;
    if (x >= sizexy || y >= sizexy) return;
    uint id = x+y*sizexy;
    //uint x = id % sizexy;
    //uint y = id / sizexy;
    uint neighbors = 0u;
    neighbors += uint(x > 0u && get_index(y*sizexy + (x-1u)) == 1u);
    //if (x > 0u && get_index(y*sizexy + (x-1u)) == 1u) neighbors++; // лево
    neighbors += uint(x+1u < sizexy && get_index(y*sizexy + (x+1u)) == 1u);
    //if (x+1u < sizexy && get_index(y*sizexy + (x+1u)) == 1u) neighbors++; // право
    //if (y+1u < sizexy) {
    neighbors += uint(y+1u < sizexy && get_index((y+1u)*sizexy + x) == 1u);
    //    if (get_index((y+1u)*sizexy + x) == 1u) neighbors++; // низ
    neighbors += uint(y+1u < sizexy && x > 0u && get_index((y+1u)*sizexy + (x-1u)) == 1u);
    //    if (x > 0u && get_index((y+1u)*sizexy + (x-1u)) == 1u) neighbors++; // низ-лево
    neighbors += uint(y+1u < sizexy && x+1u < sizexy && get_index((y+1u)*sizexy + (x+1u)) == 1u);
    //    if (x+1u < sizexy && get_index((y+1u)*sizexy + (x+1u)) == 1u) neighbors++; // низ-право
    //}

    //if (y > 0u) {
    neighbors += uint(y > 0u && get_index((y-1u)*sizexy + x) == 1u);
    //    if (get_index((y-1u)*sizexy + x) == 1u) neighbors++; // верх
    neighbors += uint(y > 0u && x > 0u && get_index((y-1u)*sizexy + (x-1u)) == 1u);
    //    if (x > 0u && get_index((y-1u)*sizexy + (x-1u)) == 1u) neighbors++; // верх-лево
    neighbors += uint(y > 0u && x+1u < sizexy && get_index((y-1u)*sizexy + (x+1u)) == 1u);
    //    if (x+1u < sizexy && get_index((y-1u)*sizexy + (x+1u)) == 1u) neighbors++; // верх-право
    //}
    uint current = get_index(id);
    //uint result = 0u;
    uint result = uint(neighbors == 3u || (current == 1u && neighbors == 2u));
    //if (current == 1u) {
    //    if (neighbors == 2u || neighbors == 3u) result = 1u; // Выжил
    //} else {
    //    if (neighbors == 3u) result = 1u; // Родился
    //}
    set_index(id, result);
}
)"
;
#endif
inline bool get_index(std::vector<uint32_t>& ceils, uint32_t index) {
    return (ceils[index/32]>>(index%32)) & 1;
}
inline void set_index(std::vector<uint32_t>& ceils, uint32_t index, bool val) {
    uint32_t bit_pos = index % 32;
    uint32_t array_idx = index / 32;
    if (val) {
        ceils[array_idx] |= (1u << bit_pos);
    } else {
        ceils[array_idx] &= ~(1u << bit_pos);
    }
}
#ifdef ANDROID_MODE
SDL_Rect safeArea;
#else
struct rect_struct {
    int x = 0; int y = 0;
    int w = 800; int h = 600;
};
rect_struct safeArea = {0, 0, 800, 600};
#endif
sf::ConvexShape createRoundedRect(float width, float height, float radius) {
    sf::ConvexShape shape;
    unsigned int quality = 10; // Сколько точек на угол
    shape.setPointCount(quality * 4);

    for (int i = 0; i < 4; i++) {
        float centerX = (i == 0 || i == 3) ? radius : width - radius;
        float centerY = (i == 0 || i == 1) ? radius : height - radius;
        for (int j = 0; j < quality; j++) {
            float angle = (i + (float)j / (quality - 1)) * 3.14159f / 2.0f + 3.14159f;
            shape.setPoint(i * quality + j, sf::Vector2f(centerX + radius * cos(angle), centerY + radius * sin(angle)));
        }
    }
    return shape;
}
sf::Text createTextForButton(std::string str, sf::Vector2f pos, float width, float height, float radius) {
    width -= radius;
    width -= radius;
    height -= radius;
    height -= radius;
    int size_char = width/str.length()*1.5;
    if (size_char>height) {
        size_char = height;
    }
    sf::Text text;
    text.setFont(defaultFont);
    text.setCharacterSize(size_char);
    text.setString(str);
    text.setPosition(sf::Vector2f(pos.x+radius+(width-size_char/1.5*str.length())*0.5, pos.y+radius+(height-size_char)*0.5));
    // text.setFillColor(sf::Color(83, 243, 23));
    return text;
}
bool is_click_button(sf::Vector2f pos, sf::Vector2f size, sf::Vector2f click) {
    return pos.x<=click.x && click.x<=pos.x+size.x && pos.y<=click.y && click.y<=pos.y+size.y;
}

class Slider {
private:
    sf::Vector2f pos;
    sf::Vector2f size;
    float degree = 1.5;
    int max_val;
    sf::Color line_color;
    sf::Color circle_color;
public:
    Slider(int max_val) : max_val(max_val) {}

    void setLineColor(sf::Color col) {
        line_color = col;
    }

    void setCircleColor(sf::Color col) {
        circle_color = col;
    }

    void setPosition(sf::Vector2f pos) {
        this->pos = pos;
    }

    void setSize(sf::Vector2f size) {
        this->size = size;
    }

#ifdef ANDROID_MODE

    void render(int value) {
#else
        void render(sf::RenderWindow& window, int value) {
#endif
        sf::Vector2f pos_circle = sf::Vector2f(
                (value < max_val ? std::pow(value, 1 / degree) / std::pow(max_val, 1 / degree) *
                                   size.x : size.x), 0) + pos;
        float size_circle = size.y * 2;
        auto line = createRoundedRect(size.x, size.y, size.y * 0.5);
        line.setPosition(pos);
        line.setFillColor(line_color);
#ifdef ANDROID_MODE
        line.render();
#else
        window.draw(line);
#endif
#ifdef ANDROID_MODE
        sf::CircleShape circle;
#else
        sf::CircleShape circle(size_circle/2);
#endif
        circle.setPosition(pos_circle - sf::Vector2f(size_circle * 0.5, size_circle * 0.25));
#ifdef ANDROID_MODE
        circle.setSize(sf::Vector2f(size_circle, size_circle));
#endif
        circle.setFillColor(circle_color);
#ifdef ANDROID_MODE
        circle.render();
#else
        window.draw(circle);
#endif
    }

#ifdef ANDROID_MODE
    std::optional<int> event_to(int count, SDL_Finger **fingers) {
        if (count == 1) {
            if (is_click_button(pos - sf::Vector2f(0, size.y * 0.5), size + sf::Vector2f(0, size.y),
                                sf::Vector2f(fingers[0]->x * width, fingers[0]->y * height))) {
                return std::pow((fingers[0]->x * width - pos.x) / size.x, degree) * max_val;
            }
        }
        return std::nullopt;
    }

#else
    std::optional<int> event_to(sf::Event event) {
        if (event.type==sf::Event::MouseMoved && sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
            if (is_click_button(pos-sf::Vector2f(0, size.y*0.5), size+sf::Vector2f(0, size.y), sf::Vector2f(event.mouseMove.x, event.mouseMove.y))) {
                return std::pow((event.mouseMove.x - pos.x) / size.x, degree) * max_val;
            }
        }
        return std::nullopt;
    }
#endif
};
// часть сохранений:
void write_in_file(const std::string& filename, const std::vector<uint8_t>& data) {
#ifdef ANDROID_MODE
    // --- ВЕТКА ДЛЯ ANDROID (SDL3) ---
    // Получаем безопасный путь для записи данных приложения
    char* pref_path = SDL_GetPrefPath("example", "gameoflive");
    if (!pref_path) return;

    std::string full_path = std::string(pref_path) + filename;
    SDL_free(pref_path);

    SDL_IOStream* io = SDL_IOFromFile(full_path.c_str(), "wb");
    if (io) {
        SDL_WriteIO(io, data.data(), data.size());
        SDL_CloseIO(io);
        SDL_Log("Android: Saved %zu bytes to %s", data.size(), full_path.c_str());
    }
#else
    // --- ВЕТКА ДЛЯ PC (Windows/Linux) ---
    std::ofstream file(filename, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        std::cout << "PC: Saved " << data.size() << " bytes to " << filename << std::endl;
    }
#endif
}
std::vector<uint8_t> read_from_file(const std::string& filename) {
#ifdef ANDROID_MODE
    // --- ВЕТКА ДЛЯ ANDROID (SDL3) ---
    char* pref_path = SDL_GetPrefPath("example", "gameoflive");
    if (!pref_path) return {};

    std::string full_path = std::string(pref_path) + filename;
    SDL_free(pref_path);

    SDL_IOStream* io = SDL_IOFromFile(full_path.c_str(), "rb");
    if (!io) return {};

    Sint64 size = SDL_GetIOSize(io);
    if (size <= 0) {
        SDL_CloseIO(io);
        return {};
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    SDL_ReadIO(io, buffer.data(), buffer.size());
    SDL_CloseIO(io);
    return buffer;
#else
    // --- ВЕТКА ДЛЯ PC (Windows/Linux) ---
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return buffer;
    }
    return {};
#endif
}
bool delete_file(const std::string& filename) {
#ifdef ANDROID_MODE
    // --- ВЕТКА ДЛЯ ANDROID (SDL3) ---
    char* pref_path = SDL_GetPrefPath("example", "gameoflive");
    if (!pref_path) return false;

    std::string full_path = std::string(pref_path) + filename;
    SDL_free(pref_path);

    if (SDL_RemovePath(full_path.c_str()) == 0) {
        return true;
    }
    return false;
#else
    // --- ВЕТКА ДЛЯ PC (Windows/Linux) ---
    if (std::remove(filename.c_str()) == 0) {
        return true;
    }
    return false;
#endif
}
namespace fs = std::filesystem;
std::string get_base_dir() {
#ifdef ANDROID_MODE
    char* pref_path = SDL_GetPrefPath("example", "gameoflive");
    if (pref_path) {
        std::string path = pref_path;
        SDL_free(pref_path);
        return path;
    }
    return "";
#else
    return "./"; // Текущая директория для PC
#endif
}
std::vector<std::string> get_all_files() {
    std::vector<std::string> file_list;
    std::string path = get_base_dir();

    try {
        if (path.empty() || !fs::exists(path)) {
            return file_list;
        }

        // Перебираем элементы в директории
        for (const auto& entry : fs::directory_iterator(path)) {
            // Добавляем в список только обычные файлы (не папки)
            if (entry.is_regular_file()) {
                file_list.push_back(entry.path().filename().string());
            }
        }
    } catch (const fs::filesystem_error& e) {
#ifdef ANDROID_MODE
        SDL_Log("FS Error: %s", e.what());
#else
        std::cerr << "FS Error: " << e.what() << std::endl;
#endif
    }

    return file_list;
}
struct save_struct {
    uint32_t sizex;
    uint32_t sizey;
    std::vector<uint32_t> data;
    void load_from_part(std::vector<bool>& ceils, uint32_t x, uint32_t y, int sx, int sy) {
        data.resize((abs((long long)sx*(long long)sy)+31)/32);
        if (sx<0) {
            x-=-sx;
            sx = abs(sx);
        }
        if (sy<0) {
            y-=-sy;
            sy = abs(sy);
        }
        sizex = sx;
        sizey = sy;
        std::cout << "sx:" << sx << "sy:" << sy << std::endl;
        std::cout << "x:" << x << "y:" << y << std::endl;
        std::cout << "sizex:" << sizex << "sizey:" << sizey << std::endl;
        for (uint32_t iy = y; iy!=y+sy; iy++) {
            for (uint32_t ix = x; ix!=x+sx; ix++) {
                uint64_t id = iy*size+ix;
                uint64_t id_in = (iy-y)*sx+ix-x;
                set_index(data, id_in, ceils[id]);
            }
        }
    }
    void load_from_part(std::vector<uint32_t>& ceils, uint32_t x, uint32_t y, int sx, int sy) {
        data.resize((abs(sx*sy)+31)/32);
        if (sx<0) {
            x-=-sx;
            sx = abs(sx);
        }
        if (sy<0) {
            y-=-sy;
            sy = abs(sy);
        }
        sizex = sx;
        sizey = sy;
        std::cout << "sx:" << sx << "sy:" << sy << std::endl;
        std::cout << "x:" << x << "y:" << y << std::endl;
        std::cout << "sizex:" << sizex << "sizey:" << sizey << std::endl;
        for (uint32_t iy = y; iy!=y+sy; iy++) {
            for (uint32_t ix = x; ix!=x+sx; ix++) {
                uint64_t id = iy*size+ix;
                uint64_t id_in = (iy-y)*sx+ix-x;
                set_index(data, id_in, get_index(ceils, id));
            }
        }
    }
    void import_in_part(std::vector<bool>& ceils, uint32_t x, uint32_t y) {
        for (uint32_t ix = x; ix!=x+sizex; ix++) {
            for (uint32_t iy = y; iy!=y+sizey; iy++) {
                uint32_t id = iy*size+ix;
                uint64_t id_in = (iy-y)*sizex+ix-x;
                ceils[id] = get_index(data, id_in);
            }
        }
    }
    void import_in_part(std::vector<uint32_t>& ceils, uint32_t x, uint32_t y) {
        for (uint32_t ix = x; ix!=x+sizex; ix++) {
            for (uint32_t iy = y; iy!=y+sizey; iy++) {
                uint32_t id = iy*size+ix;
                uint64_t id_in = (iy-y)*sizex+ix-x;
                set_index(ceils, id, get_index(data, id_in));
            }
        }
    }
    void load_from_name(std::string name) {
        std::vector<uint8_t> buff = read_from_file(name);
        Ser::load_from_buffer(*this, buff);
    }
    void save(std::string name) {
        std::vector<uint8_t> buff = Ser::to_buffer(*this);
        write_in_file(name, buff);
    }
#ifndef ANDROID_MODE
    void draw(sf::RenderWindow& window, float x, float y, float sx, float sy) {
#else
    void draw(SDL_Window* window, float x, float y, float sx, float sy) {
#endif
        sf::RectangleShape all;
        all.setFillColor(sf::Color(50, 50, 50));
        all.setSize(sf::Vector2f(sx, sy));
        all.setPosition(sf::Vector2f(x, y));
#ifdef ANDROID_MODE
        all.render();
#else
        window.draw(all);
#endif
        float scale_ceil = std::min(sx/sizex, sy/sizey);
        float paddx = abs(sx-scale_ceil*sizex)/2;
        float paddy = abs(sy-scale_ceil*sizey)/2;
        x += paddx;
        y += paddy;
        sf::RectangleShape rect;
        rect.setFillColor(sf::Color(100, 100, 100));
        rect.setPosition(sf::Vector2f(x, y));
        rect.setSize(sf::Vector2f(scale_ceil*sizex, scale_ceil*sizey));
#ifdef ANDROID_MODE
        rect.render();
#else
        window.draw(rect);
#endif
        sf::VertexArray vex(sf::Quads);
        vex.resize(sizex*sizey*4);
        float padd_start = scale_ceil*0.05;
        float padd_end = scale_ceil*0.95;
#pragma omp parallel for
        for (uint32_t iy = 0; iy!=sizey; iy++) {
            for (uint32_t ix = 0; ix!=sizex; ix++) {
                uint64_t id = (sizey-iy-1)*sizex+ix;
                bool ceil = get_index(data, id);
                sf::Color col = (ceil? sf::Color::Black: sf::Color::White);
                uint64_t id_vex = id*4;
                vex[id_vex].position.x = x+ix*scale_ceil+padd_start;
                vex[id_vex].position.y = y+iy*scale_ceil+padd_start;
                vex[id_vex].color = col;
                id_vex++;
                vex[id_vex].position.x = x+ix*scale_ceil+padd_end;
                vex[id_vex].position.y = y+iy*scale_ceil+padd_start;
                vex[id_vex].color = col;
                id_vex++;
                vex[id_vex].position.x = x+ix*scale_ceil+padd_end;
                vex[id_vex].position.y = y+iy*scale_ceil+padd_end;
                vex[id_vex].color = col;
                id_vex++;
                vex[id_vex].position.x = x+ix*scale_ceil+padd_start;
                vex[id_vex].position.y = y+iy*scale_ceil+padd_end;
                vex[id_vex].color = col;
            }
        }
#ifdef ANDROID_MODE
        vex.render();
#else
        window.draw(vex);
#endif
    }
};
std::string format_save_name(std::string name) {
    // 1. Убираем расширение .gol (последние 4 символа)
    if (name.size() > 4 && name.substr(name.size() - 4) == ".gol") {
        name.erase(name.size() - 4);
    }

    // 2. Заменяем все '_' на ' '
    std::replace(name.begin(), name.end(), '_', ' ');

    return name;
}
std::string format_no_save_name(std::string name) {
    std::replace(name.begin(), name.end(), ' ', '_');
    return name+".gol";
}
sf::Texture delete_icon;
#ifdef ANDROID_MODE
std::optional<save_struct> show_saves(SDL_Window* window) {
#else
    std::optional<save_struct> show_saves(sf::RenderWindow& window) {
#endif
    std::vector<std::pair<std::string, save_struct>> saves;
    std::vector<std::string> files = get_all_files();
    for (const auto& name: files) {
        if (name.ends_with(".gol")) {
            save_struct save_file;
            save_file.load_from_name(name);
            saves.emplace_back(format_save_name(name), std::move(save_file));
        }
    }
    float scroll = 0;
#ifdef ANDROID_MODE
    bool runing = true;
    while (runing) {
#else
        while (window.isOpen()) {
#endif
        float sizex = std::min(width, height)*0.9;
        float sizey = sizex*1.1;
        float paddx = (width-sizex)/2;
        float paddy = paddx;
        float step = (paddy + paddx * 0.5f + sizey);
        float total_content_height = step * saves.size() + paddx * 0.5f+height*0.1;
        float scroll_max = std::max(0.0f, total_content_height - height);

        sf::Vector2f pos_button_exit = sf::Vector2f(width*0.1, height*0.9);
        sf::Vector2f size_button_exit = sf::Vector2f(width*0.8, height*0.1);
        float round_button_exit = std::min(width, height)*0.02;

#ifndef ANDROID_MODE
        sf::Event event;
        while (window.pollEvent(event)) {
#else
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
#endif
#ifndef ANDROID_MODE
            if (event.type == sf::Event::Closed)
                window.close();
#else
            if (event.type == SDL_EVENT_QUIT) runing = false;
#endif
#ifdef ANDROID_MODE
            else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                float scroll_percent = (scroll_max > 0) ? (scroll / -scroll_max) : 0.0f;
                width = event.window.data1;
                height = event.window.data2;
                glViewport(0, 0, width, height);
                SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(window), &safeArea);
                glUseProgram(graphicsProgramID); // ID твоей скомпилированной программы
                GLint loc = glGetUniformLocation(graphicsProgramID, "u_res");
                if (loc != -1) {
                    glUniform2f(loc, 1/(float)width, 1/(float)height);
                }
                SDL_Log("width: %d", width);
                SDL_Log("height: %d", height);
                SDL_Log("safeArea.x: %d", safeArea.x);
                SDL_Log("safeArea.y: %d", safeArea.y);
                SDL_Log("safeArea.w: %d", safeArea.w);
                SDL_Log("safeArea.h: %d", safeArea.h);
                if (safeArea.w>width) {
                    safeArea.w = width-safeArea.x;
                }
                if (safeArea.h>height) {
                    safeArea.h = height-safeArea.y;
                }

                float step = paddy + paddx * 0.5f + sizey;
                float total_height = step * saves.size() + paddy;
                scroll_max = std::max(0.0f, total_height - (float)height);
                scroll = -scroll_percent * scroll_max;
                scroll = std::max(-scroll_max, std::min(0.0f, scroll));
            }
#else
                else if (event.type==sf::Event::Resized) {
                float scroll_percent = (scroll_max > 0) ? (scroll / -scroll_max) : 0.0f;
                sf::FloatRect visibleArea(0.f, 0.f, event.size.width, event.size.height);
                width = event.size.width;
                height = event.size.height;
                safeArea.w = width;
                safeArea.h = height;
                window.setView(sf::View(visibleArea));

                float step = paddy + paddx * 0.5f + sizey;
                float total_height = step * saves.size() + paddy;
                scroll_max = std::max(0.0f, total_height - (float)height);
                scroll = -scroll_percent * scroll_max;
                scroll = std::max(-scroll_max, std::min(0.0f, scroll));
            }
#endif
#ifndef ANDROID_MODE
                else if (event.type==sf::Event::MouseWheelScrolled && event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel) {
                scroll+=event.mouseWheelScroll.delta*50;
                scroll = std::max(-scroll_max, std::min(0.0f, scroll));
            }
#else
            else if (event.type == SDL_EVENT_FINGER_MOTION) {
                //SDL_Log("move");
                int count = 0;
                SDL_Finger** fingers = SDL_GetTouchFingers(event.tfinger.touchID, &count);
                if (count==1) {
                    scroll+=event.tfinger.dx*width;
                    scroll = std::max(-scroll_max, std::min(0.0f, scroll));
                }
            }
#endif
#ifndef ANDROID_MODE
                else if (event.type==sf::Event::MouseButtonPressed) {
                const sf::Vector2f click_pos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
#else
            else if (event.type == SDL_EVENT_FINGER_DOWN) {
                const sf::Vector2f click_pos = sf::Vector2f(event.tfinger.x * width, event.tfinger.y * height);
#endif
                if (is_click_button(pos_button_exit, size_button_exit, click_pos)) {
                    return std::nullopt;
                }
                uint32_t i = 0;
                for (auto& [name, save]: saves) {
                    float x = paddx;
                    float y = (paddy+x*0.5+sizey)*i+scroll+x*0.5;
                    float delete_button_size = sizex*0.08;
                    sf::Vector2f delete_button_bg_pos = sf::Vector2f(x+sizex*0.98-delete_button_size, y+sizex*1.01);
                    if (is_click_button(delete_button_bg_pos, sf::Vector2f(delete_button_size, delete_button_size), click_pos)) { // delete button
                        delete_file(format_no_save_name(name));
                        saves.erase(saves.begin() + i);
                        break;
                    }
                    if (is_click_button(sf::Vector2f(x, y), sf::Vector2f(sizex, sizey), click_pos)) {
                        return save;
                    }
                    i++;
                }
            }
        }
#ifdef ANDROID_MODE
        clearWindow(sf::Color(50, 50, 50));
#else
        window.clear(sf::Color(50, 50, 50));
#endif
        uint32_t i = 0;
        sf::RectangleShape rect_out;
        rect_out.setSize(sf::Vector2f(sizex+paddx, sizey+paddx));
        rect_out.setFillColor(sf::Color::Black);
        sf::RectangleShape rect_in;
        rect_in.setSize(sf::Vector2f(sizex+paddx*0.5, sizey+paddx*0.5));
        rect_in.setFillColor(sf::Color(200, 200, 200));
        sf::Text text;
        text.setFont(defaultFont);
        text.setFillColor(sf::Color(20, 20, 20));
        text.setCharacterSize(sizex*0.08);
        for (auto& [name, save]: saves) {
            float x = paddx;
            float y = (paddy+x*0.5+sizey)*i+scroll+x*0.5;
            rect_out.setPosition(sf::Vector2f(x*0.5, y-x*0.5));
            rect_in.setPosition(sf::Vector2f(x*0.75, y-x*0.25));
#ifdef ANDROID_MODE
            rect_out.render();
            rect_in.render();
#else
            window.draw(rect_out);
            window.draw(rect_in);
#endif
            save.draw(window, x, y, sizex, sizex);
            text.setPosition(sf::Vector2f(x+sizex*0.02, y+sizex*1.01));
            text.setString(name);
#ifdef ANDROID_MODE
            text.render();
#else
            window.draw(text);
#endif
            float delete_button_size = sizex*0.08;
#ifndef ANDROID_MODE
            sf::CircleShape delete_button_bg(delete_button_size*0.5f);
#else
            sf::CircleShape delete_button_bg;
            delete_button_bg.setSize(sf::Vector2f(delete_button_size, delete_button_size));
#endif
            sf::Vector2f delete_button_bg_pos = sf::Vector2f(x+sizex*0.98f-delete_button_size, y+sizex*1.01f);
            delete_button_bg.setPosition(delete_button_bg_pos);
            delete_button_bg.setFillColor(sf::Color(180, 50, 50));
            sf::RectangleShape delete_icon_img;
#ifndef ANDROID_MODE
            delete_icon_img.setTexture(&delete_icon);
#endif
            float delete_icon_img_size = delete_button_size/std::sqrt(2);
            delete_icon_img.setSize(sf::Vector2f(delete_icon_img_size, delete_icon_img_size));
            float add_pos_img = (delete_button_size-delete_icon_img_size)*0.5f;
            delete_icon_img.setPosition(delete_button_bg_pos+sf::Vector2f(add_pos_img, add_pos_img));
            delete_icon_img.setFillColor(sf::Color::White);
#ifdef ANDROID_MODE
            delete_icon_img.setTexture();
            delete_button_bg.render();
            delete_icon.use();
            delete_icon_img.setTransparency();
            delete_icon_img.render();
            delete_icon_img.setUnTransparency();
            delete_icon.unuse();
#else
            window.draw(delete_button_bg);
            window.draw(delete_icon_img);
#endif
            i++;
        }
        // button exit
        auto button_exit = createRoundedRect(size_button_exit.x, size_button_exit.y, round_button_exit);
        button_exit.setPosition(pos_button_exit);
        button_exit.setFillColor(sf::Color(180, 50, 50));
        auto button_text_exit = createTextForButton("exit", pos_button_exit, size_button_exit.x, size_button_exit.y, round_button_exit);
        button_text_exit.setFillColor(sf::Color(50, 10, 10));
#ifdef ANDROID_MODE
        button_exit.render();
        button_text_exit.render();
#else
        window.draw(button_exit);
        window.draw(button_text_exit);
#endif
#ifdef ANDROID_MODE
        SDL_GL_SwapWindow(window);
        SDL_Delay(16);
#else
        window.display();
#endif
    }
#ifdef ANDROID_MODE
    SDL_Event quit_event;
    quit_event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&quit_event);
#else
    window.close();
#endif
    return std::nullopt;
}
int sps = 5;
#ifndef ANDROID_MODE
int main() {
#else
int main(int argc, char* argv[]) {
#endif
#ifndef ANDROID_MODE
    defaultFont.loadFromFile("C:/Windows/Fonts/arial.ttf");
#endif
    bool is_all_buttons = false;
    uint8_t is_select = 0; //0 - не выделяется. 1 - ожидание выделения. 2 - ожидание конца выделения. 3 - выделено
    uint32_t select_x = 0;
    uint32_t select_y = 0;
    int select_sx = 0;
    int select_sy = 0;
    bool is_paste = false;
    std::optional<save_struct> paste_struct = std::nullopt;
#ifndef ANDROID_MODE
    sf::RenderWindow window(sf::VideoMode(800, 600), "Game Of Live (FPS=30)");
    window.setFramerateLimit(30);
    safeArea.w = width;
    safeArea.h = height;
    sf::Vector2f lastMousePos = sf::Vector2f(-1, -1);
#else
    //SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight Portrait PortraitUpsideDown");

    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    //SDL_Window *window = SDL_CreateWindow("Game of Live", 0, 0,SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_RESIZABLE);
    SDL_Window *window = SDL_CreateWindow("Game of Live", 0, 0,SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(0);
    SDL_GetWindowSizeInPixels(window, &width, &height);
    SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(window), &safeArea);
    startAndroidSFML();
    sf::Clock clock_start;
    {
        glUseProgram(graphicsProgramID);
        sf::Texture texture;
        texture.load("loading.png");
        sf::RectangleShape rect;
        rect.setFillColor(sf::Color::White);
        if (width<height) {
            rect.setPosition(sf::Vector2f(0.1 * width, height / 2 - 0.4 * width));
            rect.setSize(sf::Vector2f(0.8 * width, 0.8*width));
        } else {
            rect.setPosition(sf::Vector2f(width/2-height*0.4, height*0.1));
            rect.setSize(sf::Vector2f(0.8 * height, 0.8*height));
        }
        rect.setTexture();
        rect.setTransparency();
        texture.use();
        for(int i = 0; i < 3; i++) {
            clearWindow(sf::Color(50, 50, 50));
            rect.render();
            SDL_GL_SwapWindow(window);
            SDL_Delay(16);
        }
        rect.setUnTransparency();
        texture.unuse();
    }
    SDL_Log("time first screen: %d ms", (int)clock_start.restart().get_direction());
    float lastDist = 0;
    float lastDelay = 0;
    if (!defaultFont.loadFromFile("arial.ttf")) {
        SDL_Log("file \"arial.ttf\" not found");
    }
    SDL_Log("time load font: %d ms", (int)clock_start.restart().get_direction());
#endif
    bool active = true;
    bool is_stop = false;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 100);
    sf::Texture save_icon;
    if (!save_icon.loadFromFile("save.png")) {
        print("error load \"save.png\"");
    }
    sf::Texture select_icon;
    if (!select_icon.loadFromFile("select.png")) {
        print("error load \"select.png\"");
    }
    sf::Texture build_icon;
    if (!build_icon.loadFromFile("build_icon.png")) {
        print("error load \"build_icon.png\"");
    }
    if (!delete_icon.loadFromFile("delete_icon.png")) {
        print("error load \"delete_icon.png\"");
    }
#ifdef CPU_MODE
#ifdef ANDROID_MODE
    loadGPUFunctions();
#endif
#endif
#ifndef CPU_MODE
    loadGPUFunctions();
#define SSBO_IN (active ? ssboIn: ssboOut)
#define SSBO_OUT (active ? ssboOut: ssboIn)
    auto ssboIn = createEmptySSBO(calc_size_ssbo());
    auto ssboOut = createEmptySSBO(calc_size_ssbo());
    auto shader = compileShader(shader_code);
    std::vector<uint32_t> ceils((size*size + 31) / 32);
    for (int i = 0; i!=size*size; i++) {
        set_index(ceils, i, dis(gen)<=37);
    }
    updateSSBOData(ssboIn, ceils);
#endif
#ifdef CPU_MODE
#define WRITE_CEILS (active? ceils: ceils2)
#define READ_CEILS (active? ceils2: ceils)
    std::vector<bool> ceils(size*size);
    std::vector<bool> ceils2(size*size);
    for (int i = 0; i!=ceils.size(); i++) {
        READ_CEILS[i] = dis(gen)<=37;
    }
#endif
    sf::Clock clock;
    sf::Clock clock_simulate;
    double last_simulate = 0;
    std::vector<std::pair<sf::Vector2f, bool>> rects;
    sf::VertexArray vex(sf::Quads);
    Slider slider_sps(100);
    slider_sps.setPosition(sf::Vector2f(0, 0));
    slider_sps.setSize(sf::Vector2f(0, 0));
    slider_sps.setLineColor(sf::Color(50, 100, 200));
    slider_sps.setCircleColor(sf::Color(50, 200, 50));
#ifdef ANDROID_MODE
    bool runing = true;
    while (runing) {
#else
        while (window.isOpen()) {
#endif
#ifndef ANDROID_MODE
        sf::Event event;
        while (window.pollEvent(event)) {
            std::optional<int> new_sps = slider_sps.event_to(event);
            if (new_sps) {
                sps = *new_sps;
                continue;
            }
#else
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
#endif
#ifndef ANDROID_MODE
            if (event.type == sf::Event::Closed)
                window.close();
#else
            if (event.type == SDL_EVENT_QUIT) runing = false;
#endif
#ifdef ANDROID_MODE
            else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                width = event.window.data1;
                height = event.window.data2;
                glViewport(0, 0, width, height);
                SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(window), &safeArea);
                glUseProgram(graphicsProgramID); // ID твоей скомпилированной программы
                GLint loc = glGetUniformLocation(graphicsProgramID, "u_res");
                if (loc != -1) {
                    glUniform2f(loc, 1/(float)width, 1/(float)height);
                }
                SDL_Log("width: %d", width);
                SDL_Log("height: %d", height);
                SDL_Log("safeArea.x: %d", safeArea.x);
                SDL_Log("safeArea.y: %d", safeArea.y);
                SDL_Log("safeArea.w: %d", safeArea.w);
                SDL_Log("safeArea.h: %d", safeArea.h);
                if (safeArea.w>width) {
                    safeArea.w = width-safeArea.x;
                }
                if (safeArea.h>height) {
                    safeArea.h = height-safeArea.y;
                }
            }
#else
                else if (event.type==sf::Event::Resized) {
                sf::FloatRect visibleArea(0.f, 0.f, event.size.width, event.size.height);
                width = event.size.width;
                height = event.size.height;
                safeArea.w = width;
                safeArea.h = height;
                window.setView(sf::View(visibleArea));
                clock.restart();
                clock_simulate.restart();
            }
#endif
#ifndef ANDROID_MODE
                else if (event.type==sf::Event::MouseButtonReleased) {
                if (is_select==2) {
                    is_select = 3;
                }
            } else if (event.type==sf::Event::MouseButtonPressed) {
                const sf::Vector2f click_pos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
#else
            else if (event.type == SDL_EVENT_FINGER_UP) {
                lastDist = 0;
                if (is_select==2) {
                    is_select = 3;
                }
            } else if (event.type == SDL_EVENT_FINGER_DOWN) {
                const sf::Vector2f click_pos = sf::Vector2f(event.tfinger.x * width, event.tfinger.y * height);
#endif
//#ifdef ANDROID_MODE
                float buttonsWidth = safeArea.w*0.5;
                float buttonsHeight = safeArea.h*0.1;
                float size_button_all = std::min(safeArea.w, safeArea.h)*0.1;
                sf::Vector2f pos_rect_random = sf::Vector2f(safeArea.x, safeArea.h*0.9+safeArea.y);
                sf::Vector2f pos_rect_run_stop = sf::Vector2f(safeArea.x+safeArea.w*0.5, safeArea.h*0.9+safeArea.y);
#ifdef ANDROID_MODE
                sf::Vector2f pos_rect_all = sf::Vector2f(safeArea.w-size_button_all*1.1+safeArea.x, size_button_all*0.1+safeArea.y+safeArea.h*0.05);
#else
                sf::Vector2f pos_rect_all = sf::Vector2f(safeArea.w-size_button_all*1.1+safeArea.x, size_button_all*0.1+safeArea.y);
#endif
                sf::Vector2f pos_select_button = pos_rect_all-sf::Vector2f(size_button_all*1.1, 0);
                sf::Vector2f pos_save_button = pos_select_button-sf::Vector2f(size_button_all*1.1, 0);
                sf::Vector2f pos_rect_resize = sf::Vector2f(safeArea.x + safeArea.w * 0.25,safeArea.h * 0.1 + safeArea.y);
                sf::Vector2f pos_rect_clear = pos_rect_resize+sf::Vector2f(0, safeArea.h*0.12);
                sf::Vector2f pos_rect_change_sps = pos_rect_clear+sf::Vector2f(0, safeArea.h*0.12);
                sf::Vector2f pos_rect_saves = pos_rect_change_sps+sf::Vector2f(0, safeArea.h*0.12);
                if (is_click_button(pos_rect_random, sf::Vector2f(buttonsWidth, buttonsHeight), click_pos)) { // random button
                    for (int i = 0; i!=size*size; i++) {
#ifdef CPU_MODE
                        READ_CEILS[i] = dis(gen)<=37;
#endif
#ifndef CPU_MODE
                        // ceils[i] = dis(gen);
                        set_index(ceils, i, dis(gen)<=37);
#endif
                    }
#ifndef CPU_MODE
                    updateSSBOData(SSBO_IN, ceils);
#endif
                    clock_simulate.restart();
                    clock.restart();
                } else if (is_click_button(pos_rect_all, sf::Vector2f(size_button_all, size_button_all), click_pos)) { // all button
                    is_all_buttons = !is_all_buttons;
                    is_stop = true;
                } else if (is_select==3 && is_click_button(pos_save_button, sf::Vector2f(size_button_all, size_button_all), click_pos)) { // save button
                    std::string name = getString("input", "input name save", window);
                    if (!name.empty()) {
                        save_struct save_dat;
#ifdef CPU_MODE
                        save_dat.load_from_part(READ_CEILS, select_x, select_y, select_sx, select_sy);
#else
                        save_dat.load_from_part(ceils, select_x, select_y, select_sx, select_sy);
#endif
                        save_dat.save(format_no_save_name(name));
                    }
                } else if (!is_paste && is_click_button(pos_select_button, sf::Vector2f(size_button_all, size_button_all), click_pos)) { // select button
                    if (is_select==0) {
                        is_select = 1;
                    } else {
                        is_select = 0;
                    }
                } else if (is_paste && is_click_button(pos_select_button, sf::Vector2f(size_button_all, size_button_all), click_pos)) { // select button
                    save_struct& sp = *paste_struct;
                    int x_paste = camx-sp.sizex/2;
                    int y_paste = camy-sp.sizey/2;
                    for (uint32_t y = std::min((int)sp.sizey, std::max(0, -(int)y_paste)); y!=std::max(std::min((int)sp.sizey, (int)size - y_paste), 0); y++) {
                        for (uint32_t x = std::min((int)sp.sizex, std::max(0, -(int)x_paste)); x!=std::max(std::min((int)sp.sizex, (int)size - x_paste), 0); x++) {
                            uint64_t id_in = y*sp.sizex+x;
                            uint64_t id_out = (y+y_paste)*size+x+x_paste;
#ifdef CPU_MODE
                            READ_CEILS[id_out] = get_index(sp.data, id_in);
#else
                            set_index(ceils, id_out, get_index(sp.data, id_in));
#endif
                        }
                    }
#ifndef CPU_MODE
                    updateSSBOData(SSBO_IN, ceils);
#endif
                    is_paste = false;
                    paste_struct = std::nullopt;
                } else if (is_all_buttons && is_click_button(pos_rect_saves, sf::Vector2f(buttonsWidth, buttonsHeight), click_pos)) { // saves button
                    paste_struct = show_saves(window);
                    if (paste_struct) {
                        is_paste = true;
                        is_select = false;
                    }
                } else if (is_all_buttons && is_click_button(pos_rect_resize, sf::Vector2f(buttonsWidth, buttonsHeight), click_pos)) { // resize button
                    int new_size = getNumber("enter", "enter size: ", window);
                    if (new_size>1) {
                        size = new_size;
#ifdef CPU_MODE
                        ceils.resize(new_size*new_size);
                        ceils2.resize(new_size*new_size);
#endif
#ifndef CPU_MODE
                        ceils.resize((new_size*new_size+31)/32);
#endif
                        for (int i = 0; i!=size*size; i++) {
#ifdef CPU_MODE
                            READ_CEILS[i] = dis(gen)<=37;
#endif
#ifndef CPU_MODE
                            set_index(ceils, i, dis(gen)<=37);
#endif
                        }
#ifndef CPU_MODE
                        ssboIn = resize_size_ssbo(ssboIn, calc_size_ssbo());
                        ssboOut = resize_size_ssbo(ssboOut, calc_size_ssbo());
                        updateSSBOData(SSBO_IN, ceils);
#endif
                    }
                    clock_simulate.restart();
                    clock.restart();
                } else if (is_all_buttons && is_click_button(pos_rect_clear, sf::Vector2f(buttonsWidth, buttonsHeight), click_pos)) { // clear button
                    for (int i = 0; i!=size*size; i++) {
#ifdef CPU_MODE
                        READ_CEILS[i] = false;
#endif
#ifndef CPU_MODE
                        // ceils[i] = 0;
                        set_index(ceils, i, false);
#endif
                    }
#ifndef CPU_MODE
                    updateSSBOData(SSBO_IN, ceils);
#endif
                    clock_simulate.restart();
                    clock.restart();
                } else if (is_click_button(pos_rect_run_stop, sf::Vector2f(buttonsWidth, buttonsHeight), click_pos)) { // run/stop button
                    is_stop = !is_stop;
                } else if (is_all_buttons && is_click_button(pos_rect_change_sps, sf::Vector2f(buttonsWidth, buttonsHeight), click_pos)) { // run/stop button
                    int new_sps = getNumber("enter", "enter sps: ", window);
                    if (new_sps>0) {
                        sps = new_sps;
                    }
                    clock_simulate.restart();
                    clock.restart();
                } else if (!is_all_buttons && is_select==1) {
                    sf::Vector2f pos_ceilf = get_pos_ceil(click_pos);
                    if (pos_ceilf.x>=0 && pos_ceilf.y>=0 && pos_ceilf.x<=size && pos_ceilf.y<=size) {
                        sf::Vector2i pos_ceil = sf::Vector2i(pos_ceilf.x, pos_ceilf.y);
                        is_select = 2;
                        select_x = pos_ceil.x;
                        select_y = pos_ceil.y;
                        select_sx = 1;
                        select_sy = 1;
                    }
                } else if (!is_all_buttons && is_select==3) {
                    is_select = 0;
                } else if (is_stop && !is_all_buttons) {
//#else
//                    if (is_stop) {
//#endif
                    unsigned long long id = get_id_ceil(click_pos);
                    if (id!=std::numeric_limits<unsigned long long>::max()) {
#ifndef CPU_MODE
                        // ceils[id] = !ceils[id];
                        set_index(ceils, id, get_index(ceils, id)==0);
                        updateSSBOData(SSBO_IN, ceils);
#endif
#ifdef CPU_MODE
                        READ_CEILS[id] = !READ_CEILS[id];
#endif
                    }
                }
            }
#ifndef ANDROID_MODE
                else if (event.type == sf::Event::MouseMoved) {
                if (!is_all_buttons && sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
                    sf::Vector2f newMousePos = sf::Vector2f(event.mouseMove.x, event.mouseMove.y);
                    if (is_select==2) {
                        sf::Vector2f pos_ceilf = get_pos_ceil(newMousePos);
                        if (pos_ceilf.x>=0 && pos_ceilf.y>=0 && pos_ceilf.x<=size && pos_ceilf.y<=size) {
                            sf::Vector2i size_ceils = sf::Vector2i(pos_ceilf.x, pos_ceilf.y)-sf::Vector2i(select_x, select_y);
                            if ((size_ceils.y < 0) != (select_sy < 0)) {
                                if (select_sy<0) {
                                    select_y--;
                                } else {
                                    select_y++;
                                }
                            }
                            if ((size_ceils.x < 0) != (select_sx < 0)) {
                                if (select_sx<0) {
                                    select_x--;
                                } else {
                                    select_x++;
                                }
                            }
                            select_sx = size_ceils.x;
                            select_sy = size_ceils.y;
                            if (select_sx>=0) {
                                select_sx++;
                            }
                            if (select_sy>=0) {
                                select_sy++;
                            }
                        }
                    } else if (is_select!=1 && lastMousePos.x!=-1) {
                        sf::Vector2f direction = newMousePos-lastMousePos;
                        sf::Vector2f direction_ceils = sf::Vector2f(direction.x/calc_size_px(), direction.y/calc_size_px());
                        if (!std::isnan(direction_ceils.x) && !std::isinf(direction_ceils.x) && !std::isnan(direction_ceils.y) && !std::isinf(direction_ceils.y)) {
                            camx -= direction_ceils.x;
                            camy += direction_ceils.y;
                        }
                    }
                }
                lastMousePos = sf::Vector2f(event.mouseMove.x, event.mouseMove.y);
            } else if (!is_all_buttons && event.type==sf::Event::MouseWheelScrolled && event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel) {
                const sf::Vector2f mouse_pos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
#else
            else if (event.type == SDL_EVENT_FINGER_MOTION && !is_all_buttons) {
                //SDL_Log("move");
                int count = 0;
                SDL_Finger** fingers = SDL_GetTouchFingers(event.tfinger.touchID, &count);
                //SDL_Log("fingers: %d", count);

                if (fingers && count >= 2) {
                    //SDL_Log("scale detect");
                    float midX = (fingers[0]->x + fingers[1]->x) / 2.0f;
                    float midY = (fingers[0]->y + fingers[1]->y) / 2.0f;
                    const sf::Vector2f mouse_pos = sf::Vector2f(midX * width, midY * height);
                    //SDL_Log("mouse_pos: (x=%f, y=%f)", mouse_pos.x, mouse_pos.y);
                    float currentDist = hypotf((fingers[0]->x - fingers[1]->x)*width, (fingers[0]->y - fingers[1]->y)*height);
                    //SDL_Log("current dist: %f", currentDist);
                    float dDist = (lastDist > 0) ? (currentDist - lastDist) : 0;
                    //SDL_Log("dDist: %f", dDist);
                    lastDist = currentDist;
                    SDL_free(fingers);
#endif
                    sf::Vector2f last_pos = get_pos_ceil(mouse_pos);
#ifndef ANDROID_MODE
                    scale *= std::pow(1.2, event.mouseWheelScroll.delta);
#else
                    scale *= std::pow(1.2, dDist / std::min(width, height) * 10.0);
#endif
                    sf::Vector2f new_pos = get_pos_ceil(mouse_pos);
                    camx -= new_pos.x - last_pos.x;
                    camy -= new_pos.y - last_pos.y;
#ifdef ANDROID_MODE
                } else {
                    lastDist = 0;
                    if (count==1) {
                        std::optional<int> res = slider_sps.event_to(count, fingers);
                        if (res) {
                            sps = *res;
                        } else {
                            sf::Vector2f newMousePos = sf::Vector2f(fingers[0]->x*width, fingers[0]->y*height);
                            if (is_select==2) {
                                sf::Vector2f pos_ceilf = get_pos_ceil(newMousePos);
                                if (pos_ceilf.x>=0 && pos_ceilf.y>=0 && pos_ceilf.x<=size && pos_ceilf.y<=size) {
                                    sf::Vector2i size_ceils = sf::Vector2i(pos_ceilf.x, pos_ceilf.y)-sf::Vector2i(select_x, select_y);
                                    if ((size_ceils.y < 0) != (select_sy < 0)) {
                                        if (select_sy<0) {
                                            select_y--;
                                        } else {
                                            select_y++;
                                        }
                                    }
                                    if ((size_ceils.x < 0) != (select_sx < 0)) {
                                        if (select_sx<0) {
                                            select_x--;
                                        } else {
                                            select_x++;
                                        }
                                    }
                                    select_sx = size_ceils.x;
                                    select_sy = size_ceils.y;
                                    if (select_sx>=0) {
                                        select_sx++;
                                    }
                                    if (select_sy>=0) {
                                        select_sy++;
                                    }
                                }
                            } else if (is_select!=1) {
                                sf::Vector2f direction = sf::Vector2f(event.tfinger.dx*width, event.tfinger.dy*height);
                                sf::Vector2f direction_ceils = sf::Vector2f(direction.x/calc_size_px(), direction.y/calc_size_px());
                                camx -= direction_ceils.x;
                                camy += direction_ceils.y;
                            }
                        }
                    }
                    SDL_free(fingers);
                }
#endif
            }
#ifndef ANDROID_MODE
            else if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::R) {
                    for (uint64_t i = 0; i!=size*size; i++) {
#ifdef CPU_MODE
                        READ_CEILS[i] = dis(gen)<=37;
#endif
#ifndef CPU_MODE
                        // ceils[i] = dis(gen);
                        set_index(ceils, i, dis(gen)<=37);
#endif
                    }
#ifndef CPU_MODE
                    updateSSBOData(SSBO_IN, ceils);
#endif
                    clock_simulate.restart();
                    clock.restart();
                } else if (event.key.code == sf::Keyboard::E) {
                    if (event.key.alt) {
                        for (uint64_t i = 0; i!=size*size; i++) {
#ifdef CPU_MODE
                            READ_CEILS[i] = false;
#endif
#ifndef CPU_MODE
                            // ceils[i] = 0;
                            set_index(ceils, i, false);
#endif
                        }
#ifndef CPU_MODE
                        updateSSBOData(SSBO_IN, ceils);
#endif
                        clock_simulate.restart();
                        clock.restart();
                    } else {
                        scale /= 1.2;
                    }
                } else if (event.key.code == sf::Keyboard::Q) {
                    scale *= 1.2;
                } else if (event.key.code == sf::Keyboard::F) {
                    int new_sps = getNumber("enter", "enter sps: ", window);
                    if (new_sps>0) {
                        sps = new_sps;
                    }
                    clock_simulate.restart();
                    clock.restart();
                } else if (event.key.code == sf::Keyboard::S) {
                    if (event.key.shift) {
                        is_stop = !is_stop;
                    } else if (!event.key.alt) {
                        int new_size = getNumber("enter", "enter size: ", window);
                        if (new_size>1) {
                            size = new_size;
#ifdef CPU_MODE
                            ceils.resize(new_size*new_size);
                            ceils2.resize(new_size*new_size);
#endif
#ifndef CPU_MODE
                            ceils.resize((new_size*new_size+31)/32);
#endif

                            for (uint64_t i = 0; i!=size*size; i++) {
#ifdef CPU_MODE
                                READ_CEILS[i] = dis(gen)<=37;
#endif
#ifndef CPU_MODE
                                set_index(ceils, i, dis(gen)<=37);
#endif
                            }
#ifndef CPU_MODE
                            ssboIn = resize_size_ssbo(ssboIn, calc_size_ssbo());
                            ssboOut = resize_size_ssbo(ssboOut, calc_size_ssbo());
                            updateSSBOData(SSBO_IN, ceils);
#endif
                        }
                        clock_simulate.restart();
                        clock.restart();
                    } else if (!is_paste) {
                        is_select = 1;
                    }
                }
            }
#endif
        }
        last_simulate += clock_simulate.restart().asSeconds();
        bool calc = last_simulate>=(1.0/sps);
        while (last_simulate>=(1.0/sps)) {
            bool calc = last_simulate>=(1.0/sps);
            if (calc) {
                last_simulate -= 1.0/sps;
            }
            if (!is_stop && calc) {
#ifndef CPU_MODE
                gpuRun22d(shader, size, size, SSBO_IN, SSBO_OUT, "sizexy", size);
#endif
#ifdef CPU_MODE
#pragma omp parallel for
                for (uint32_t y = 0; y!=size; y++) {
                    for (uint32_t x = 0; x!=size; x++) {
                        //uint32_t x = id % size;
                        //uint32_t y = id / size;
                        uint64_t id = x+y*size;
                        uint32_t neighbors = 0;
                        if (x > 0 && READ_CEILS[y*size + (x-1)] == 1) neighbors++; // лево
                        if (x+1 < size && READ_CEILS[y*size + (x+1)] == 1) neighbors++; // право

                        if (y+1 < size) {
                            if (READ_CEILS[(y+1)*size + x] == 1) neighbors++; // низ
                            if (x > 0 && READ_CEILS[(y+1)*size + (x-1)] == 1) neighbors++; // низ-лево
                            if (x+1 < size && READ_CEILS[(y+1)*size + (x+1)] == 1) neighbors++; // низ-право
                        }

                        if (y > 0) {
                            if (READ_CEILS[(y-1)*size + x] == 1) neighbors++; // верх
                            if (x > 0 && READ_CEILS[(y-1)*size + (x-1)] == 1) neighbors++; // верх-лево
                            if (x+1 < size && READ_CEILS[(y-1)*size + (x+1)] == 1) neighbors++; // верх-право
                        }
                        bool current = READ_CEILS[id];
                        bool result = false;
                        if (current == 1) {
                            if (neighbors == 2 || neighbors == 3) result = true; // Выжил
                        } else {
                            if (neighbors == 3) result = true; // Родился
                        }
                        WRITE_CEILS[id] = result;
                    }
                }
#endif
                active = !active;
            }
        }
#ifndef ANDROID_MODE
        window.clear(sf::Color(50, 50, 50));
#else
        glUseProgram(graphicsProgramID);
        clearWindow(sf::Color(50, 50, 50));
#endif
#ifndef CPU_MODE
        if (!is_stop && calc) {
            downloadSSBOData(SSBO_IN, ceils);
        }
#endif
        float sizepx = calc_size_px();
        double gbs = get_base_scale()*scale;
        // sf::RectangleShape rect;
        // rect.setSize(sf::Vector2f(sizepx, sizepx));
        // rect.setOutlineThickness(-sizepx*0.05);
        // rect.setOutlineColor(sf::Color(100, 100, 100));
        std::pair<sf::Vector2i, sf::Vector2i> view = get_view();
        rects.reserve(size*size);
        uint32_t sizey = (view.second.x-view.first.x+1);
        rects.resize(sizey*(view.second.y-view.first.y+1));
        // std::cout << rects.size() << std::endl;
#pragma omp parallel for
        // for (uint32_t i = view.first.y*size+view.first.x; i!=std::min(size*size, view.second.y*size+view.second.x+1); i++) {
        for (uint32_t y = view.first.y; y!=view.second.y+1; y++) {
            // uint32_t x = i%size;
            // if (view.first.x<=x && x<=view.second.x) {
            for (uint32_t x = view.first.x; x!=view.second.x+1; x++) {
                // uint32_t y = i/size;
                uint32_t i = y*size+x;
                // #ifdef CPU_MODE
                // rect.setFillColor(READ_CEILS[i]? sf::Color::Black: sf::Color::White);
                // #endif
                // #ifndef CPU_MODE
                // rect.setFillColor(ceils[i]? sf::Color::Black: sf::Color::White);
                // #endif
                // rect.setPosition(calc_pos(x, y));
                // window.draw(rect);
                size_t index = x-view.first.x+(y-view.first.y)*sizey;
#ifdef CPU_MODE
                // rects.emplace_back(calc_pos(x, y), READ_CEILS[i]);
                rects[index].first = calc_pos(x, y, gbs);
                rects[index].second = READ_CEILS[i];
#endif
#ifndef CPU_MODE
                // rects.emplace_back(calc_pos(x, y), get_index(ceils, i)==1);
                rects[index].first = calc_pos(x, y, gbs);
                rects[index].second = get_index(ceils, i)==1;
#endif
            }
        }
        sf::RectangleShape fon;
        fon.setFillColor(sf::Color(100, 100, 100));
        fon.setPosition(calc_pos(0, -1, gbs));
        fon.setSize(calc_pos(size, size, gbs)-calc_pos(0, 0, gbs));
#ifndef ANDROID_MODE
        window.draw(fon);
#else
        fon.render();
#endif
        vex.resize(rects.size()*4);
#pragma omp parallel for
        for (uint64_t i = 0; i!=rects.size(); i++) {
            const auto& rect = rects[i];
            sf::Color col = (rect.second? sf::Color::Black: sf::Color::White);
            vex[i*4].position = rect.first+sf::Vector2f(sizepx*0.05, sizepx*0.05);
            vex[i*4].color = col;
            vex[i*4+1].position = rect.first+sf::Vector2f(sizepx*0.95, sizepx*0.05);
            vex[i*4+1].color = col;
            vex[i*4+2].position = rect.first+sf::Vector2f(sizepx*0.95, sizepx*0.95);
            vex[i*4+2].color = col;
            vex[i*4+3].position = rect.first+sf::Vector2f(sizepx*0.05, sizepx*0.95);
            vex[i*4+3].color = col;
        }
        size_t size_rects = rects.size();
        rects.clear();
#ifndef ANDROID_MODE
        window.draw(vex);
#else
        vex.render();
#endif
        if (is_paste) {
            save_struct& sp = *paste_struct;
            int x_paste = camx-sp.sizex/2;
            int y_paste = camy-sp.sizey/2;
            sf::VertexArray paste_vex(sf::Quads);
            paste_vex.resize(sp.sizex*sp.sizey*4);
            float padd_start = sizepx*0.05;
            float padd_end = sizepx*0.95;
            for (uint32_t y = std::min((int)sp.sizey, std::max(0, -(int)y_paste)); y!=std::max(std::min((int)sp.sizey, (int)size - y_paste), 0); y++) {
                for (uint32_t x = std::min((int)sp.sizex, std::max(0, -(int)x_paste)); x!=std::max(std::min((int)sp.sizex, (int)size - x_paste), 0); x++) {
                    uint64_t id = y*sp.sizex+x;
                    uint64_t id_in = id*4;
                    sf::Color col = (get_index(sp.data, id)? sf::Color::Black: sf::Color::White);
                    col.a = 150;
                    sf::Vector2f cp = calc_pos(x+x_paste, y+y_paste);

                    paste_vex[id_in].position = cp+sf::Vector2f(padd_start, padd_start);
                    paste_vex[id_in].color = col;
                    id_in++;
                    paste_vex[id_in].position = cp+sf::Vector2f(padd_start, padd_end);
                    paste_vex[id_in].color = col;
                    id_in++;
                    paste_vex[id_in].position = cp+sf::Vector2f(padd_end, padd_end);
                    paste_vex[id_in].color = col;
                    id_in++;
                    paste_vex[id_in].position = cp+sf::Vector2f(padd_end, padd_start);
                    paste_vex[id_in].color = col;
                }
            }
#ifdef ANDROID_MODE
            paste_vex.render();
#else
            window.draw(paste_vex);
#endif
        }
        if (is_select>1) {
            sf::RectangleShape rect_select;
            rect_select.setSize(sf::Vector2f(select_sx*sizepx, -select_sy*sizepx));
            rect_select.setPosition(calc_pos(select_x, select_y-1));
            rect_select.setFillColor(sf::Color(30, 60, 200, 100));
#ifndef ANDROID_MODE
            window.draw(rect_select);
#else
            rect_select.render();
#endif
        }
//#ifdef ANDROID_MODE
        float buttonsWidth = safeArea.w*0.5;
        float buttonsHeight = safeArea.h*0.1;
        float buttonsRound = std::min(safeArea.w, safeArea.h)*0.02;
        if (is_all_buttons) {
            sf::RectangleShape all;
            all.setPosition(sf::Vector2f(0, 0));
            all.setSize(sf::Vector2f(width, height));
            all.setFillColor(sf::Color(0, 0, 0, 100));
#ifdef ANDROID_MODE
            all.render();
#else
            window.draw(all);
#endif
            // buttons (resize, clear, stop/run)
            sf::Vector2f pos_rect = sf::Vector2f(safeArea.x + safeArea.w * 0.25,safeArea.h * 0.1 + safeArea.y);
            // button resize
            {
                auto rect = createRoundedRect(buttonsWidth, buttonsHeight, buttonsRound);
                rect.setPosition(pos_rect);
                rect.setFillColor(sf::Color(200, 220, 20));
                auto text = createTextForButton("resize", pos_rect, buttonsWidth, buttonsHeight,
                                                buttonsRound);
                text.setFillColor(sf::Color::White);
#ifdef ANDROID_MODE
                rect.render();
                text.render();
#else
                window.draw(rect);
                window.draw(text);
#endif
            }
            // button clear
            {
                auto rect = createRoundedRect(buttonsWidth, buttonsHeight, buttonsRound);
                pos_rect += sf::Vector2f(0, safeArea.h * 0.12);
                rect.setPosition(pos_rect);
                rect.setFillColor(sf::Color(150, 150, 150));
                auto text = createTextForButton("clear", pos_rect, buttonsWidth, buttonsHeight, buttonsRound);
                text.setFillColor(sf::Color::White);
#ifdef ANDROID_MODE
                rect.render();
                text.render();
#else
                window.draw(rect);
                window.draw(text);
#endif
            }
            // button change SPS
            {
                auto rect = createRoundedRect(buttonsWidth, buttonsHeight, buttonsRound);
                pos_rect += sf::Vector2f(0, safeArea.h * 0.12);
                rect.setPosition(pos_rect);
                rect.setFillColor(sf::Color(10, 220, 10));
                auto text = createTextForButton("change SPS", pos_rect, buttonsWidth, buttonsHeight, buttonsRound);
                text.setFillColor(sf::Color::White);
#ifdef ANDROID_MODE
                rect.render();
                text.render();
#else
                window.draw(rect);
                window.draw(text);
#endif
            }
            // button saves
            {
                auto rect = createRoundedRect(buttonsWidth, buttonsHeight, buttonsRound);
                pos_rect += sf::Vector2f(0, safeArea.h * 0.12);
                rect.setPosition(pos_rect);
                rect.setFillColor(sf::Color(50, 100, 180));
                auto text = createTextForButton("saves", pos_rect, buttonsWidth, buttonsHeight, buttonsRound);
                text.setFillColor(sf::Color::White);
#ifdef ANDROID_MODE
                rect.render();
                text.render();
#else
                window.draw(rect);
                window.draw(text);
#endif
            }
        }
        auto rect = createRoundedRect(buttonsWidth, buttonsHeight, buttonsRound);
        sf::Vector2f pos_rect = sf::Vector2f(safeArea.x, safeArea.h*0.9+safeArea.y);
        rect.setPosition(pos_rect);
        rect.setFillColor(sf::Color(220, 10, 10));
        auto text = createTextForButton("random", pos_rect, buttonsWidth, buttonsHeight, buttonsRound);
        text.setFillColor(sf::Color::White);
#ifdef ANDROID_MODE
        rect.render();
        text.render();
#else
        window.draw(rect);
        window.draw(text);
#endif
        auto rect2 = createRoundedRect(buttonsWidth, buttonsHeight, buttonsRound);
        sf::Vector2f pos_rect2 = sf::Vector2f(safeArea.x+safeArea.w*0.5, safeArea.h*0.9+safeArea.y);
        rect2.setPosition(pos_rect2);
        if (!is_stop) {
            rect2.setFillColor(sf::Color(220, 10, 10));
        } else {
            rect2.setFillColor(sf::Color(10, 220, 10));
        }
        auto text2 = createTextForButton((is_stop ?"run": "stop"), pos_rect2, buttonsWidth, buttonsHeight, buttonsRound);
        text2.setFillColor(sf::Color::White);
#ifdef ANDROID_MODE
        rect2.render();
        text2.render();
#else
        window.draw(rect2);
        window.draw(text2);
#endif

        // render all button
        {
            float size_button = std::min(safeArea.w, safeArea.h)*0.1;
            float padding_line = size_button/2.0*0.3;
            float size_line = (size_button-padding_line*2)/3;
            padding_line += size_line;
#ifdef ANDROID_MODE
            sf::Vector2f start_pos_button = sf::Vector2f(safeArea.w-size_button*1.1+safeArea.x, size_button*0.1+safeArea.y+safeArea.h*0.05);
#else
            sf::Vector2f start_pos_button = sf::Vector2f(safeArea.w-size_button*1.1+safeArea.x, size_button*0.1+safeArea.y);
#endif
            auto line1 = createRoundedRect(size_button, size_line, size_line*0.5);
            line1.setPosition(start_pos_button+sf::Vector2f(0, padding_line*0));
            auto line2 = createRoundedRect(size_button, size_line, size_line*0.5);
            line2.setPosition(start_pos_button+sf::Vector2f(0, padding_line*1));
            auto line3 = createRoundedRect(size_button, size_line, size_line*0.5);
            line3.setPosition(start_pos_button+sf::Vector2f(0, padding_line*2));

            const sf::Color color_lines = sf::Color(110, 110, 110);
            line1.setFillColor(color_lines);
            line2.setFillColor(color_lines);
            line3.setFillColor(color_lines);

#ifdef ANDROID_MODE
            line1.render();
            line2.render();
            line3.render();
#else
            window.draw(line1);
            window.draw(line2);
            window.draw(line3);
#endif
            // render select button
            sf::Vector2f pos_select_button = start_pos_button-sf::Vector2f(size_button*1.1, 0);
            sf::RectangleShape select_button;
            select_button.setSize(sf::Vector2f(size_button, size_button));
            select_button.setPosition(pos_select_button);
#ifndef ANDROID_MODE
            if (!is_paste) {
                select_button.setTexture(&select_icon);
            } else {
                select_button.setTexture(&build_icon);
            }
            window.draw(select_button);
#else
            select_button.setTexture();
            if (!is_paste) {
                select_icon.use();
            } else {
                build_icon.use();
            }
            select_button.setTransparency();
            select_button.render();
            select_button.setUnTransparency();
            if (!is_paste) {
                select_icon.unuse();
            } else {
                build_icon.unuse();
            }
#endif
            // render save button
            if (is_select==3) {
                sf::Vector2f pos_save_button = pos_select_button-sf::Vector2f(size_button*1.1, 0);
                sf::RectangleShape save_button;
                save_button.setSize(sf::Vector2f(size_button, size_button));
                save_button.setPosition(pos_save_button);
#ifndef ANDROID_MODE
                save_button.setTexture(&save_icon);
                window.draw(save_button);
#else
                save_button.setTexture();
                save_icon.use();
                save_button.setTexture();
                save_button.setTransparency();
                save_button.render();
                save_button.setUnTransparency();
                save_icon.unuse();
#endif
            }
        }
#ifdef ANDROID_MODE
        sf::Vector2f pos_slider = sf::Vector2f(safeArea.h*0.02+safeArea.x, safeArea.h*0.02+safeArea.y+safeArea.h*0.05);
#else
        sf::Vector2f pos_slider = sf::Vector2f(safeArea.h*0.02+safeArea.x, safeArea.h*0.02+safeArea.y);
#endif
        slider_sps.setPosition(pos_slider);
        slider_sps.setSize(sf::Vector2f(safeArea.w*0.2, safeArea.h*0.02));
#ifdef ANDROID_MODE
        slider_sps.render(sps);
#else
        slider_sps.render(window, sps);
#endif
        sf::Text text_sps;
        text_sps.setPosition(pos_slider+sf::Vector2f(safeArea.w*0.22, 0));
        text_sps.setCharacterSize(safeArea.h*0.02);
        text_sps.setString(std::to_string(sps)+" SPS");
        text_sps.setFillColor(sf::Color(46, 204, 113));
        text_sps.setFont(defaultFont);
#ifdef ANDROID_MODE
        text_sps.render();
#else
        window.draw(text_sps);
#endif
#ifdef ANDROID_MODE
        // render title
        float dt = clock.restart().asSeconds();
        std::string title = "FPS="+std::to_string((int)(1/dt))+", rects="+std::to_string(size_rects) + "("+std::to_string(view.second.x-view.first.x+1)+"x"+std::to_string(view.second.y-view.first.y+1)+")";
        sf::RectangleShape title_rect;
        title_rect.setPosition(sf::Vector2f(0,0));
        title_rect.setSize(sf::Vector2f(width, safeArea.h*0.05+safeArea.y));
        title_rect.setFillColor(sf::Color(100, 100, 100));
        sf::Text title_text;
        title_text.setString(title);
        bool val = (safeArea.h*0.05*(width/(safeArea.h*0.05)>20));
        title_text.setPosition(sf::Vector2f(safeArea.x, safeArea.y+(val?0:safeArea.h*0.025)));
        title_text.setCharacterSize(safeArea.h*0.05*(val?1:0.5));
        title_text.setFont(defaultFont);
        title_rect.render();
        title_text.render();
#else
        float dt = clock.restart().asSeconds();
#endif
#ifndef ANDROID_MODE
        std::string title = "Game of Live (FPS="+std::to_string((int)(1/dt))+", SPS="+std::to_string(sps)+", rects="+std::to_string(size_rects) + "("+std::to_string(view.second.x-view.first.x+1)+"x"+std::to_string(view.second.y-view.first.y+1)+"))";
        window.setTitle(title);
        window.display();
#else
        //SDL_SetWindowTitle(window, title.c_str());
        SDL_GL_SwapWindow(window);
        if (1/30.0>dt-lastDelay) {
            lastDelay = (1/30.0-dt+lastDelay);
            SDL_Delay(lastDelay*1000);
            //SDL_Log("delay: %d", (int)(lastDelay*1000));
        }
        //lastDt = dt;
#endif
    }
#ifdef ANDROID_MODE
    TTF_Quit();
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
#endif
    return 0;
}
