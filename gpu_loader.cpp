#ifndef ANDROID_MODE
#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h> // Если этого файла нет, скачай его (1 файл) с сайта Khronos
// Объявляем указатели на функции
PFNGLCREATESHADERPROC glCreateShader;
PFNGLSHADERSOURCEPROC glShaderSource;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLATTACHSHADERPROC glAttachShader;
PFNGLLINKPROGRAMPROC glLinkProgram;
PFNGLUSEPROGRAMPROC glUseProgram;
PFNGLGENBUFFERSPROC glGenBuffers;
PFNGLBINDBUFFERPROC glBindBuffer;
PFNGLBUFFERDATAPROC glBufferData;
PFNGLBINDBUFFERBASEPROC glBindBufferBase;
PFNGLDISPATCHCOMPUTEPROC glDispatchCompute;
PFNGLMEMORYBARRIERPROC glMemoryBarrier;
PFNGLGETBUFFERSUBDATAPROC glGetBufferSubData;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
PFNGLUNIFORM1DPROC glUniform1d;
PFNGLUNIFORM1UIPROC glUniform1ui;
PFNGLBUFFERSUBDATAPROC glBufferSubData;
PFNGLGETSHADERIVPROC glGetShaderiv;           // Узнать, скомпилировался ли
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog; // Получить текст ошибки
PFNGLGETPROGRAMIVPROC glGetProgramiv;         // Узнать статус линковки
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog; // Ошибка линковки
PFNGLUNIFORM1FPROC glUniform1f;
PFNGLUNIFORM1IPROC glUniform1i;
PFNGLGETBUFFERPARAMETERIVPROC glGetBufferParameteriv;
PFNGLDELETEBUFFERSPROC glDeleteBuffers;
// надо эту функцию в main в начале вызвать
void loadGPUFunctions() {
    glCreateShader = (PFNGLCREATESHADERPROC)wglGetProcAddress("glCreateShader");
    glShaderSource = (PFNGLSHADERSOURCEPROC)wglGetProcAddress("glShaderSource");
    glCompileShader = (PFNGLCOMPILESHADERPROC)wglGetProcAddress("glCompileShader");
    glCreateProgram = (PFNGLCREATEPROGRAMPROC)wglGetProcAddress("glCreateProgram");
    glAttachShader = (PFNGLATTACHSHADERPROC)wglGetProcAddress("glAttachShader");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)wglGetProcAddress("glLinkProgram");
    glUseProgram = (PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram");
    glGenBuffers = (PFNGLGENBUFFERSPROC)wglGetProcAddress("glGenBuffers");
    glBindBuffer = (PFNGLBINDBUFFERPROC)wglGetProcAddress("glBindBuffer");
    glBufferData = (PFNGLBUFFERDATAPROC)wglGetProcAddress("glBufferData");
    glBindBufferBase = (PFNGLBINDBUFFERBASEPROC)wglGetProcAddress("glBindBufferBase");
    glDispatchCompute = (PFNGLDISPATCHCOMPUTEPROC)wglGetProcAddress("glDispatchCompute");
    glMemoryBarrier = (PFNGLMEMORYBARRIERPROC)wglGetProcAddress("glMemoryBarrier");
    glGetBufferSubData = (PFNGLGETBUFFERSUBDATAPROC)wglGetProcAddress("glGetBufferSubData");
    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)wglGetProcAddress("glGetUniformLocation");
    glUniform1d = (PFNGLUNIFORM1DPROC)wglGetProcAddress("glUniform1d");
    glUniform1ui = (PFNGLUNIFORM1UIPROC)wglGetProcAddress("glUniform1ui");
    glBufferSubData = (PFNGLBUFFERSUBDATAPROC)wglGetProcAddress("glBufferSubData");
    glGetShaderiv = (PFNGLGETSHADERIVPROC)wglGetProcAddress("glGetShaderiv");
    glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)wglGetProcAddress("glGetShaderInfoLog");
    glGetProgramiv = (PFNGLGETPROGRAMIVPROC)wglGetProcAddress("glGetProgramiv");
    glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)wglGetProcAddress("glGetProgramInfoLog");
    glUniform1f = (PFNGLUNIFORM1FPROC)wglGetProcAddress("glUniform1f");
    glUniform1i = (PFNGLUNIFORM1IPROC)wglGetProcAddress("glUniform1i");
    glGetBufferParameteriv = (PFNGLGETBUFFERPARAMETERIVPROC)wglGetProcAddress("glGetBufferParameteriv");
    glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)wglGetProcAddress("glDeleteBuffers");
}
#else
#include <GLES3/gl32.h>
#include <string.h>
void glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void* data) {
    void* mappedPtr = glMapBufferRange(target, offset, size, GL_MAP_READ_BIT);
    if (mappedPtr) {
        memcpy(data, mappedPtr, size);
        glUnmapBuffer(target);
    }
}
void loadGPUFunctions() {}
#endif
#include <iostream>

