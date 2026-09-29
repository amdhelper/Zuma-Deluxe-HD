cmake_minimum_required(VERSION 3.15)
project(
    ZumaHD
    LANGUAGES C
    VERSION 0.1.0
)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(isBassFound)
set(isBassFxFound)

find_library (isBassFound bass NO_CACHE PATHS "${PROJECT_SOURCE_DIR}/lib/bass")
message(STATUS ${isBassFound})
if (${isBassFound} STREQUAL "isBassFound-NOTFOUND")
    message(STATUS "Downloading bass...")

    set(bassLibBaseUrl "https://www.un4seen.com/files")
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
        set(bassLibTag "bass24-osx")
    elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        set(bassLibTag "bass24")
    elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        set(bassLibTag "bass24-linux")
    else ()
        message(FATAL_ERROR "Unsupported OS. Download BASS LIB manually")
    endif()

    file(DOWNLOAD "${bassLibBaseUrl}/${bassLibTag}.zip" "${PROJECT_SOURCE_DIR}/lib/bass.zip" SHOW_PROGRESS)
    file(ARCHIVE_EXTRACT INPUT "${PROJECT_SOURCE_DIR}/lib/bass.zip" DESTINATION "${PROJECT_SOURCE_DIR}/lib/bass")
    find_library (isBassFound bass NO_CACHE PATHS "${PROJECT_SOURCE_DIR}/lib/bass")
endif()


find_library (isBassFxFound bass_fx NO_CACHE PATHS "${PROJECT_SOURCE_DIR}/lib/bass-fx")
if (${isBassFxFound} STREQUAL isBassFxFound-NOTFOUND)
    message(STATUS "Downloading bass fx...")

    set(bassLibBaseUrl "https://www.un4seen.com/files/z/0")
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
        set(bassLibTag "bass_fx24-osx")
    elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        set(bassLibTag "bass_fx24")
    elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        set(bassLibTag "bass_fx24-linux")
    else ()
        message(FATAL_ERROR "Unsupported OS. Download BASS FX LIB manually")
    endif()

    file(DOWNLOAD "${bassLibBaseUrl}/${bassLibTag}.zip" "${PROJECT_SOURCE_DIR}/lib/bass-fx.zip" SHOW_PROGRESS)
    file(ARCHIVE_EXTRACT INPUT "${PROJECT_SOURCE_DIR}/lib/bass-fx.zip" DESTINATION "${PROJECT_SOURCE_DIR}/lib/bass-fx")
    find_library (isBassFxFound bass_fx NO_CACHE PATHS "${PROJECT_SOURCE_DIR}/lib/bass-fx")
endif()

find_package(EXPAT REQUIRED)

# SDL2 / SDL2_image：优先 CMake config（MSYS2/Homebrew 都有），找不到就退回 pkg-config。
# ⚠️ Ubuntu 22.04 的 libsdl2-image-dev 不保证带 CMake config → CI 里 find_package 会失败。
find_package(SDL2 QUIET)
if (NOT SDL2_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2_PC REQUIRED IMPORTED_TARGET sdl2)
    add_library(SDL2::SDL2 ALIAS PkgConfig::SDL2_PC)
    message(STATUS "SDL2: using pkg-config (no CMake config found)")
endif()

find_package(SDL2_image QUIET)
if (NOT SDL2_image_FOUND)
    if (NOT PkgConfig_FOUND)
        find_package(PkgConfig REQUIRED)
    endif()
    pkg_check_modules(SDL2_IMAGE_PC REQUIRED IMPORTED_TARGET SDL2_image)
    add_library(SDL2_image::SDL2_image ALIAS PkgConfig::SDL2_IMAGE_PC)
    message(STATUS "SDL2_image: using pkg-config (no CMake config found)")
endif()

# SDL_ttf：优先用仓库内的 vendored 副本（external/SDL_ttf 是 gitlink，干净克隆里可能是空的），
# 没有就退回系统库（发行版打包/CI 用 libsdl2-ttf-dev）。
# ⚠️ 代码用到 TTF_SetFontWrappedAlign（SDL_ttf >= 2.20），旧系统库会在编译期报错，
#    源码里已用 SDL_TTF_VERSION_ATLEAST 兜底（见 HQC_Artist.c）。
if (EXISTS "${PROJECT_SOURCE_DIR}/external/SDL_ttf/CMakeLists.txt")
    message(STATUS "SDL_ttf: using vendored copy (external/SDL_ttf)")
    add_subdirectory(external/SDL_ttf)
    set(SDL2_TTF_TARGET SDL2_ttf)
    set(SDL2_TTF_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/external/SDL_ttf")
else()
    message(STATUS "SDL_ttf: vendored copy missing, falling back to system library")
    find_library(SDL2_TTF_LIBRARY NAMES SDL2_ttf REQUIRED)
    find_path(SDL2_TTF_INCLUDE_DIRS NAMES SDL_ttf.h PATH_SUFFIXES SDL2)
    set(SDL2_TTF_TARGET ${SDL2_TTF_LIBRARY})
endif()

FILE(GLOB_RECURSE SOURCES RELATIVE ${PROJECT_SOURCE_DIR} "src/*.c")

set(BASS_LIB "${PROJECT_SOURCE_DIR}/lib/bass/libs/x86_64/libbass.so")
set(BASS_FX_LIB "${PROJECT_SOURCE_DIR}/lib/bass-fx/libs/x86_64/libbass_fx.so")

add_executable(
    ZumaHD
    ${SOURCES}
        src/global/HQC_Key.h
        src/zuma/FloatingText.h
        src/zuma/FloatingText.c
        src/zuma/Statistics.c
        src/zuma/Statistics.h
        src/zuma/LevelMgr.c
        src/zuma/LevelMgr.h
        src/zuma/Progress.c
        src/zuma/Progress.h
)

target_include_directories(
    ZumaHD PUBLIC
    "${PROJECT_BINARY_DIR}"
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/lib/bass/c"
    "${PROJECT_SOURCE_DIR}/lib/bass-fx/C"

    ${EXPAT_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
)

target_link_libraries(
    ZumaHD PRIVATE
    SDL2::SDL2
    SDL2_image::SDL2_image
    ${SDL2_TTF_TARGET}

    ${BASS_LIB}
    ${BASS_FX_LIB}
    
    EXPAT::EXPAT
    m
)

# Copy DLLs needed for runtime on Windows
if (WIN32)
    if(BUILD_SHARED_LIBS)
        add_custom_command(TARGET ZumaHD POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PROJECT_SOURCE_DIR}/lib/bass/x64/bass.dll
            ${PROJECT_SOURCE_DIR}/lib/bass-fx/x64/bass_fx.dll
            $<TARGET_FILE_DIR:ZumaHD>)
    endif()
endif()

add_custom_command(TARGET ${PROJECT_NAME}  POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/content
                ${CMAKE_CURRENT_BINARY_DIR}/bin)
