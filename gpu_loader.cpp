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
#include <iostream>

// 1. Функция для проверки ошибок OpenGL после команд
void checkGL(const char* label) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cout << "ОШИБКА OpenGL [" << label << "]: 0x" << std::hex << err << std::dec << std::endl;
        exit(1);
    }
}

GLuint compileShader(const char* shaderSource) {
    // 1. Создаем шейдер
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    
    // ДОБАВЬ ЭТОТ ВЫВОД:
    std::cout << "DEBUG: Попытка создания Compute Shader. ID: " << shader << std::endl;
    
    if (shader == 0) {
        std::cout << "ОШИБКА: Видеокарта вернула 0. Compute Shaders не поддерживаются!" << std::endl;
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
        std::cout << "ОШИБКА КОМПИЛЯЦИИ ШЕЙДЕРА:\n" << infoLog << std::endl;
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
        std::cout << "ОШИБКА ЛИНКОВКИ ПРОГРАММЫ:\n" << infoLog << std::endl;
        return 0;
    }
    
    std::cout << "Шейдер успешно собран! ID: " << program << std::endl;
    return program;
}

GLuint createEmptySSBO(size_t sizeInBytes) {
    if (sizeInBytes == 0) return 0;

    // 1. ПОЛНАЯ РАЗБЛОКИРОВКА перед созданием
    glUseProgram(0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0); // GL_SHADER_STORAGE_BUFFER, слот 0, NULL
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); 
    glFinish();
    GLuint ssbo;
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo); 
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeInBytes, NULL, 0x88E0); 
    checkGL("createEmptySSBO (BufferData)");
    glBindBuffer(0x90D2, 0); 
    
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
void updateSSBOData(GLuint ssbo, std::vector<T>& data) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, data.size() * sizeof(T), data.data());
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
// Функции для юниформ без изменений, просто добавим одну проверку в gpuRun
template<typename... Args>
void gpuRun(unsigned int prog, unsigned int count, GLuint ssbo, Args... uniforms) {
    if (prog == 0) return; // Не запускаем, если шейдер битый

    glUseProgram(prog);
    glBindBufferBase(0x90D2, 0, ssbo);

    _set_unif(prog, uniforms...);

    glDispatchCompute((count + 63) / 64, 1, 1);
    
    // Барьер памяти и проверка, не "упала" ли видеокарта после вычислений
    glMemoryBarrier(0x00008000); 
    checkGL("gpuRun (Dispatch)");

    glUseProgram(0);
    glBindBufferBase(0x90D2, 0, 0);
    glBindBuffer(0x90D2, 0);
}
template<typename... Args>
void gpuRun2(unsigned int prog, unsigned int count, GLuint ssboIn, GLuint ssboOut, Args... uniforms) {
    if (prog == 0) return;

    glUseProgram(prog);

    // Привязываем ВХОДНОЙ буфер к binding = 0
    glBindBufferBase(0x90D2, 0, ssboIn);
    // Привязываем ВЫХОДНОЙ буфер к binding = 1
    glBindBufferBase(0x90D2, 1, ssboOut);

    _set_unif(prog, uniforms...);

    // Запуск. count — общее число клеток
    glDispatchCompute((count + 63) / 64, 1, 1);
    
    glMemoryBarrier(0x00008000); // GL_SHADER_STORAGE_BARRIER_BIT
    checkGL("gpuRun (Double SSBO Dispatch)");

    // Очистка состояний
    glUseProgram(0);
    glBindBufferBase(0x90D2, 0, 0);
    glBindBufferBase(0x90D2, 1, 0);
}
#else
#include <GLES3/gl32.h>
// #include <android/api-level.h>
// #include <dlfcn.h>

// void showAndroidInfo() {
//     SDL_Log("--- [ANDROID DEBUG INFO START] ---");

//     // 1. Проверка контекста
//     SDL_GLContext currentCtx = SDL_GL_GetCurrentContext();
//     SDL_Log("Context Active: %s", (currentCtx != nullptr ? "YES" : "NO (CRITICAL ERROR)"));

