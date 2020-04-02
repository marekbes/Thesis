cmake_minimum_required(VERSION 3.15)
project(proofOfConcept)

set(CMAKE_CXX_STANDARD 20)

add_executable(proofOfConcept src/HashTable.h src/ObjectPool.h src/Semaphore.h src/NodeCoordinator.cpp src/NodeCoordinator.h src/Executor.cpp src/Executor.h src/runner.cpp src/YahooQuery.cpp src/YahooQuery.h src/QueryTask.h src/HashTablePool.h src/Setting.h src/TaskResult.h src/Setting.cpp src/Job.cpp src/Job.h src/NumaAlloc.h src/NodeComm.cpp src/NodeComm.h src/ResultGroup.cpp src/ResultGroup.h)

SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread -lnuma -lboost_thread -lboost_program_options")
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()
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
        numa
        ${Boost_LIBRARIES})
add_compile_definitions(POC_DEBUG)