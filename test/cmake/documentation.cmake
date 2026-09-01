if(NOT DEFINED TAPAS OR NOT DEFINED DOCUMENT OR
   NOT DEFINED SOURCE_ROOT OR NOT DEFINED TEST_BLAS)
    message(FATAL_ERROR
        "TAPAS, DOCUMENT, SOURCE_ROOT, and TEST_BLAS are required")
endif()

file(GLOB_RECURSE generated_bytecode LIST_DIRECTORIES false
    "${SOURCE_ROOT}/docs/*.tapc")
if(generated_bytecode)
    file(REMOVE ${generated_bytecode})
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "TAPAS_BLAS_LIBRARY=${TEST_BLAS}"
        "${TAPAS}" --stdout "${DOCUMENT}"
    WORKING_DIRECTORY "${SOURCE_ROOT}"
    RESULT_VARIABLE document_result
    OUTPUT_VARIABLE document_output
    ERROR_VARIABLE document_error
)

file(GLOB_RECURSE generated_bytecode LIST_DIRECTORIES false
    "${SOURCE_ROOT}/docs/*.tapc")
if(generated_bytecode)
    file(REMOVE ${generated_bytecode})
endif()

if(NOT document_result EQUAL 0)
    message(FATAL_ERROR
        "Documentation execution failed (${document_result}):\n"
        "${document_output}${document_error}")
endif()
