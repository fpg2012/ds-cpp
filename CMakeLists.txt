cmake_minimum_required(VERSION 3.14)
project(DeepSeekChat)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 引入FetchContent模块
include(FetchContent)

# 下载zlib源码
FetchContent_Declare(
    zlib
    URL http://zlib.net/zlib-1.3.1.tar.gz
)

# 查找依赖库
find_package(OpenGL REQUIRED)
# find_package(glfw3 REQUIRED)
# find_package(nlohmann_json REQUIRED)
# find_package(ZLIB REQUIRED)
find_package(Freetype REQUIRED)

# 添加 imgui 库
# add_subdirectory(imgui)

set(THIRDPARTY_DIR ${CMAKE_SOURCE_DIR}/thirdparty)

# 添加 cpr 库
add_subdirectory(${THIRDPARTY_DIR}/cpr)
add_subdirectory(${THIRDPARTY_DIR}/nlohmann_json)
add_subdirectory(${THIRDPARTY_DIR}/glfw)

# 包含目录
include_directories(
    ${THIRDPARTY_DIR}/cpr/include
    ${THIRDPARTY_DIR}/imgui
    ${THIRDPARTY_DIR}/imgui/backends
    ${THIRDPARTY_DIR}/ImGuiFileDialog
    ${THIRDPARTY_DIR}/imgui/misc/freetype
    ${THIRDPARTY_DIR}/argparse/include
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

# 添加可执行文件
add_executable(deepseek_chat deepseek_chat.cpp ${FONT_SOURCES} ${IMGUI_SOURCES})

target_compile_definitions(deepseek_chat PRIVATE IMGUI_USE_WCHAR32)

# 链接库
target_link_libraries(deepseek_chat
    PRIVATE
    cpr::cpr
    OpenGL::GL
    glfw
    zlibstatic
    Freetype::Freetype
    ${CMAKE_DL_LIBS}
)

# 设置编译选项
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(deepseek_chat PRIVATE -Wall -Wextra -pedantic)
endif()
