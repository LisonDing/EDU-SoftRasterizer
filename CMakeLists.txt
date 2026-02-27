cmake_minimum_required(VERSION 3.10)
project(RENDERP)

set(CMAKE_CXX_STANDARD 11)

# ==========================================
# 【核心修复 1】添加 src 目录到头文件搜索路径
# ==========================================
# 这样 #include "core/model.h" 等路径才能在任何地方被找到
include_directories(${CMAKE_SOURCE_DIR}/src)

# ==========================================
# 【核心修复 2】macOS X11 路径
# ==========================================
if (APPLE)
    include_directories(/opt/X11/include)
    link_directories(/opt/X11/lib)
endif()

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(RENDERP ${SOURCES})

if (APPLE)
    target_link_libraries(RENDERP X11)
else()
    target_link_libraries(RENDERP X11) 
endif()