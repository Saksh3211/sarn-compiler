file(GLOB_RECURSE COMPILER_SOURCES
    src/lexer/*.cpp
    src/parser/*.cpp
    src/ast/*.cpp
    src/sema/*.cpp
    src/codegen/*.cpp
    src/driver/*.cpp
)

add_executable(sluac ${COMPILER_SOURCES})
target_include_directories(sluac PRIVATE include)

if(SLUA_HAS_LLVM)
    llvm_map_components_to_libnames(LLVM_LIBS
        core support irreader passes
        x86codegen x86asmparser x86desc x86info
        mc mcjit target analysis scalaropts ipo
        bitwriter)
    target_link_libraries(sluac PRIVATE ${LLVM_LIBS} libslua)
    target_include_directories(sluac PRIVATE ${LLVM_INCLUDE_DIRS})
    target_compile_definitions(sluac PRIVATE SLUA_HAS_LLVM=1)
else()
    target_link_libraries(sluac PRIVATE libslua)
endif()

target_compile_options(sluac PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
)