//     if (currentCtx) {
//         // 2. Версии и Драйвер
//         SDL_Log("GL_VERSION:  %s", (const char*)glGetString(GL_VERSION));
//         SDL_Log("GL_VENDOR:   %s", (const char*)glGetString(GL_VENDOR));
//         SDL_Log("GL_RENDERER: %s", (const char*)glGetString(GL_RENDERER));
//         SDL_Log("GL_SL_VER:   %s", (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

//         // 3. Лимиты (важно для SSBO и Compute)
//         GLint maxSSBOSize, maxComputeInvocations;
//         glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &maxSSBOSize);
//         glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxComputeInvocations);
//         SDL_Log("Max SSBO Size: %d bytes", maxSSBOSize);
//         SDL_Log("Max Compute Invocations: %d", maxComputeInvocations);
//     }

//     // 4. Экран и Окно
//     SDL_Window* window = SDL_GL_GetCurrentWindow();
//     if (window) {
//         int w, h, dw, dh;
//         SDL_GetWindowSize(window, &w, &h);
//         SDL_GetWindowSizeInPixels(window, &dw, &dh); // Реальное разрешение в пикселях
//         SDL_Log("Window Size: %dx%d (Pixels: %dx%d)", w, h, dw, dh);
//     }

//     // 5. Система Android
//     SDL_Log("Android API Level: %d", android_get_device_api_level());

//     SDL_Log("--- [ANDROID DEBUG INFO END] ---");
// }
// static void (GL_APIENTRY* glShaderStorageBlockBinding)(GLuint program, GLuint storageBlockIndex, GLuint storageBlockBinding) = nullptr;
// static void (GL_APIENTRY* glBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, const void* data) = nullptr;
void glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void* data) {
    // 1. Отображаем память
    void* mappedPtr = glMapBufferRange(target, offset, size, GL_MAP_READ_BIT);

    if (mappedPtr) {
        // 2. Копируем данные
        memcpy(data, mappedPtr, size);

        // 3. ВАЖНО: Разблокируем буфер, иначе всё сломается на следующем кадре
        glUnmapBuffer(target);
    }
}
//static void (GL_APIENTRY* glShaderStorageBlockBinding)(GLuint program, GLuint storageBlockIndex, GLuint storageBlockBinding) = nullptr;
// static void (GL_APIENTRY* glGetBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, void* data) = nullptr;
void loadGPUFunctions() {
    //glShaderStorageBlockBinding = (void(GL_APIENTRY*)(GLuint, GLuint, GLuint))SDL_GL_GetProcAddress("glShaderStorageBlockBinding");
    //glGetBufferSubData = (void(GL_APIENTRY*)(GLenum, GLintptr, GLsizeiptr, void*))SDL_GL_GetProcAddress("glGetBufferSubData");
    // glShaderStorageBlockBinding = (void(GL_APIENTRY*)(GLuint, GLuint, GLuint))SDL_GL_GetProcAddress("glShaderStorageBlockBinding");
    // //glBufferSubData = (void(GL_APIENTRY*)(GLenum, GLintptr, GLsizeiptr, const void*))SDL_GL_GetProcAddress("glBufferSubData");

    // if (!glShaderStorageBlockBinding || !glBufferSubData) {
    //     SDL_Log("ОШИБКА: Не удалось загрузить функции GLES из драйвера!");
    // }
}
#include <iostream>

// 1. Функция для проверки ошибок OpenGL после команд
void checkGL(const char* label) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        SDL_Log("ОШИБКА OpenGL [%s]: 0x%x", label, err);
        exit(1);
    }
}

