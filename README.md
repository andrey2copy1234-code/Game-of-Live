# Game-of-Live
репозиторий клеточного автомата игры в жизнь. имеет версии для cpu и gpu. имеет возможности отдаления и приближения в точку.
## Управление
Нажать на клетку - сделать её живой/неживой
Колёсико мыши - приближение/отдаление из точки
Жест приблежения на тачпаде - приближение/отдаление из точки
## Горячии клавиши
E - отдалить
Q - приблизить
S - перерегенерировать поле с новым размером
R - сгенерировать рандомизированное поле
F - увеличить SPS (симуляции в секунду)
Alt-E - очистить поле
Shift-s - остановить симуляцию
## Отображение
Сверху название игры, FPS, SPS, кол-во нарисованых клеток (сколько на экране)
## Компиляция
### Windows
Сначало файла есть закоментированный `#define CPU_MODE`. по умолчанию код компилируется на видиокарту но можно запустить компиляцию на процесор и для этого надо раскоментировать строку `#define CPU_MODE`. Весь код и так и так компилируется частично на видиокарте. это из-за пакета sfml.
Рекомендуется использовать -O3 для компиляции и компилятор g++ 15.1.0+.
### Android
Для компиляции на android вам необходимо установить Android Studio. создать проект Native C++. вставить в native-lib.cpp код game_of_live.cpp. установить SDL3 с github (версия 3.2) и SDL_ttf (версия 3.2). устанавлевать их в папку main/cpp/sdl3 и SDL3 устанавливать в main/cpp/sdl3/sdl3 а SDL_ttf в main/cpp/sdl3/ttf. потом вставить с CMakeLists.txt основной: ```txt cmake_minimum_required(VERSION 3.12)
project("gameoflive")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SDL3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sdl3/sdl3")
set(SDL3_TTF_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sdl3/ttf")

set(SDL3TTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDL3TTF_HARFBUZZ ON CACHE BOOL "" FORCE)
set(SDL3TTF_PLUTOVG ON CACHE BOOL "" FORCE)
set(SDL3TTF_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL3_INSTALL OFF CACHE BOOL "" FORCE)

set(PLUTOSVG_BUILD_PLUTOVG OFF CACHE BOOL "" FORCE)
set(PLUTOVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

set(FREETYPE_INCLUDE_DIRS "${SDL3_TTF_DIR}/external/freetype/include" CACHE PATH "" FORCE)
set(FREETYPE_INCLUDE_DIR_ft2build "${SDL3_TTF_DIR}/external/freetype/include" CACHE PATH "" FORCE)
set(FREETYPE_INCLUDE_DIR_freetype2 "${SDL3_TTF_DIR}/external/freetype/include" CACHE PATH "" FORCE)

set(Freetype_FOUND TRUE CACHE BOOL "" FORCE)
set(FREETYPE_FOUND TRUE CACHE BOOL "" FORCE)
set(HARFBUZZ_CAN_USE_FREETYPE ON CACHE BOOL "" FORCE)
set(HB_HAVE_FREETYPE ON CACHE BOOL "" FORCE)

if(NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype INTERFACE IMPORTED)
    set_target_properties(Freetype::Freetype PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${FREETYPE_INCLUDE_DIRS}"
    )
endif()
set(SDL3TTF_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL3_INSTALL OFF CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
set(SKIP_INSTALL_LIBRARIES ON CACHE BOOL "" FORCE)
set(SKIP_INSTALL_EXECUTABLES ON CACHE BOOL "" FORCE)
set(SKIP_INSTALL_FILES ON CACHE BOOL "" FORCE)

set(PLUTOVG_INSTALL OFF CACHE BOOL "" FORCE)
set(PLUTOSVG_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(${SDL3_DIR} sdl3_core)


add_subdirectory(${SDL3_TTF_DIR} sdl3_ttf)


add_library(gameoflive SHARED native-lib.cpp)

target_include_directories(gameoflive PRIVATE
    "${SDL3_DIR}/include"
    "${SDL3_TTF_DIR}/include"
)

find_library(log-lib log)
find_library(gles-lib GLESv3)

target_link_libraries(gameoflive
    SDL3-shared
    SDL3_ttf-shared
    GLESv3
    ${log-lib}
    android
)
```
Потом с github установить файл stb_image.h и вставить в main/cpp/stb_image.h. Потом разкоментировать макрос ANDROID_MODE.
Дальше нужно в папку main/java вставить папку main/cpp/sdl3/sdl3/android-project/app/src/main/java/org. Дальше в main/java/com.example.gameoflive/MainActivity.java вставить код: ```java package com.example.gameoflive;

import org.libsdl.app.SDLActivity;

/**
 * MainActivity теперь просто "запускалка" для движка SDL3.
 */
public class MainActivity extends SDLActivity {

    /**
     * Здесь мы перечисляем библиотеки, которые должны быть загружены.
     * SDLActivity сам вызовет System.loadLibrary для каждой из них.
     */
    @Override
    protected String[] getLibraries() {
        return new String[] {
                "SDL3",
                "SDL3_ttf",
                "gameoflive"
        };
    }
}
```
в папке main/cpp/sdl3/ttf/extension добавить freetype, harfbuzz, plutosvg, plutovg (всё скачивать с github с версиями указаными в файле main/cpp/sdl3/ttf/.gitmodules). потом удалить всё находящиеся в папке main/cpp/sdl3/ttf/examples.
Дальше нужно в Android Studio нажать на 3 полоски сверху (в левом верхнем углу чуть правее самого значка Android Studio). потом навестись на Build. потом навестись на Generate App Bundles or APKs. потом нажать на Generate APKs. подождать пока вылезут ошибки.
Потом зная лог ошибок зайти в main/cpp/sdl3/ttf/external и в нужные библиотеки. там в файле CMakeLists.txt удалить строки начинающиеся с install и export.
Дальше нужно в Android Studio нажать на 3 полоски сверху (в левом верхнем углу чуть правее самого значка Android Studio). потом навестись на Build. потом навестись на Generate App Bundles or APKs. потом нажать на Generate APKs. Забрать apk в app/build/outputs/apk/debug/app-debug.apk. 
Если надо перекинуть на телефон быстро то заходим в настройки. кликаем "О телефоне". нажимаем 7 раз на версию. появится надпись "Вы стали разработчиком!". Потом перейти в расширеные настройки. там в конце нажать на "Для разработчиков". потом нажать "Отладка по USB" (в низ сильно мотать не надо). высветится то что типа это опасно через 10 секунд нажимаем "Ок" и то что вы соглашаетесь с условиями. дальше возьми USB подключи компьютер к телефону через USB. потом нажми Terminal в Android Studio (находится слева снизу там на полоске вертикальной вверх чутка). и туда ввести `& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" push "app/build/outputs/apk/debug/app-debug.apk" /sdcard/Download/my_game.apk` потом можно зайти в загрузки там будет файл на телефоне my_game.apk. нажатием на его запустится установка.
Если приложение вытыкает то сделаете всё что написано в прошлом обзатце но после включения "Отладки по USB" надо нажать Logcat (находится примерно там же где и Terminal). перезапустить приложение на телефоне. в Logcat будет вся информация.
