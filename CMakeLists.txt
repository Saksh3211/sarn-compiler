cmake_minimum_required(VERSION 3.15)
project(sarn VERSION 0.3 LANGUAGES C CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# C Standard  
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# MSVC settings
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /permissive- /EHsc")
    add_compile_definitions(WIN32 _WINDOWS NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

# LLVM is optional
find_package(LLVM CONFIG QUIET)
if(LLVM_FOUND)
    message(STATUS "LLVM ${LLVM_PACKAGE_VERSION} found")
    set(SARN_HAS_LLVM TRUE)
else()
    message(STATUS "LLVM not found - building without codegen support")
    set(SARN_HAS_LLVM FALSE)
endif()

add_subdirectory(runtime)
add_subdirectory(compiler)