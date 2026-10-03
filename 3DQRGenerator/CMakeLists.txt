cmake_minimum_required(VERSION 3.14)
project(3DQRGenerator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POLICY_DEFAULT_CMP0004 NEW)

# Configure output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Reuse the shared generation engine (QR / 3D mesh / STL / PNG / validation).
# It contains no OpenGL, GLFW, ImGui or glm code, so this desktop app and the
# FastAPI backend link the exact same algorithms.
add_subdirectory(../engine ${CMAKE_BINARY_DIR}/engine)

# Fetch dependencies
include(FetchContent)

# 1. GLFW
FetchContent_Declare(
    glfw
    URL https://github.com/glfw/glfw/archive/refs/tags/3.3.8.zip
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# 2. GLM
FetchContent_Declare(
    glm
    URL https://github.com/g-truc/glm/archive/refs/heads/master.zip
)
FetchContent_MakeAvailable(glm)

# 3. ImGui
FetchContent_Declare(
    imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.90.zip
)
FetchContent_MakeAvailable(imgui)

# 4. QR Code Generator (Nayuki) is provided by the engine subdirectory above.
#    It is not re-fetched here on purpose: one copy of the QR algorithm only.

# Find OpenGL
find_package(OpenGL REQUIRED)

# Add main executable
# NOTE: QRGenerator.cpp / MeshBuilder.cpp / STLWriter.cpp / PNGWriter.cpp now
# live in ../engine and are compiled into qr_engine_core. They are deliberately
# not repeated here so the desktop app and the web app cannot drift apart.
set(SOURCES
    src/main.cpp
    src/App.cpp
    src/Camera.cpp
    src/Renderer.cpp
)

# ImGui sources
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

add_executable(3DQRGenerator 
    ${SOURCES} 
    ${IMGUI_SOURCES}
)

# Includes
target_include_directories(3DQRGenerator PRIVATE
    include
    ${glfw_SOURCE_DIR}/include
    ${glm_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# Linking
target_link_libraries(3DQRGenerator PRIVATE
    qr_engine_core   # QR / 3D mesh / STL / PNG / validation — shared with the web backend
    glfw
    glm::glm
    OpenGL::GL
    z    # system zlib for PNGWriter
)

if (APPLE)
    target_link_libraries(3DQRGenerator PRIVATE "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()
