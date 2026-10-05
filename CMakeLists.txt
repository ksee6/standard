cmake_minimum_required(VERSION 3.14)
project(Standard VERSION 1.0.0 LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(OpenMP)

add_compile_options(-O3 -fno-sanitize=all)
if(OpenMP_CXX_FOUND)
    add_compile_options(${OpenMP_CXX_FLAGS})
    add_link_options(${OpenMP_CXX_FLAGS})
endif()
add_link_options(-fno-sanitize=all)

# --- Automatic Source Discovery ---
file(GLOB_RECURSE KSEE_SOURCES 
    "src/Ksee/*.cpp"
    "src/Ksee/**/*.cpp"
)

# Main Library
add_library(Ksee STATIC ${KSEE_SOURCES})

# Include Directories
target_include_directories(Ksee PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Dependencies
find_package(ZLIB QUIET)
if(ZLIB_FOUND)
    target_include_directories(Ksee PUBLIC ${ZLIB_INCLUDE_DIRS})
    target_link_libraries(Ksee PUBLIC ${ZLIB_LIBRARIES})
endif()

find_path(MINIZ_INCLUDE_DIR NAMES miniz.h PATH_SUFFIXES miniz)
if(MINIZ_INCLUDE_DIR)
    target_include_directories(Ksee PUBLIC ${MINIZ_INCLUDE_DIR})
    find_library(MINIZ_LIB NAMES miniz QUIET)
    if(MINIZ_LIB)
        target_link_libraries(Ksee PUBLIC ${MINIZ_LIB})
    endif()
endif()

if(UNIX AND NOT APPLE)
    find_library(PTHREAD_LIB pthread)
    find_library(RT_LIB rt)
    if(PTHREAD_LIB)
        target_link_libraries(Ksee PUBLIC ${PTHREAD_LIB})
    endif()
    if(RT_LIB)
        target_link_libraries(Ksee PUBLIC ${RT_LIB})
    endif()
endif()

# Alias for consistent usage
if(NOT TARGET Ksee::Ksee)
    add_library(Ksee::Ksee ALIAS Ksee)
endif()

# Tests
option(KSEE_BUILD_TESTS "Build Ksee tests" ON)

if(KSEE_BUILD_TESTS)
    enable_testing()

    add_executable(test_ksee tests/test_ksee.cpp)
    target_link_libraries(test_ksee PRIVATE Ksee::Ksee)
    set_target_properties(test_ksee PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
    add_test(NAME test_ksee COMMAND test_ksee)
endif()
