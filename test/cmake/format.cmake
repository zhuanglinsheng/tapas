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

set(EDITS_CHECK "${WORK_DIR}/edits.tap")
file(WRITE "${EDITS_CHECK}" "import format as formatter

let source = io::read_text('${SAMPLE}')
let changes = formatter::edits(source)
let output = ''
let position = 0
let valid = true
for (let change in changes) {
    if (types::matches(change, formatter::Edit) == false or
        change['start'] < position or change['end'] < change['start']) {
        valid = false
    }
    output.append(source[position:change['start']])
    output.append(change['text'])
    position = change['end']
}
output.append(source[position:source.len()])
let formatted = formatter::source(source)
print(valid and len(changes) > 0)
print(output == formatted)
let first = changes[0]
let partial = source[0:first['start']]
partial.append(first['text'])
partial.append(source[first['end']:source.len()])
print(len(changes) > 1 and partial != source and partial != formatted)
print(len(formatter::edits(formatted)) == 0)
")
execute_process(
    COMMAND "${TAPAS}" "${EDITS_CHECK}"
    RESULT_VARIABLE EDITS_RESULT
    OUTPUT_VARIABLE EDITS_OUTPUT
)
if(NOT EDITS_RESULT EQUAL 0 OR
   NOT EDITS_OUTPUT STREQUAL "true\ntrue\ntrue\ntrue\n")
    message(FATAL_ERROR
        "format::edits returned invalid edits: ${EDITS_OUTPUT}")
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
