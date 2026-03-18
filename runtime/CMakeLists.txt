file(GLOB RUNTIME_SOURCES src/*.c)
file(GLOB_RECURSE STDLIB_MODULE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../stdlib/modules/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../stdlib/modules/*.cpp)

add_library(libslua STATIC
    ${RUNTIME_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../stdlib/slua_stdlib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../stdlib/modules/slua_registry.c
    ${STDLIB_MODULE_SOURCES})

target_include_directories(libslua PUBLIC include)
target_include_directories(libslua PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../stdlib)
target_include_directories(libslua PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../stdlib/modules)

set_target_properties(libslua PROPERTIES OUTPUT_NAME slua)

if(MSVC)
    target_compile_options(libslua PRIVATE /W3 /WX-)
else()
    target_compile_options(libslua PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

find_package(raylib CONFIG QUIET)
if(raylib_FOUND)
    message(STATUS "raylib found")
    target_sources(libslua PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../stdlib/slua_raylib.c)
    target_link_libraries(libslua PUBLIC raylib)
    target_compile_definitions(libslua PUBLIC SLUA_HAS_RAYLIB=1)
else()
    message(STATUS "raylib not found")
endif()

if(WIN32)
    target_link_libraries(libslua PUBLIC ws2_32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(libslua PUBLIC Threads::Threads)
endif()

install(TARGETS libslua DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/slua)
