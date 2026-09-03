if(NOT DEFINED TAPAS OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "TAPAS and WORK_DIR are required")
endif()

file(MAKE_DIRECTORY "${WORK_DIR}")
set(SAMPLE "${WORK_DIR}/sample.tap")
file(WRITE "${SAMPLE}" [=[function compact(value: Int) -> Int {
    if(true){
        return value
    }else{
        return 0
    }
}

function expanded(
    left: Int,
    right: Int,
) -> Int
{
    return left + right
}

let positive = rule(value: Int){
    value > 0
}
assert(rule{
    positive(1)
})
let ExactRule = types::rule(types::Int)
]=])

execute_process(
    COMMAND "${TAPAS}" -m format --check "${SAMPLE}"
    RESULT_VARIABLE CHECK_RESULT
    OUTPUT_VARIABLE CHECK_OUTPUT
)
if(NOT CHECK_RESULT EQUAL 1 OR NOT CHECK_OUTPUT STREQUAL "${SAMPLE}\n")
    message(FATAL_ERROR "format --check did not report the unformatted file")
endif()

execute_process(
    COMMAND "${TAPAS}" -m format "${SAMPLE}"
    RESULT_VARIABLE FORMAT_RESULT
)
if(NOT FORMAT_RESULT EQUAL 0)
    message(FATAL_ERROR "format command failed")
endif()

file(READ "${SAMPLE}" FORMATTED)
set(EXPECTED [=[function compact(value: Int) -> Int
{
    if (true) {
        return value
    } else {
        return 0
    }
}

function expanded(
    left: Int,
    right: Int,
) -> Int {
    return left + right
}

let positive = rule (value: Int) {
    value > 0
}
assert(rule {
    positive(1)
})
let ExactRule = types::rule(types::Int)
]=])
if(NOT FORMATTED STREQUAL EXPECTED)
    message(FATAL_ERROR "unexpected formatted source:\n${FORMATTED}")
endif()

execute_process(
    COMMAND "${TAPAS}" -m format --check "${SAMPLE}"
    RESULT_VARIABLE IDEMPOTENT_RESULT
    OUTPUT_VARIABLE IDEMPOTENT_OUTPUT
)
if(NOT IDEMPOTENT_RESULT EQUAL 0 OR NOT IDEMPOTENT_OUTPUT STREQUAL "")
    message(FATAL_ERROR "formatting is not idempotent")
endif()
