cmake_minimum_required(VERSION 3.20)
project(slua VERSION 0.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# -- LLVM (optional until codegen phase) ---------------------------------------
find_package(LLVM CONFIG QUIET)
if(LLVM_FOUND)
    message(STATUS "LLVM ${LLVM_PACKAGE_VERSION} found — codegen enabled")
    include_directories(${LLVM_INCLUDE_DIRS})
    add_definitions(${LLVM_DEFINITIONS})
    set(SLUA_HAS_LLVM TRUE)
else()
    message(STATUS "LLVM not found — building without codegen (lexer/parser only)")
    set(SLUA_HAS_LLVM FALSE)
endif()

# -- Sub-projects --------------------------------------------------------------
add_subdirectory(runtime)
add_subdirectory(compiler)

# -- Tests ---------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
