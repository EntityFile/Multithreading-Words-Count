cmake_minimum_required(VERSION 3.15)
project(WordsCount2)

# Options
option(WARNINGS_AS_ERRORS   "Treat compiler warnings as errors."    ON)
option(ENABLE_CONAN         "Use Conan as a package manager."       OFF)
option(ENABLE_PVS_STUDIO    "Use PVS-Studio static code analyzer."  OFF) # Option for the local usage only. PVS-Studio isn't installed on GitHub machines.
option(ENABLE_SANITIZERS    "Use sanitizers to detect errors."      OFF) # Option for the test builds. Do not use it in the production builds.

find_package(Boost 1.71.0 COMPONENTS filesystem program_options system locale REQUIRED)
find_package(LibArchive REQUIRED)
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 14)
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREAD ON)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${ICU_INCLUDE_DIRS})

add_executable(WordsCount2 ./src/main.cpp)
target_link_libraries(WordsCount2 ${Boost_LIBRARIES} ${LibArchive_LIBRARIES} ${ICU_LIBRARIES})