// 1. Функция для проверки ошибок OpenGL после команд
void checkGL(const char* label) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        #ifndef ANDROID_MODE
        std::cout << "ОШИБКА OpenGL [" << label << "]: 0x" << std::hex << err << std::dec << std::endl;
        #else
        SDL_Log("ОШИБКА OpenGL [%s]: 0x%x", label, err);
        #endif
        exit(1);
    }
}

GLuint compileShader(const char* shaderSource) {
    // 1. Создаем шейдер
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    
    // ДОБАВЬ ЭТОТ ВЫВОД:
    #ifndef ANDROID_MODE
    std::cout << "DEBUG: Попытка создания Compute Shader. ID: " << shader << std::endl;
    #else
    SDL_Log("DEBUG: Попытка создания Compute Shader. ID: %u", shader);
    #endif
    
    if (shader == 0) {
        #ifndef ANDROID_MODE
        std::cout << "ОШИБКА: Видеокарта вернула 0. Compute Shaders не поддерживаются!" << std::endl;
        #else
        SDL_Log("ОШИБКА: Видеокарта вернула 0. Compute Shaders не поддерживаются!");
        #endif
        return 0;
    }
    GLint length = (GLint)strlen(shaderSource);
    glShaderSource(shader, 1, &shaderSource, &length);
    glCompileShader(shader);

    // Проверяем, скомпилировался ли шейдер
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        #ifndef ANDROID_MODE
        std::cout << "ОШИБКА КОМПИЛЯЦИИ ШЕЙДЕРА:\n" << infoLog << std::endl;
        #else
        SDL_Log("ОШИБКА КОМПИЛЯЦИИ ШЕЙДЕРА:\n%s", infoLog);
        #endif
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);

    // Проверяем линковку программы
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        #ifndef ANDROID_MODE
        std::cout << "ОШИБКА ЛИНКОВКИ ПРОГРАММЫ:\n" << infoLog << std::endl;
        #else
        SDL_Log("ОШИБКА ЛИНКОВКИ ПРОГРАММЫ:\n%s", infoLog);
        #endif
        return 0;
    }
    #ifndef ANDROID_MODE
    std::cout << "Шейдер успешно собран! ID: " << program << std::endl;
    #else
    SDL_Log("Шейдер успешно собран! ID: %u", program);
    #endif
    return program;
}

GLuint createEmptySSBO(size_t sizeInBytes) {
    if (sizeInBytes == 0) return 0;
    glUseProgram(0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0); // GL_SHADER_STORAGE_BUFFER, слот 0, NULL
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); 
    glFinish();
    GLuint ssbo;
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo); 
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeInBytes, NULL, GL_STREAM_DRAW); 
    checkGL("createEmptySSBO (BufferData)");
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); 
    return ssbo;
}
void deleteSSBO(GLuint ssbo) {
    checkGL("no deleteSSBO");
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glFinish(); 
    glDeleteBuffers(1, &ssbo);
    checkGL("deleteSSBO");
}
#include <vector>
template<typename T>
void updateSSBOData(GLuint ssbo, const std::vector<T>& data) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, data.size() * sizeof(T), data.data());
    checkGL("updateSSBOData");
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}
template<typename T>
void updateSSBODataIndex(GLuint ssbo, size_t index, const std::vector<T>& data) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(T)*index, sizeof(T), &data[index]);
    checkGL("updateSSBOData");
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}


