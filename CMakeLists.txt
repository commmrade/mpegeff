cmake_minimum_required(VERSION 3.10)
project(proj VERSION 0.1.0 LANGUAGES C CXX)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil)

add_executable(proj main.cpp)
target_include_directories(proj PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(proj PRIVATE ${FFMPEG_LIBRARIES})
target_compile_options(proj PRIVATE ${FFMPEG_CFLAGS_OTHER})