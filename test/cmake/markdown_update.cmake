if(NOT DEFINED TAPAS OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "TAPAS and WORK_DIR are required")
endif()
file(MAKE_DIRECTORY "${WORK_DIR}")
set(document "${WORK_DIR}/example.md")
file(WRITE "${document}" [=[# Markdown execution

```tapas
let Positive = rule (value: Int) {
    value > 0
}
function increment(value: Int) -> Int
{
    return value + 1
}
let amount = increment(2)
assert(Positive(amount))
print(amount)
```

<pre class='Tapas-Return'>
stale
</pre>

Text between code blocks.

```tapas
let next = increment(amount)
assert(Positive(next))
print(next)
```
]=])
execute_process(COMMAND "${TAPAS}" --stdout "${document}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT output STREQUAL "3\n4\n" OR NOT error STREQUAL "")
    message(FATAL_ERROR "stdout execution failed: ${output}${error}")
endif()
foreach(pass RANGE 1 2)
    execute_process(COMMAND "${TAPAS}" "${document}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0 OR NOT output STREQUAL "" OR NOT error STREQUAL "")
        message(FATAL_ERROR "Markdown update failed: ${output}${error}")
    endif()
    file(READ "${document}" updated)
    foreach(value 3 4)
        string(FIND "${updated}" "<pre class='Tapas-Return'>\n${value}\n</pre>" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "Missing output ${value}: ${updated}")
        endif()
    endforeach()
    if(updated MATCHES "stale" OR NOT updated MATCHES "Text between code blocks")
        message(FATAL_ERROR "Incorrect document replacement: ${updated}")
    endif()
    if(pass EQUAL 2 AND NOT updated STREQUAL previous)
        message(FATAL_ERROR "Repeated update changed the document")
    endif()
    set(previous "${updated}")
endforeach()
