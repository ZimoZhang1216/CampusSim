cmake_minimum_required(VERSION 3.20)
project(CampusSim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 找 SFML（不强制版本），同时兼容 SFML2 / SFML3
find_package(SFML COMPONENTS graphics window system REQUIRED)

add_executable(CampusSim
    src/main.cpp
)

# 优先使用现代 CMake target（vcpkg / SFML 官方推荐）
if (TARGET SFML::Graphics)
    target_link_libraries(CampusSim PRIVATE
        SFML::Graphics
        SFML::Window
        SFML::System
    )
else()
    # 兼容老式 target 名字（你本地如果还是用 sfml-graphics 这套）
    target_link_libraries(CampusSim PRIVATE
        sfml-graphics
        sfml-window
        sfml-system
    )
endif()

# ---------------- 安装规则（Windows 打包用） ----------------

# 把 exe 安装到包根目录
install(TARGETS CampusSim
        RUNTIME DESTINATION .)

# 把资源目录（assets、scenes）一起打进发布包
install(DIRECTORY assets scenes
        DESTINATION .)
