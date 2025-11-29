cmake_minimum_required(VERSION 3.20)
project(CampusSim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows 下多加一个 Main 组件，其他平台只要 Graphics / Window / System
if (WIN32)
    set(SFML_COMPONENTS Graphics Window System Main)
else()
    set(SFML_COMPONENTS Graphics Window System)
endif()

find_package(SFML COMPONENTS ${SFML_COMPONENTS} REQUIRED)

# Windows 用 WIN32 子系统（隐藏黑框），其他平台正常
if (WIN32)
    add_executable(CampusSim WIN32 src/main.cpp)
else()
    add_executable(CampusSim src/main.cpp)
endif()

# 优先使用现代 CMake target（vcpkg / SFML 官方推荐）
if (TARGET SFML::Graphics)
    target_link_libraries(CampusSim PRIVATE
        SFML::Graphics
        SFML::Window
        SFML::System
    )
    # Windows 上如果有 SFML::Main，就链接它，把 WinMain 入口交给 SFML
    if (WIN32 AND TARGET SFML::Main)
        target_link_libraries(CampusSim PRIVATE SFML::Main)
    endif()
else()
    # 兼容老式 target 名字（你本地如果还是用 sfml-graphics 这套）
    target_link_libraries(CampusSim PRIVATE
        sfml-graphics
        sfml-window
        sfml-system
    )
    if (WIN32)
        target_link_libraries(CampusSim PRIVATE sfml-main)
    endif()
endif()

# ---------------- 安装规则（Windows 打包用） ----------------

# 把 exe 安装到包根目录
install(TARGETS CampusSim
        RUNTIME DESTINATION .)

# 把资源目录（assets、scenes）一起打进发布包
install(DIRECTORY assets scenes
        DESTINATION .)
