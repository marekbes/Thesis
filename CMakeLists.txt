cmake_minimum_required(VERSION 3.15)
project(proofOfConcept)

set(CMAKE_CXX_STANDARD 20)

add_executable(proofOfConcept main.cpp
        HashTable.h ObjectPool.cpp)

SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread -lnuma -lboost_thread")
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Boost Library
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.63.0 COMPONENTS filesystem regex program_options iostreams thread)

message(INFO ${Boost_LIBRARIES})

include_directories(${Boost_INCLUDE_DIRS})
target_link_libraries(proofOfConcept PRIVATE
        ${Boost_LIBRARIES})