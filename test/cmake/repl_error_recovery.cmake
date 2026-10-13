if(NOT DEFINED TAPAS OR NOT DEFINED WORK_DIR)
	message(FATAL_ERROR "TAPAS and WORK_DIR are required")
endif()

file(MAKE_DIRECTORY "${WORK_DIR}")
set(input "${WORK_DIR}/input.txt")
file(WRITE "${input}" "1 / 0\n[1][9]\n")

execute_process(
	COMMAND "${TAPAS}"
	INPUT_FILE "${input}"
	OUTPUT_VARIABLE output
	ERROR_VARIABLE error
	RESULT_VARIABLE status)

if(NOT status EQUAL 0)
	message(FATAL_ERROR "REPL did not recover from both errors: ${status}\n${output}${error}")
endif()
if(NOT error MATCHES "Divided by Integer Zero")
	message(FATAL_ERROR "first runtime error is missing: ${error}")
endif()
if(NOT error MATCHES "Index out of Range")
	message(FATAL_ERROR "second runtime error is missing: ${error}")
endif()
string(REGEX MATCHALL "instruction: [0-9]+" instructions "${error}")
list(LENGTH instructions instruction_count)
if(NOT instruction_count EQUAL 2)
	message(FATAL_ERROR
		"both recovered errors must retain VM instruction context: ${error}")
endif()
