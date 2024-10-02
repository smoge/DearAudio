cmake_minimum_required(VERSION 3.10)
project(DearAudio VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Option for build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()

# Set the path to your Dear ImGui directory
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
set(IMPLOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/implot)

# Find GLFW
find_package(glfw3 REQUIRED)

# Find JACK
find_package(PkgConfig REQUIRED)
pkg_check_modules(JACK REQUIRED jack)

# Set OpenGL preference to GLVND
if(POLICY CMP0072)
  cmake_policy(SET CMP0072 NEW)
endif()
set(OpenGL_GL_PREFERENCE GLVND)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Manually linking FFTW since find_package(FFTW) did not work
set(FFTW_LIB /usr/lib64/libfftw3f.so)
find_library(FFTW_LIB NAMES fftw3f PATHS /usr/lib64)

# Source files
set(SOURCES
    main.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_demo.cpp
    ${IMPLOT_DIR}/implot_items.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMPLOT_DIR}
    ${JACK_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    OpenGL::GL
    ${JACK_LIBRARIES}
    ${FFTW_LIB}
)

# Platform-specific settings
if(UNIX AND NOT APPLE)
    # Linux
    target_link_libraries(${PROJECT_NAME} PRIVATE dl)
elseif(APPLE)
    # macOS
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
elseif(WIN32)
    # Windows
    target_link_libraries(${PROJECT_NAME} PRIVATE
        gdi32
        imm32
    )
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wformat)
endif()

# Debug symbols
target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:-g>)

# Install rules
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Enable testing
enable_testing()
