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
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)

add_subdirectory(external/SDL_ttf)

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
        src/zuma/ecs/World.h
        src/zuma/ecs/System.h
        src/zuma/ecs/World.c
        src/zuma/components/PositionComponent.h
        src/zuma/entities/FrogEntity.c
        src/zuma/systems/SpriteDrawSystem.h
        src/zuma/systems/SpriteDrawSystem.c
        src/zuma/ecs/Entity.h
        src/zuma/systems/HudSystem.c
        src/zuma/components/FrogComponent.h
        src/zuma/components/ComponentID.h
        src/zuma/components/HudComponent.h
        src/zuma/systems/FrogSystem.c
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
    "external/SDL_ttf"
)

target_link_libraries(
    ZumaHD PRIVATE
    SDL2::SDL2
    SDL2_image::SDL2_image
    SDL2_ttf

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
