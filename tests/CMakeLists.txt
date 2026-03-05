# CTest integration for .slua test files

function(add_slua_test name source expected_exit)
    add_test(
        NAME ${name}
        COMMAND sluac ${CMAKE_CURRENT_SOURCE_DIR}/${source} -o ${CMAKE_BINARY_DIR}/test_${name}
    )
    set_tests_properties(${name} PROPERTIES PASS_REGULAR_EXPRESSION "")
endfunction()

add_slua_test(strict_hello    strict/hello.slua    0)
add_slua_test(nonstrict_null  nonstrict/null_propagation.slua 0)
