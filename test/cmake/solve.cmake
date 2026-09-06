# Formatting must also work without Python/OR-Tools, in both execution modes.
set(display_source "${SOURCE_ROOT}/test/solve/rule_display.tap")
execute_process(COMMAND "${TAPAS}" -c "${display_source}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Rule display bytecode compilation failed")
endif()
execute_process(COMMAND "${TAPAS}" "${display_source}"
    RESULT_VARIABLE source_result OUTPUT_VARIABLE source_output ERROR_VARIABLE source_error)
execute_process(COMMAND "${TAPAS}" -e "${display_source}c"
    RESULT_VARIABLE bytecode_result OUTPUT_VARIABLE bytecode_output ERROR_VARIABLE bytecode_error)
file(REMOVE "${display_source}c")
# Heap addresses deliberately differ between processes; compare all other output.
string(REGEX REPLACE "<0x[0-9a-fA-F]+>" "<address>" source_output "${source_output}")
string(REGEX REPLACE "<0x[0-9a-fA-F]+>" "<address>" bytecode_output "${bytecode_output}")
if(NOT source_result EQUAL 0 OR NOT bytecode_result EQUAL 0
   OR NOT source_output STREQUAL bytecode_output
   OR NOT source_output MATCHES "No checker or captured function was executed."
   OR NOT source_output MATCHES "RuleInstance\\[#[0-9]+; quantity=3\\]"
   OR NOT source_output MATCHES "Requirement\\[Rule #[0-9]+\\[Int\\]"
   OR NOT source_output MATCHES "<address>"
   OR NOT source_output MATCHES "RuleTerm Parameter\\[quantity: Int\\]"
   OR NOT source_output MATCHES "RuleItem Condition\\["
   OR NOT source_output MATCHES "RuleItem Requirement\\["
   OR NOT source_output MATCHES "Function add\\[Int\\] -> Int"
   OR NOT source_output MATCHES "Function\\[Int\\] -> Int"
   OR NOT source_output MATCHES "Function\\[List\\[Int\\]\\] -> List\\[Int\\]"
   OR NOT source_output MATCHES "Rule Positive\\[Int\\]"
   OR NOT source_output MATCHES "RuleInstance\\[Positive; value=3\\]"
   OR NOT source_output MATCHES "RuleIR Positive \{"
   OR source_output MATCHES "Rule Source|Function anonymous_function|Function saved_adder|Rule restrict")
    message(FATAL_ERROR "Rule display failed: ${source_output}${source_error}${bytecode_output}${bytecode_error}")
endif()

set(ENV{TAPAS_SOLVE_INT_MIN} "-1000000000000")
set(ENV{TAPAS_SOLVE_INT_MAX} "1000000000000")
if(DEFINED ENV{TAPAS_SOLVE_PYTHON} AND NOT "$ENV{TAPAS_SOLVE_PYTHON}" STREQUAL "")
    set(SOLVE_PYTHON "$ENV{TAPAS_SOLVE_PYTHON}")
else()
    find_program(SOLVE_PYTHON NAMES python3)
endif()
# A missing interpreter must produce a normal HoldResult, never crash the VM.
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
    "TAPAS_SOLVE_PYTHON=/nonexistent/tapas-solve-python"
    "${TAPAS}" "${SOURCE_ROOT}/test/solve/backend_error.tap"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT output MATCHES "solve error: ok")
    message(FATAL_ERROR "Missing interpreter handling failed: ${output}${error}")
endif()
if(NOT SOLVE_PYTHON)
    message(STATUS "CP-SAT integration skipped: Python is unavailable")
    return()
endif()
execute_process(COMMAND "${SOLVE_PYTHON}" -c "from ortools.sat.python import cp_model"
    RESULT_VARIABLE available OUTPUT_QUIET ERROR_QUIET)
if(NOT available EQUAL 0)
    message(STATUS "CP-SAT integration skipped: install src/stdlib/solve/requirements.txt")
    return()
