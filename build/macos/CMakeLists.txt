
# Set the minimum version of CMake that can be used
# To find the cmake version run
# $ cmake --version
cmake_minimum_required(VERSION 3.4)

# Set the project name
project(respitarion)
set(CMAKE_VERBOSE_MAKEFILE TRUE)

# set the C++ standard to C++ 17
# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_BUILD_TYPE DEBUG)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")

# Check some variables 
message (STATUS "==============================================================================")
get_filename_component(SRC_ROOT_DIR "../.." ABSOLUTE)
message(STATUS "<-- APPLE => APPLE -- >")
message(STATUS "Project root dir: ${SRC_ROOT_DIR}")

# Move Prerequesite Files


#SDL 
find_package(SDL2 REQUIRED)


message (STATUS "==============================================================================")
get_cmake_property(_variableNames VARIABLES)
list (SORT _variableNames)
foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()
set(OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
message (STATUS "==============================================================================")
message(STATUS "SDL2 Include Directories ${SDL2_INCLUDE_DIRS}")
message(STATUS "SDL2 Library Directories ${SDL2_LIBDIR}")

message(STATUS "==============================================================================")
message(STATUS "Dictionary where buiilding is taking place: ${OUTPUT_DIR}")

#Ok headers
set(APP_HEADERS

        ${SRC_ROOT_DIR}/src/inc        
        ${SRC_ROOT_DIR}/src/exengine        
        ${SRC_ROOT_DIR}/src/lib/physfs
        ${SRC_ROOT_DIR}/src/lib/MojoAL
        ${SRC_ROOT_DIR}/src/exengine/render/legacy

        ${SDL2_INCLUDE_DIRS}/
        ${SDL2_INCLUDE_DIRS}/../
        )

message(STATUS "APP HEADERS: ${APP_HEADERS}")
message (STATUS "==============================================================================")


file(GLOB APPLICATION_SOURCES 
    ${SRC_ROOT_DIR}/src/*.c
    ${SRC_ROOT_DIR}/src/exengine/*.c
    ${SRC_ROOT_DIR}/src/lib/physfs/*.c
    ${SRC_ROOT_DIR}/src/lib/MojoAL/*.c
    ${SRC_ROOT_DIR}/src/exengine/render/*.c
    ${SRC_ROOT_DIR}/src/exengine/util/*.c
    ${SRC_ROOT_DIR}/src/exengine/math/*.c
    ${SRC_ROOT_DIR}/src/exengine/input/*.c
    ${SRC_ROOT_DIR}/src/exengine/sound/sound.c
    )

if (APPLE)
    set (APPLICATION_SOURCES "${APPLICATION_SOURCES};${SRC_ROOT_DIR}/src/lib/physfs/physfs_platform_apple.m")
endif()
message(STATUS "Files are:${APPLICATION_SOURCES} ")

add_executable(game ${APPLICATION_SOURCES})

include_directories(${APP_HEADERS})

target_link_libraries(game PUBLIC ${SDL2_LIBDIR}/libSDL2.a)
if (APPLE)
    target_link_libraries(game PUBLIC
            "-framework OpenGl"
            "-framework Foundation"
            "-framework IOKit"
            "-framework CoreAudio"
            "-framework AudioToolBox"
            "-framework CoreGraphics"
            "-framework CoreVideo"
            "-framework ForceFeedback"
            "-framework Carbon"
            "-framework Metal"
            "-framework AppKit"
            -liconv
            -lstdc++
            
            )
endif()
#Copy resources to build directory:
add_custom_command(
        TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${SRC_ROOT_DIR}/misc/resources/*
                ${OUTPUT_DIR}/)


