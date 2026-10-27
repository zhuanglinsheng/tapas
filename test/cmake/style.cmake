if(NOT DEFINED TAPAS OR NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "TAPAS and SOURCE_DIR are required")
endif()

file(GLOB_RECURSE SOURCES
    "${SOURCE_DIR}/src/stdlib/*.tap"
    "${SOURCE_DIR}/docs/examples/*.tap"
    "${SOURCE_DIR}/examples/*.tap"
    "${SOURCE_DIR}/external/*.tap"
    "${SOURCE_DIR}/test/benchmarks/*.tap"
    "${SOURCE_DIR}/test/solve/*.tap"
    "${SOURCE_DIR}/test/language_rules/fixtures/*.tap"
    "${SOURCE_DIR}/test/language_rules/regression/*.tap"
)

# This fixture deliberately retains a compatibility spelling. Invalid-source
# fixtures and the formatter's unformatted inputs are not normal source files.
list(FILTER SOURCES EXCLUDE REGEX "/compatibility_forms\\.tap$")

execute_process(
    COMMAND "${TAPAS}" -m format --check ${SOURCES}
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE RESULT
    OUTPUT_VARIABLE OUTPUT
    ERROR_VARIABLE ERROR_OUTPUT
)
if(NOT RESULT EQUAL 0)
    message(FATAL_ERROR "Tapas source style check failed:\n${OUTPUT}${ERROR_OUTPUT}")
endif()
