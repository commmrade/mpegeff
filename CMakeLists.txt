cmake_minimum_required(VERSION 3.10)
project(proj VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswresample)

include(FetchContent)

# Argument parser
FetchContent_Declare(
    argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
)
FetchContent_MakeAvailable(argparse)

# Build library target from sources (so tests can link against it).
add_library(proj_lib
    src/types/audiofifo.cpp
    src/types/codeccontext.cpp
    src/types/formatcontext.cpp
    src/types/frame.cpp
    src/types/packet.cpp
    src/types/swrcontext.cpp
    src/algos/remux.cpp
    src/algos/transcode.cpp
    src/utils.cpp
)

target_include_directories(proj_lib PRIVATE ${FFMPEG_INCLUDE_DIRS} src/algos src/types)
target_link_libraries(proj_lib PRIVATE ${FFMPEG_LIBRARIES})
target_compile_options(proj_lib PRIVATE ${FFMPEG_CFLAGS_OTHER} -Wall -Wextra)


# Executable that uses the library
add_executable(proj src/main.cpp)
target_include_directories(proj PRIVATE ${FFMPEG_INCLUDE_DIRS} src/algos src/types)
target_link_libraries(proj PRIVATE proj_lib argparse)

# Enable testing and add GoogleTest via FetchContent
enable_testing()

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0
)
# Make googletest available
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

# Tests are placed in the project's root `tests` directory (not under src)
# Create a test target that links against the library.
# Add your test files under `tests/` (for example: tests/test_utils.cpp)
add_executable(unit_tests
    tests/test_utils.cpp
)

target_include_directories(unit_tests PRIVATE ${FFMPEG_INCLUDE_DIRS} src/algos src/types)
target_link_libraries(unit_tests PRIVATE gtest_main proj_lib ${FFMPEG_LIBRARIES})
target_compile_options(unit_tests PRIVATE ${FFMPEG_CFLAGS_OTHER} -Wall -Wextra)

gtest_discover_tests(unit_tests)
