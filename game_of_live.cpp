// game of live on gpu and cpu
// cd C:/Users/Azerty/Desktop/Программы/filescpp; g++ game_of_live.cpp -o game_of_live.exe -I"c:/Users/Azerty/Downloads/SFML-2.6.1/include" -L"c:/Users/Azerty/Downloads/SFML-2.6.1/lib" -lsfml-graphics-d -lsfml-window-d -lsfml-system-d -lopengl32 -lwinmm -lgdi32 -lcomdlg32 -lws2_32 -fconcepts-diagnostics-depth=2 -Wfatal-errors
// #define CPU_MODE
#include <SFML/Graphics.hpp>
#include <random>
// #include <iostream>
#ifndef CPU_MODE
#include "gpu_loader.cpp"
#endif
#include <mutex>
std::mutex mtx;
int width = 800;
int height = 600;
uint32_t size = 20; // Количество клеток по одной стороне (поле size x size)
double camx = 10;   // Центр камеры (в клетках)
double camy = 10;
double scale = 1.0; // 1.0 = поле занимает весь экран

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

inline int calc_size_px() {
    // Размер одной стороны квадрата в пикселях
    return (int)(get_base_scale() * scale);
}
inline int calc_size_ssbo() {
    return (size*size + 31) / 32*4;
}
inline int get_id_ceil(const sf::Vector2f pos) {
    double s = get_base_scale() * scale;
    int x = std::floor((pos.x - width/2) / s + camx);
    int y = std::floor((height/2-pos.y) / s + camy)+1;
    if (x < 0 || y < 0 || x >= size || y >= size) {
        return -1;
    }
    return y * size + x;
}
inline sf::Vector2f get_pos_ceil(const sf::Vector2f pos) {
    double s = get_base_scale() * scale;
    float x = ((pos.x-width / 2)/s - camx);
    float y = ((height/2-pos.y)/s - camy);
    return sf::Vector2f(x, y);
}
inline std::pair<sf::Vector2i, sf::Vector2i> get_view() {
    double s = get_base_scale() * scale;
    double radius_x = (width / 2.0) / s;
    double radius_y = (height / 2.0) / s;
    int x_start = (int)std::floor(camx - radius_x);
    int x_end   = (int)std::ceil(camx + radius_x);
    int y_start = (int)std::floor(camy - radius_y);
    int y_end   = (int)std::ceil(camy + radius_y);
    x_start = std::max(0, x_start);
    y_start = std::max(0, y_start);
    x_end   = std::min((int)size - 1, x_end);
    y_end   = std::min((int)size - 1, y_end);
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
const char* shader_code = R"(#version 430 core
#extension GL_ARB_compute_shader : enable
#extension GL_ARB_shader_storage_buffer_object : enable

layout(local_size_x = 64) in;
// Тот самый буфер (binding = 0), который мы привязывали в C++
layout(std430, binding = 0) buffer InBuffer {
    uint ceil[];
};
layout(std430, binding = 1) buffer OutBuffer {
    uint ceils[];
};
uniform uint total;
uniform uint sizexy;
uint get_index(uint index) {
    return (ceil[index/32]>>(index%32)) & 1;
}
void set_index(uint index, uint val) {
    uint bit_pos = index % 32;
    uint array_idx = index / 32;
    if (val == 1) {
        atomicOr(ceils[array_idx], (1u << bit_pos));
    } else {
        atomicAnd(ceils[array_idx], ~(1u << bit_pos));
    }
}
void main() {
    uint id = gl_GlobalInvocationID.x;
    if (id >= total) return;
    uint x = id % sizexy; 
    uint y = id / sizexy;
    uint neighbors = 0;
    if (x > 0 && get_index(y*sizexy + (x-1)) == 1) neighbors++; // лево
    if (x+1 < sizexy && get_index(y*sizexy + (x+1)) == 1) neighbors++; // право
    if (y+1 < sizexy) {
        if (get_index((y+1)*sizexy + x) == 1) neighbors++; // низ
        if (x > 0 && get_index((y+1)*sizexy + (x-1)) == 1) neighbors++; // низ-лево
        if (x+1 < sizexy && get_index((y+1)*sizexy + (x+1)) == 1) neighbors++; // низ-право
    }
    
    if (y > 0) {
        if (get_index((y-1)*sizexy + x) == 1) neighbors++; // верх
        if (x > 0 && get_index((y-1)*sizexy + (x-1)) == 1) neighbors++; // верх-лево
        if (x+1 < sizexy && get_index((y-1)*sizexy + (x+1)) == 1) neighbors++; // верх-право
    }
    uint current = get_index(id);
    uint result = 0;
    if (current == 1) {
        if (neighbors == 2 || neighbors == 3) result = 1; // Выжил
    } else {
        if (neighbors == 3) result = 1; // Родился
    }
    set_index(id, result);
}
)"
;
#ifndef CPU_MODE
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
#endif
int sps = 5;
int main() {
    defaultFont.loadFromFile("C:/Windows/Fonts/arial.ttf");
    sf::RenderWindow window(sf::VideoMode(800, 600), "Game Of Live (FPS=30)");
    window.setFramerateLimit(30);
    bool active = true;
    bool is_stop = false;
    #ifndef CPU_MODE
    loadGPUFunctions();
    #define SSBO_IN (active ? ssboIn: ssboOut)
    #define SSBO_OUT (active ? ssboOut: ssboIn)
    auto ssboIn = createEmptySSBO(calc_size_ssbo());
    auto ssboOut = createEmptySSBO(calc_size_ssbo());
    auto shader = compileShader(shader_code);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 1);
    std::vector<uint32_t> ceils((size*size + 31) / 32);
    for (int i = 0; i!=size*size; i++) {
        set_index(ceils, i, dis(gen)==1);
    }
    updateSSBOData(ssboIn, ceils);
    #endif
    #ifdef CPU_MODE
    #define WRITE_CEILS (active? ceils: ceils2)
    #define READ_CEILS (active? ceils2: ceils)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 1);
    std::vector<bool> ceils(size*size);
    std::vector<bool> ceils2(size*size);
    for (int i = 0; i!=ceils.size(); i++) {
        READ_CEILS[i] = dis(gen);
    }
    #endif
    sf::Clock clock;
    sf::Clock clock_simulate;
    double last_simulate = 0;
    std::vector<std::pair<sf::Vector2f, bool>> rects;
    sf::VertexArray vex(sf::Quads);
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) 
                window.close();
            else if (event.type==sf::Event::MouseButtonPressed) {
                const sf::Vector2f click_pos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                int id = get_id_ceil(click_pos);
                if (id!=-1) {
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
            else if (event.type==sf::Event::MouseWheelScrolled && event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel) {
                const sf::Vector2f mouse_pos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                sf::Vector2f last_pos = get_pos_ceil(mouse_pos);
                scale *= std::pow(1.2, event.mouseWheelScroll.delta);
                sf::Vector2f new_pos = get_pos_ceil(mouse_pos);
                camx -= new_pos.x-last_pos.x;
                camy -= new_pos.y-last_pos.y;
            }
            else if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::R) {
                    for (int i = 0; i!=ceils.size(); i++) {
                        #ifdef CPU_MODE    
                        READ_CEILS[i] = dis(gen);
                        #endif
                        #ifndef CPU_MODE
                        // ceils[i] = dis(gen);
                        set_index(ceils, i, dis(gen)==1);
                        #endif
                    }
                    #ifndef CPU_MODE
                    updateSSBOData(SSBO_IN, ceils);
                    #endif
                    clock_simulate.restart();
                    clock.restart();
                } else if (event.key.code == sf::Keyboard::E) {
                    if (event.key.alt) {
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
                    } else {
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
                                READ_CEILS[i] = dis(gen);
                                #endif
                                #ifndef CPU_MODE
                                set_index(ceils, i, dis(gen)==1);
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
                    }
                }
            }
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
                gpuRun2(shader, size*size, SSBO_IN, SSBO_OUT, "sizexy", size, "total", size*size);
                #endif
                #ifdef CPU_MODE
                #pragma omp parallel for
                for (uint32_t id = 0; id!=ceils.size(); id++) {
                    uint32_t x = id % size;
                    uint32_t y = id / size;
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
                #endif
            }
        }
        window.clear(sf::Color(50, 50, 50));
        #ifndef CPU_MODE
        if (!is_stop && calc) {
            downloadSSBOData(SSBO_OUT, ceils);
        }
        #endif
        int sizepx = calc_size_px();
        double gbs = get_base_scale()*scale;
        // sf::RectangleShape rect;
        // rect.setSize(sf::Vector2f(sizepx, sizepx));
        // rect.setOutlineThickness(-sizepx*0.05);
        // rect.setOutlineColor(sf::Color(100, 100, 100));
        if (!is_stop && calc) {
            active = !active;
        }
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
        sf::RectangleShape fon; // обводка для всех сразу
        fon.setFillColor(sf::Color(100, 100, 100));
        fon.setPosition(calc_pos(0, 0, gbs));
        fon.setSize(calc_pos(size, size-1, gbs)-calc_pos(0, 0, gbs));
        window.draw(fon);
        vex.resize(rects.size()*4);
        #pragma omp parallel for
        for (uint32_t i = 0; i!=rects.size(); i++) {
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
        window.draw(vex);
        float dt = clock.restart().asSeconds();
        window.setTitle("Game Of Live (FPS="+(is_stop? "stop": std::to_string((int)(1/dt)))+", SPS="+(is_stop? "stop": std::to_string(std::min((int)(1/dt), sps)))+", rects="+std::to_string(size_rects)+")");
        window.display();
    }
}