GLuint compileShader(const char* shaderSource) {
    // 1. Создаем шейдер
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);

    // ДОБАВЬ ЭТОТ ВЫВОД:
    SDL_Log("DEBUG: Попытка создания Compute Shader. ID: %u", shader);

    if (shader == 0) {
        //std::cout << "ОШИБКА: Видеокарта вернула 0. Compute Shaders не поддерживаются!" << std::endl;
        SDL_Log("ОШИБКА: Видеокарта вернула 0. Compute Shaders не поддерживаются!");
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
        SDL_Log("ОШИБКА КОМПИЛЯЦИИ ШЕЙДЕРА:\n%s", infoLog);
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
        //std::cout << "ОШИБКА ЛИНКОВКИ ПРОГРАММЫ:\n" << infoLog << std::endl;
        SDL_Log("ОШИБКА ЛИНКОВКИ ПРОГРАММЫ:\n%s", infoLog);
        return 0;
    }
    SDL_Log("Шейдер успешно собран! ID: %u", program);
    std::cout << "Шейдер успешно собран! ID: " << program << std::endl;
    return program;
}

GLuint createEmptySSBO(size_t sizeInBytes) {
    if (sizeInBytes == 0) return 0;

    // 1. ПОЛНАЯ РАЗБЛОКИРОВКА перед созданием
    glUseProgram(0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0); // GL_SHADER_STORAGE_BUFFER, слот 0, NULL
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); 
    glFinish();
    GLuint ssbo;
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo); 
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeInBytes, NULL, 0x88E0); 
    checkGL("createEmptySSBO (BufferData)");
    glBindBuffer(0x90D2, 0); 
    
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
void updateSSBOData(GLuint ssbo, std::vector<T>& data) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, data.size() * sizeof(T), data.data());
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
// Функции для юниформ без изменений, просто добавим одну проверку в gpuRun
template<typename... Args>
void gpuRun(unsigned int prog, unsigned int count, GLuint ssbo, Args... uniforms) {
    if (prog == 0) return; // Не запускаем, если шейдер битый

    glUseProgram(prog);
    glBindBufferBase(0x90D2, 0, ssbo);

    _set_unif(prog, uniforms...);

    glDispatchCompute((count + 63) / 64, 1, 1);

    // Барьер памяти и проверка, не "упала" ли видеокарта после вычислений
    glMemoryBarrier(0x00008000);
    checkGL("gpuRun (Dispatch)");

    glUseProgram(0);
    glBindBufferBase(0x90D2, 0, 0);
    glBindBuffer(0x90D2, 0);
}
template<typename... Args>
void gpuRun2(unsigned int prog, unsigned int count, GLuint ssboIn, GLuint ssboOut, Args... uniforms) {
    if (prog == 0) return;

    glUseProgram(prog);

    // Привязываем ВХОДНОЙ буфер к binding = 0
    glBindBufferBase(0x90D2, 0, ssboIn);
    // Привязываем ВЫХОДНОЙ буфер к binding = 1
    glBindBufferBase(0x90D2, 1, ssboOut);

    _set_unif(prog, uniforms...);

    // Запуск. count — общее число клеток
    glDispatchCompute((count + 63) / 64, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT); // GL_SHADER_STORAGE_BARRIER_BIT
    checkGL("gpuRun (Double SSBO Dispatch)");

    // Очистка состояний
    glUseProgram(0);
    glBindBufferBase(0x90D2, 0, 0);
    glBindBufferBase(0x90D2, 1, 0);
}
#endif
template<typename... Args>
void gpuRun22d(unsigned int prog, unsigned int countx, unsigned int county, GLuint ssboIn, GLuint ssboOut, Args... uniforms) {
    if (prog == 0) return;

    glUseProgram(prog);

    // Привязываем ВХОДНОЙ буфер к binding = 0
    glBindBufferBase(0x90D2, 0, ssboIn);
    // Привязываем ВЫХОДНОЙ буфер к binding = 1
    glBindBufferBase(0x90D2, 1, ssboOut);

    _set_unif(prog, uniforms...);

    glDispatchCompute((countx + 15) / 16, (county + 15) / 16, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT); // GL_SHADER_STORAGE_BARRIER_BIT
    checkGL("gpuRun (Double SSBO Dispatch)");

    // Очистка состояний
    glUseProgram(0);
    glBindBufferBase(0x90D2, 0, 0);
    glBindBufferBase(0x90D2, 1, 0);
}