template<typename T>
void downloadSSBOData(GLuint ssbo, std::vector<T>& data) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, data.size() * sizeof(T), data.data());
    checkGL("downloadSSBOData");
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}
void _set_unif(unsigned int prog) {} 

// 2. Основной шаблон, который "разбирает" пары: имя юниформа + значение
template<typename T, typename... Args>
void _set_unif(unsigned int prog, const char* name, T val, Args... args) {
    int loc = glGetUniformLocation(prog, name);
    if (loc != -1) {
        // Проверяем тип данных и вызываем нужную OpenGL функцию
        if constexpr (std::is_same_v<T, double>)        glUniform1d(loc, val);
        else if constexpr (std::is_same_v<T, float>)    glUniform1f(loc, (float)val);
        else if constexpr (std::is_same_v<T, uint32_t>) glUniform1ui(loc, (uint32_t)val);
        else if constexpr (std::is_same_v<T, int32_t>)  glUniform1i(loc, (int)val);
    }
    // Рекурсивный вызов для следующей пары аргументов
    _set_unif(prog, args...); 
}
void bindSSBO(GLuint ssbo, int binding) {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, ssbo);
}
void clearBind(int binding) {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, 0);
}
template<typename... Args>
void gpuRun(unsigned int prog, unsigned int count, unsigned int sizeg, Args... uniforms) {
    if (prog == 0) return;

    glUseProgram(prog);

    _set_unif(prog, uniforms...);

    // Запуск. count — общее число клеток
    glDispatchCompute((count + sizeg-1) / sizeg, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    checkGL("gpuRun (Double SSBO Dispatch)");

    // Очистка состояний
    glUseProgram(0);
    glFinish();
}
template<typename... Args>
void gpuRun2d(unsigned int prog, unsigned int countx, unsigned int county, unsigned int sizegx, unsigned int sizegy, Args... uniforms) {
    if (prog == 0) return;

    glUseProgram(prog);
    _set_unif(prog, uniforms...);
    glDispatchCompute((countx + sizegx-1) / sizegx, (county + sizegy-1) / sizegy, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    checkGL("gpuRun (Double SSBO Dispatch)");
    glUseProgram(0);
    glFinish();
}
template <typename T>
class SSBO {
private:
    GLuint ssbo = 0;
    uint32_t _size = 0;
public:
    SSBO() = default;
    ~SSBO() {
        clear();
    }
    void clear() {
        if (ssbo!=0) {
            deleteSSBO(ssbo);
            ssbo = 0;
            _size = 0;
        }
    }
    void resize(size_t size) {
        if (_size!=size) {
            if (ssbo!=0) {
                this->clear();
            }
            if (size!=0) {
                ssbo = createEmptySSBO(sizeof(T)*size);
                _size = size;
            }
        }
    }
    void update(const std::vector<T>& data) {
        if (_size!=data.size()) [[unlikely]]{
            std::cerr << "no need size" << std::endl;
            exit(1);
        }
        if (ssbo!=0) {
            updateSSBOData(ssbo, data);
        }
    }
    void download(std::vector<T>& data) {
        if (_size!=data.size()) [[unlikely]]{
            std::cerr << "no need size" << std::endl;
            exit(1);
        }
        if (ssbo!=0) {
            downloadSSBOData(ssbo, data);
        }
    }
    // не обязательно должен скачать всё в отличие от downloaf
    void downloadNA(std::vector<T>& data) {
        if (_size<data.size()) [[unlikely]]{
            std::cerr << "no need size" << std::endl;
            exit(1);
        }
        if (ssbo!=0) {
            downloadSSBOData(ssbo, data);
        }
    }
    void bind(int num) {
        bindSSBO(ssbo, num);
    }
    void unbind(int num) {
        clearBind(num);
    }
};
