cmake_minimum_required(VERSION 3.18.1)
project(sandstrike C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# raylib 5.5 (исходники) лежат в папке raylib/ рядом с этим файлом.
# Скачать:  git clone --depth 1 --branch 5.5 https://github.com/raysan5/raylib.git app/src/main/cpp/raylib
set(RAYLIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/raylib)
if(NOT EXISTS ${RAYLIB_DIR}/src/raylib.h)
    message(FATAL_ERROR "Не найден raylib. Выполните: git clone --depth 1 --branch 5.5 https://github.com/raysan5/raylib.git app/src/main/cpp/raylib")
endif()

set(GLUE_DIR ${CMAKE_ANDROID_NDK}/sources/android/native_app_glue)
set(RS ${RAYLIB_DIR}/src)

add_library(raylib STATIC
    ${RS}/rcore.c ${RS}/rshapes.c ${RS}/rtextures.c ${RS}/rtext.c
    ${RS}/rmodels.c ${RS}/raudio.c ${RS}/utils.c)
target_compile_definitions(raylib PUBLIC PLATFORM_ANDROID GRAPHICS_API_OPENGL_ES2)
target_include_directories(raylib PUBLIC ${RS} ${RS}/external ${GLUE_DIR})
target_compile_options(raylib PRIVATE -ffunction-sections -fdata-sections -Wno-everything)

add_library(sandstrike SHARED main.cpp ${GLUE_DIR}/android_native_app_glue.c)
target_include_directories(sandstrike PRIVATE ${RS} ${GLUE_DIR})
target_compile_options(sandstrike PRIVATE -O2 -Wno-everything)
target_link_libraries(sandstrike raylib log android EGL GLESv2 OpenSLES atomic m dl c)
set_target_properties(sandstrike PROPERTIES
    LINK_FLAGS "-u ANativeActivity_onCreate -Wl,-z,max-page-size=16384")
