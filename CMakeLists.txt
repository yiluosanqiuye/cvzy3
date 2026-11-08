cmake_minimum_required(VERSION 3.16)
project(zy3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OpenCV_DIR "D:/opencv_work/build/install")

find_package(OpenCV REQUIRED)
add_executable(zy3 main.cpp zy3.cpp)
target_include_directories(zy3 PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(zy3 PRIVATE ${OpenCV_LIBS})