endif()
foreach(suite IN ITEMS test_backend test_presolve test_diagnostics)
    execute_process(COMMAND "${SOLVE_PYTHON}" "${SOURCE_ROOT}/test/solve/${suite}.py"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${suite} failed: ${output}${error}")
    endif()
endforeach()
set(source "${SOURCE_ROOT}/test/solve/hold.tap")
execute_process(COMMAND "${TAPAS}" -c "${source}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "solve bytecode compilation failed")
endif()
foreach(mode IN ITEMS source bytecode)
    if(mode STREQUAL "source")
        set(command "${source}")
    else()
        set(command -e "${source}c")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "TAPAS_SOLVE_PYTHON=${SOLVE_PYTHON}"
        "${TAPAS}" ${command} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0 OR NOT output STREQUAL "solve hold: ok\n")
        message(FATAL_ERROR "solve ${mode} failed: ${output}${error}")
    endif()
endforeach()
file(REMOVE "${source}c")

set(sample "${SOURCE_ROOT}/test/solve/sample.tap")
execute_process(COMMAND "${TAPAS}" -c "${sample}" RESULT_VARIABLE result
	OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
	message(FATAL_ERROR
		"solve sample bytecode compilation failed: ${output}${error}")
endif()
foreach(mode IN ITEMS source bytecode)
	if(mode STREQUAL "source")
		set(command "${sample}")
	else()
		set(command -e "${sample}c")
	endif()
	execute_process(COMMAND "${CMAKE_COMMAND}" -E env
		"TAPAS_SOLVE_PYTHON=${SOLVE_PYTHON}"
		"${TAPAS}" ${command} RESULT_VARIABLE result
		OUTPUT_VARIABLE output ERROR_VARIABLE error)
	if(NOT result EQUAL 0 OR NOT output STREQUAL "solve sample: ok\n")
		message(FATAL_ERROR
			"solve sample ${mode} failed: ${output}${error}")
	endif()
endforeach()
file(REMOVE "${sample}c")
# A backend claiming a bad, well-typed witness must be caught by the checker.
set(fake "${CMAKE_CURRENT_BINARY_DIR}/solve_wrong_witness.py")
file(WRITE "${fake}" "#!${SOLVE_PYTHON}\nimport sys\nsource = sys.stdin.buffer\nsize = int.from_bytes(source.read(4), 'big')\nsource.read(size)\nreply = b'TAPAS_SOLVE_3\\nsat\\nconfigured\\n\\n1\\ni9\\n0\\n'\nsys.stdout.buffer.write(len(reply).to_bytes(4, 'big') + reply)\nsys.stdout.buffer.flush()\n")
file(CHMOD "${fake}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "TAPAS_SOLVE_PYTHON=${fake}"
    "${TAPAS}" "${SOURCE_ROOT}/test/solve/backend_error.tap"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
file(REMOVE "${fake}")
if(NOT result EQUAL 0 OR NOT output MATCHES "solve error: ok")
    message(FATAL_ERROR "Witness verification failed: ${output}${error}")
endif()
# Broken peers must yield an error, including EOF during send/read and invalid frames.
foreach(reply IN ITEMS "pass" "sys.stdout.buffer.write(bytes([255,255,255,255]))" "sys.stdout.buffer.write(bytes([0,0,0,8]) + b'bad')")
    file(WRITE "${fake}" "#!${SOLVE_PYTHON}\nimport sys\n${reply}\nsys.stdout.buffer.flush()\n")
    file(CHMOD "${fake}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "TAPAS_SOLVE_PYTHON=${fake}"
        "${TAPAS}" "${SOURCE_ROOT}/test/solve/backend_error.tap"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0 OR NOT output MATCHES "solve error: ok")
        message(FATAL_ERROR "Broken backend handling failed: ${output}${error}")
    endif()
endforeach()
file(REMOVE "${fake}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "TAPAS_SOLVE_PYTHON=${SOLVE_PYTHON}"
    "${TAPAS}" "${SOURCE_ROOT}/examples/solve/feasibility.tap"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT output MATCHES "quantity=3" OR NOT output MATCHES "status: sat")
    message(FATAL_ERROR "solve example failed: ${output}${error}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env
	"TAPAS_SOLVE_PYTHON=${SOLVE_PYTHON}"
	"${TAPAS}" "${SOURCE_ROOT}/examples/solve/generate_violations.tap"
	RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0
   OR NOT output MATCHES "current_stock :"
   OR NOT output MATCHES "status    = unsat")
	message(FATAL_ERROR
		"solve test-data example failed: ${output}${error}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env "TAPAS_SOLVE_PYTHON=${SOLVE_PYTHON}"
    "TAPAS_SOLVE_INT_MIN=0" "TAPAS_SOLVE_INT_MAX=3"
    "${TAPAS}" "${SOURCE_ROOT}/test/solve/configured.tap"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT output MATCHES "configured domain: ok")
    message(FATAL_ERROR "Configured domain failed: ${output}${error}")
endif()
foreach(invalid IN ITEMS "4" "oops" "" "9223372036854775808" " 1")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "TAPAS_SOLVE_PYTHON=${SOLVE_PYTHON}"
        "TAPAS_SOLVE_INT_MIN=${invalid}" "TAPAS_SOLVE_INT_MAX=3"
        "${TAPAS}" "${SOURCE_ROOT}/test/solve/backend_error.tap"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0 OR NOT output MATCHES "solve error: ok")
        message(FATAL_ERROR "Invalid configuration handling failed: ${output}${error}")
    endif()
endforeach()

set(diagnostics "${SOURCE_ROOT}/test/solve/diagnostics.tap")
execute_process(COMMAND "${TAPAS}" -c "${diagnostics}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "diagnostic bytecode compilation failed")
endif()
foreach(mode IN ITEMS source bytecode)
    if(mode STREQUAL "source")
        set(command "${diagnostics}")
    else()
        set(command -e "${diagnostics}c")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "TAPAS_SOLVE_PYTHON=${SOLVE_PYTHON}"
        "${TAPAS}" ${command} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0 OR NOT output MATCHES "quantity >= 8" OR NOT output MATCHES "quantity <= 4"
       OR output MATCHES "quantity != -1" OR NOT output MATCHES "x != z"
       OR NOT output MATCHES "count = 1000000000001 is outside" OR NOT output MATCHES "Rule 0, term")
        message(FATAL_ERROR "Diagnostic ${mode} output failed: ${output}${error}")
    endif()
endforeach()
file(REMOVE "${diagnostics}c")

set(structured "${SOURCE_ROOT}/test/solve/conflicts.tap")
execute_process(COMMAND "${TAPAS}" -c "${structured}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Structured conflict compilation failed")
endif()
foreach(mode IN ITEMS source bytecode)
    if(mode STREQUAL "source")
        set(command "${structured}")
    else()
        set(command -e "${structured}c")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "TAPAS_SOLVE_PYTHON=${SOLVE_PYTHON}"
        "${TAPAS}" ${command} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0 OR NOT output MATCHES "structured conflicts: ok")
        message(FATAL_ERROR "Structured conflicts ${mode} failed: ${output}${error}")
    endif()
endforeach()
file(REMOVE "${structured}c")

# Fixed record restrictions use the real backend and preserve witness identity.
set(records "${SOURCE_ROOT}/test/solve/records.tap")
execute_process(COMMAND "${TAPAS}" -c "${records}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Record query compilation failed")
endif()
foreach(mode IN ITEMS source bytecode)
    if(mode STREQUAL "source")
        set(command "${records}")
    else()
        set(command -e "${records}c")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "TAPAS_SOLVE_PYTHON=${SOLVE_PYTHON}"
        "${TAPAS}" ${command} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0 OR NOT output MATCHES "solve records: ok")
        message(FATAL_ERROR "Record query ${mode} failed: ${output}${error}")
    endif()
endforeach()
file(REMOVE "${records}c")
