# libslua — C runtime library

file(GLOB RUNTIME_SOURCES src/*.c)

add_library(libslua STATIC ${RUNTIME_SOURCES})

target_include_directories(libslua PUBLIC include)

set_target_properties(libslua PROPERTIES OUTPUT_NAME slua)

install(TARGETS libslua DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/slua)
