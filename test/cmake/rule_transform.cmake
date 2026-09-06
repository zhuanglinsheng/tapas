set(transform_source "${REGRESSION_DIR}/rule_transform.tap")
execute_process(COMMAND "${TAPAS}" -c "${transform_source}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Rule transform compilation failed")
endif()
foreach(mode IN ITEMS source bytecode)
    if(mode STREQUAL "source")
        set(transform_command "${transform_source}")
    else()
        set(transform_command -e "${transform_source}c")
    endif()
    execute_process(COMMAND "${TAPAS}" ${transform_command}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0 OR NOT output MATCHES "rule transforms: ok")
        message(FATAL_ERROR "Rule transforms ${mode} failed: ${output}${error}")
    endif()
endforeach()
file(REMOVE "${transform_source}c")

foreach(fixture IN ITEMS ambiguous foreign missing range premise_signature)
    execute_process(COMMAND "${TAPAS}"
        "${RUNTIME_INVALID_DIR}/rule_item_${fixture}.tap"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(result EQUAL 0 OR NOT error MATCHES "Runtime Error")
        message(FATAL_ERROR
            "Rule Item ${fixture} should be rejected: ${output}${error}")
    endif()
endforeach()

execute_process(COMMAND "${TAPAS}" -c
    "${COMPILE_INVALID_DIR}/rule_item_selector.tap"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(result EQUAL 0)
    message(FATAL_ERROR
        "Invalid Rule Item selector should fail compilation: ${output}${error}")
endif()
