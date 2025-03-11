cmake_minimum_required(VERSION 3.14)
project(DeepSeekChat)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

execute_process(
    COMMAND git rev-parse --short=8 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

configure_file(
    ${CMAKE_SOURCE_DIR}/version.h.in
    ${CMAKE_BINARY_DIR}/version.h
    @ONLY
)

# 引入FetchContent模块
include(FetchContent)

set(BUILD_SHARED_LIBS OFF)

# 下载zlib源码
FetchContent_Declare(
    zlib
    URL http://zlib.net/zlib-1.3.1.tar.gz
)

FetchContent_Declare(
  freetype
  GIT_REPOSITORY https://github.com/freetype/freetype.git
  GIT_TAG        VER-2-13-2  # 替换为你需要的版本或分支
)

FetchContent_MakeAvailable(freetype)

# 查找依赖库
find_package(OpenGL REQUIRED)
# find_package(glfw3 REQUIRED)
# find_package(nlohmann_json REQUIRED)
# find_package(ZLIB REQUIRED)
# find_package(Freetype REQUIRED)

# 添加 imgui 库
# add_subdirectory(imgui)

set(THIRDPARTY_DIR ${CMAKE_SOURCE_DIR}/thirdparty)

# 添加 cpr 库
add_subdirectory(${THIRDPARTY_DIR}/cpr)
# add_subdirectory(${THIRDPARTY_DIR}/nlohmann_json)
add_subdirectory(${THIRDPARTY_DIR}/glfw)

# 包含目录
include_directories(
    ${THIRDPARTY_DIR}/cpr/include
    ${THIRDPARTY_DIR}/imgui
    ${THIRDPARTY_DIR}/imgui/backends
    ${THIRDPARTY_DIR}/ImGuiFileDialog
    ${THIRDPARTY_DIR}/imgui/misc/freetype
    ${THIRDPARTY_DIR}/argparse/include
    ${THIRDPARTY_DIR}/nlohmann_json
    ${THIRDPARTY_DIR}/stb
)

# 添加 imgui 源文件
set(IMGUI_SOURCES
    ${THIRDPARTY_DIR}/imgui/imgui.cpp
    ${THIRDPARTY_DIR}/imgui/imgui_demo.cpp
    ${THIRDPARTY_DIR}/imgui/imgui_draw.cpp
    ${THIRDPARTY_DIR}/imgui/imgui_tables.cpp
    ${THIRDPARTY_DIR}/imgui/imgui_widgets.cpp
    ${THIRDPARTY_DIR}/imgui/backends/imgui_impl_glfw.cpp
    ${THIRDPARTY_DIR}/imgui/backends/imgui_impl_opengl3.cpp
    ${THIRDPARTY_DIR}/ImGuiFileDialog/ImGuiFileDialog.cpp
    ${THIRDPARTY_DIR}/imgui/misc/freetype/imgui_freetype.cpp
)

set(FONT_SOURCES
    ${CMAKE_SOURCE_DIR}/ttf/fusion_pixel.cpp
    ${CMAKE_SOURCE_DIR}/ttf/noto_emoji.cpp
)

set(ICON_SOURCES
    ${CMAKE_SOURCE_DIR}/icon/ds-cpp-64.c
)

# 添加可执行文件
add_executable(ds-cpp deepseek_chat.cpp ${ICON_SOURCES} ${FONT_SOURCES} ${IMGUI_SOURCES})

target_compile_definitions(ds-cpp PRIVATE IMGUI_USE_WCHAR32)

target_include_directories(ds-cpp PRIVATE ${CMAKE_BINARY_DIR})

# 链接库
target_link_libraries(ds-cpp
    PRIVATE
    cpr::cpr
    OpenGL::GL
    glfw
    zlibstatic
    freetype
    ${CMAKE_DL_LIBS}
)

# 设置编译选项
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ds-cpp PRIVATE -Wall -Wextra -pedantic)
endif()
