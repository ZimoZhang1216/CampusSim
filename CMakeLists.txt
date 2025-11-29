cmake_minimum_required(VERSION 3.20)
project(CampusSim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 这里假设你用的是 SFML 3，如果你在 Windows 上用的是 SFML 2，需要对应调整 find_package 和链接名字
find_package(SFML 3 COMPONENTS Graphics Window System CONFIG REQUIRED)

add_executable(CampusSim
    src/main.cpp
)

target_link_libraries(CampusSim PRIVATE
    sfml-graphics
    sfml-window
    sfml-system
)

# 安装：把 exe 安装到根目录，把资源目录一起装进去
install(TARGETS CampusSim
        RUNTIME DESTINATION .)

install(DIRECTORY assets scenes
        DESTINATION .)