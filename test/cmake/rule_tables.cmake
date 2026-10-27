set(table_source "${REGRESSION_DIR}/rule_tables.tap")
execute_process(COMMAND "${TAPAS}" -c "${table_source}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Table bytecode compilation failed")
endif()
foreach(mode IN ITEMS source bytecode)
    if(mode STREQUAL "source")
        set(command "${table_source}")
    else()
        set(command -e "${table_source}c")
    endif()
    execute_process(COMMAND "${TAPAS}" ${command}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0 OR NOT output STREQUAL "rule tables: ok\n")
        message(FATAL_ERROR "Table ${mode} failed: ${output}${error}")
    endif()
endforeach()
file(REMOVE "${table_source}c")

execute_process(COMMAND "${TAPAS}" "${SUPPORT_FIXTURE_DIR}/table_serialized.tap"
    RESULT_VARIABLE result OUTPUT_VARIABLE serialized ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Table serialization failed: ${serialized}${error}")
endif()
string(STRIP "${serialized}" serialized)
set(restored_source "${BINARY_ROOT}/table_restored.tap")
file(WRITE "${restored_source}"
    "let restored = rules::deserialize(${serialized})\nassert(restored(['A', 100]))\nassert(rule { not restored(['A', 200]) })\n")
execute_process(COMMAND "${TAPAS}" "${restored_source}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
file(REMOVE "${restored_source}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Table fresh-process restoration failed: ${output}${error}")
endif()

foreach(expression IN ITEMS
        "rules::table([], {})"
        "rules::table(['x', 'x'], {'x': [1]})"
        "rules::table(['x'], {'y': [1]})"
        "rules::table(['x'], {'x': [1], 'y': [2]})"
        "rules::table(['x', 'y'], {'x': [1], 'y': []})"
        "rules::table(['x'], {'x': [1, true]})"
        "rules::table(['x'], {'x': [1.0]})"
        "rules::table(['x'], {'x': [[1]]})")
    set(invalid "${BINARY_ROOT}/invalid_table.tap")
    file(WRITE "${invalid}" "let table = ${expression}\n")
    execute_process(COMMAND "${TAPAS}" "${invalid}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(result EQUAL 0 OR NOT "${output}${error}" MATCHES "rules::table")
        message(FATAL_ERROR "Invalid Table was not rejected: ${expression}: ${output}${error}")
    endif()
endforeach()
file(REMOVE "${invalid}")
