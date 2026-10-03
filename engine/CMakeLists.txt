cmake_minimum_required(VERSION 3.14)
project(qr_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POLICY_DEFAULT_CMP0004 NEW)

# No OpenGL / GLFW / ImGui / glm here on purpose: this library is the portable
# generation core shared by the desktop app and the FastAPI backend.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(CTest)

# ── QR Code generator (Nayuki) ───────────────────────────────────────
# qrcodegen is the only external QR dependency. Reuse the copy the desktop
# app already downloaded when present, otherwise fetch it.
set(QRCODEGEN_LOCAL_CANDIDATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../3DQRGenerator/build/_deps/qrcodegen-src/cpp")

if(EXISTS "${QRCODEGEN_LOCAL_CANDIDATE}/qrcodegen.cpp")
    message(STATUS "qr_engine: using local qrcodegen from ${QRCODEGEN_LOCAL_CANDIDATE}")
    set(QRCODEGEN_DIR "${QRCODEGEN_LOCAL_CANDIDATE}")
else()
    include(FetchContent)
    FetchContent_Declare(
        qrcodegen
        URL https://github.com/nayuki/QR-Code-generator/archive/refs/heads/master.zip
    )
    FetchContent_MakeAvailable(qrcodegen)
    set(QRCODEGEN_DIR "${qrcodegen_SOURCE_DIR}/cpp")
endif()

# ── Core generation engine ──────────────────────────────────────────
add_library(qr_engine_core STATIC
    src/QRGenerator.cpp        # QR bit-matrix  (unchanged from desktop app)
    src/MeshBuilder.cpp        # 3D mesh        (unchanged from desktop app)
    src/STLWriter.cpp          # binary STL     (unchanged from desktop app)
    src/PNGWriter.cpp          # QR PNG         (unchanged from desktop app)
    src/PayloadBuilder.cpp     # QR payload     (extracted from App.cpp)
    src/Validation.cpp         # validation     (extracted from App.cpp)
    src/OutputPaths.cpp        # path rules     (extracted from App.cpp)
    src/EngineService.cpp      # orchestration  (extracted from App.cpp)
    src/Json.cpp               # dependency-free JSON for the CLI boundary
    "${QRCODEGEN_DIR}/qrcodegen.cpp"
)

target_include_directories(qr_engine_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    "${QRCODEGEN_DIR}"
)

target_link_libraries(qr_engine_core PRIVATE z)

if(APPLE)
    find_library(ZLIB_SYSTEM_LIB z)
    if(ZLIB_SYSTEM_LIB)
        target_link_libraries(qr_engine_core PUBLIC ${ZLIB_SYSTEM_LIB})
    endif()
endif()

# ── CLI / service binary used by the FastAPI backend ────────────────
add_executable(qr_engine app/EngineMain.cpp)
target_link_libraries(qr_engine PRIVATE qr_engine_core)

# ── Self tests ──────────────────────────────────────────────────────
if(BUILD_TESTING)
    add_executable(engine_tests tests/engine_tests.cpp)
    target_link_libraries(engine_tests PRIVATE qr_engine_core)
    add_test(NAME engine_tests COMMAND engine_tests)
endif()
