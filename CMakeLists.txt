cmake_minimum_required(VERSION 3.16)
project(armor_detector LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OpenCV_DIR "D:/opencv_work/build/install")

find_package(OpenCV REQUIRED)
add_executable(armor_detector main.cpp armor.cpp)
target_include_directories(armor_detector PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(armor_detector PRIVATE ${OpenCV_LIBS})
