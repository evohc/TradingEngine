cmake_minimum_required(VERSION 3.25)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(TradingExchangeDemo VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LOW_LATENCY_FLAGS "-O3 -march=native -mtune=native -ffast-math -fno-omit-frame-pointer -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${LOW_LATENCY_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g -ggdb -Wall -Wextra")

set(AERON_BASE_DIR "/home/amurray/Documents/aeron-master")
set(AERON_BUILD_DIR "${AERON_BASE_DIR}/cppbuild/Release")
set(AERON_INCLUDE_DIR "${AERON_BASE_DIR}/aeron-client/src/main/cpp_wrapper/")
set(AERON_INCLUDE_C_DIR "${AERON_BASE_DIR}/aeron-client/src/main/c/")
set(AERON_CLIENT_LIB "${AERON_BASE_DIR}/cppbuild/Release/lib/libaeron_static.a")
set(BOOST_INCLUDE_DIR "/home/amurray/Documents/boost")

add_subdirectory(src/gateway)
add_subdirectory(src/matching)
add_subdirectory(src/viewer)
add_subdirectory(unit_tests)