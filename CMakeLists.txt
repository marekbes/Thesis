cmake_minimum_required(VERSION 3.15)
project(proofOfConcept)

set(CMAKE_CXX_STANDARD 20)

add_executable(proofOfConcept src/HashTable.h src/ObjectPool.h src/Semaphore.h src/NodeCoordinator.cpp src/NodeCoordinator.h src/Setting.cpp src/Executor.cpp src/Executor.h src/runner.cpp src/queries/YahooQuery.cpp src/queries/YahooQuery.h src/QueryTask.h src/HashTablePool.h src/Setting.h src/TaskResult.h src/NumaAlloc.h src/ResultGroup.cpp src/ResultGroup.h src/QueryTask.cpp src/TaskResult.cpp src/queries/SmartGrid2.cpp src/queries/SmartGrid2.h src/HashFunctions.cpp src/HashFunctions.h src/ThreadCountWindowMarker.h src/AbstractNodeCoordinator.h src/EagerResultMerger.h src/DelayedResultMerger.h src/ThreadClockWindowMarker.h src/DirectResultMerger.h src/DefaultTableProvider.h src/LatencyMonitor.h src/LatencyMonitor.cpp)

SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread -lnuma -lboost_thread -lboost_program_options")
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()
set(CMAKE_CXX_FLAGS "-Wall -Wextra -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Boost Library
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.63.0 COMPONENTS filesystem regex program_options iostreams thread)
include_directories(${Boost_INCLUDE_DIRS})


include_directories(src/concurrentqueue)

target_link_libraries(proofOfConcept PRIVATE
        numa tbb
        ${Boost_LIBRARIES})
add_compile_definitions(POC_LATENCY)
#add_compile_definitions(POC_DEBUG)
#add_compile_definitions(POC_DEBUG_POSITION)
#add_compile_definitions(POC_DEBUG_COMM)
