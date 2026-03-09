cmake_minimum_required(VERSION 3.6)

project(Normalizzatore VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(Normalizzatore
                src/main.cpp
                src/MainWindow.h
                src/MainWindow.cpp)

target_link_libraries(Normalizzatore
                    PRIVATE Qt6::Widgets)

install(TARGETS Normalizzatore
        RUNTIME DESTINATION bin)