file(GLOB_RECURSE COMPILER_SOURCES
    src/lexer/*.cpp
    src/parser/*.cpp
    src/ast/*.cpp
    src/sema/*.cpp
    src/codegen/*.cpp
    src/driver/*.cpp
)

add_executable(sarnc ${COMPILER_SOURCES})
target_include_directories(sarnc PRIVATE include)
target_link_libraries(sarnc PRIVATE libsarn)

# MSVC compiler settings
if(MSVC)
    target_compile_options(sarnc PRIVATE /W4 /EHsc /permissive-)
    target_compile_definitions(sarnc PRIVATE WIN32 _WINDOWS NOMINMAX)
else()
    target_compile_options(sarnc PRIVATE -Wall -Wextra -fPIC)
endif()

# Optional LLVM support
if(SARN_HAS_LLVM)
    llvm_map_components_to_libnames(LLVM_LIBS
        core support irreader passes
        x86codegen x86asmparser x86desc x86info
        mc mcjit target analysis scalaropts ipo
        bitwriter)
    target_link_libraries(sarnc PRIVATE ${LLVM_LIBS})
    target_include_directories(sarnc PRIVATE ${LLVM_INCLUDE_DIRS})
    target_compile_definitions(sarnc PRIVATE SARN_HAS_LLVM=1)
endif()