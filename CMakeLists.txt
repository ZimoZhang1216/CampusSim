cmake_minimum_required(VERSION 3.20)
project(CampusSim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SFML 3
find_package(SFML 3 COMPONENTS Graphics Window System CONFIG REQUIRED)

add_executable(CampusSim
    src/main.cpp
)

target_link_libraries(CampusSim PRIVATE
    sfml-graphics
    sfml-window
    sfml-system
)

# ------------- 安装规则（Windows 打包用）-------------

# 把 exe 安装到包根目录
install(TARGETS CampusSim
        RUNTIME DESTINATION .)

# 把资源目录（assets、scenes）一起打进发布包
install(DIRECTORY assets scenes
        DESTINATION .)