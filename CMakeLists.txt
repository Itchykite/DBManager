cmake_minimum_required(VERSION 3.22)
project(RestauracjaDB VERSION 0.1 LANGUAGES C CXX)

# --- Kompilator ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

# --- Ścieżki do nagłówków ---
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
include_directories(${INCLUDE_DIR})

# --- Pliki źródłowe aplikacji ---
set(APP_SOURCES
    src/main.cpp
    src/App.cpp
    src/TableSelectionBar.cpp
    src/TableOperations.cpp
)

# --- Pliki ImGui ---
set(IMGUI_DIR "${INCLUDE_DIR}/imgui")
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# --- GLAD ---
set(GLAD_DIR "${INCLUDE_DIR}/glad")
add_library(glad STATIC "${GLAD_DIR}/src/glad.c")
target_include_directories(glad PUBLIC "${GLAD_DIR}/include")

# --- Tworzymy executable ---
add_executable(${PROJECT_NAME} ${APP_SOURCES} ${IMGUI_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${IMGUI_DIR} ${IMGUI_DIR}/backends)

# --- GLFW ---
find_package(glfw3 3.3 REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

# --- OpenGL ---
find_package(OpenGL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL glad)

# --- X11 (Linux) ---
if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_include_directories(${PROJECT_NAME} PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${X11_LIBRARIES})
endif()

# --- MariaDB C++ Connector ---
# Ustaw ścieżkę do lokalnego build MariaDB Connector
set(MARIADB_ROOT "$ENV{HOME}/Downloads/mariadb-connector-cpp/build")
set(MARIADB_INCLUDE_DIR "${MARIADB_ROOT}/include")
set(MARIADB_LIB "${MARIADB_ROOT}/libmariadbcpp.so")  # lub .a jeśli chcesz statycznie

if(EXISTS ${MARIADB_LIB} AND IS_DIRECTORY ${MARIADB_INCLUDE_DIR})
    message(STATUS "Znaleziono MariaDB C++ Connector: ${MARIADB_LIB}")
    target_include_directories(${PROJECT_NAME} PRIVATE ${MARIADB_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${MARIADB_LIB})
else()
    message(FATAL_ERROR "Nie znaleziono MariaDB C++ Connector. Zainstaluj go i ustaw MARIADB_ROOT")
endif()

# --- Linux specyficzne ---
if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE dl pthread)
endif()

