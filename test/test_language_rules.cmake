if(NOT DEFINED TAPAS OR NOT DEFINED FIXTURE_DIR OR NOT DEFINED TEST_BLAS)
    message(FATAL_ERROR "TAPAS, FIXTURE_DIR, and TEST_BLAS are required")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "TAPAS_BLAS_LIBRARY=${TEST_BLAS}"
        "${TAPAS}" "${FIXTURE_DIR}/valid.tap"
    RESULT_VARIABLE valid_result
    OUTPUT_VARIABLE valid_output
    ERROR_VARIABLE valid_error
)
if(NOT valid_result EQUAL 0)
    message(FATAL_ERROR
        "Valid language-rules fixture failed (${valid_result}):\n${valid_output}${valid_error}")
endif()

set(expected_output [=[28
true
true
false
true
false
512
true
-3
2
-4
0.25
-4
4
2
0
10
0.5
5
1000
0.0015
0.001
3
2
3
[[11]]
[[4, 6]]
[[2, 2]]
[[2, 4]]
[[-1, -2]]
[[11, 12]]
[[3, 8]]
[[3, 2]]
[[0, 0]]
[[0, 1]]
[[1, 2]]
[[-2, 2]]
[[true, false, true]]
[[true, false, true]]
[[false, false, true]]
[[true, true, true]]
[[true, true, true]]
5
100
120
20
115
true
true
true
true
true
true
true
]=])
if(NOT valid_output STREQUAL expected_output)
    message(FATAL_ERROR
        "Unexpected output from valid language-rules fixture.\n"
        "Expected:\n${expected_output}\nActual:\n${valid_output}")
endif()

execute_process(
    COMMAND "${TAPAS}" "${FIXTURE_DIR}/runtime_types.tap"
    RESULT_VARIABLE runtime_types_result
    OUTPUT_VARIABLE runtime_types_output
    ERROR_VARIABLE runtime_types_error
)
if(NOT runtime_types_result EQUAL 0)
    message(FATAL_ERROR
        "Runtime Type fixture failed (${runtime_types_result}):\n"
        "${runtime_types_output}${runtime_types_error}")
endif()
set(expected_runtime_types_output [=[true
false
Type
2
true
true
true
true
false
true
2
true
false
true
true
true
false
true
false
true
false
2
2
1
point
true
false
true
false
true
true
2
]=])
if(NOT runtime_types_output STREQUAL expected_runtime_types_output)
    message(FATAL_ERROR
        "Unexpected Runtime Type output.\n"
        "Expected:\n${expected_runtime_types_output}\n"
        "Actual:\n${runtime_types_output}")
endif()

execute_process(
    COMMAND "${TAPAS}" "${FIXTURE_DIR}/static_types.tap"
    RESULT_VARIABLE static_types_result
    OUTPUT_VARIABLE static_types_output
    ERROR_VARIABLE static_types_error
)
if(NOT static_types_result EQUAL 0)
    message(FATAL_ERROR
        "Static Type fixture failed (${static_types_result}):\n"
        "${static_types_output}${static_types_error}")
endif()
set(expected_static_types_output [=[2.5
true
true
3
6.5
7
10.5
]=])
if(NOT static_types_output STREQUAL expected_static_types_output)
    message(FATAL_ERROR
        "Unexpected static Type output.\n"
        "Expected:\n${expected_static_types_output}\n"
        "Actual:\n${static_types_output}")
endif()

execute_process(
    COMMAND "${TAPAS}" "${FIXTURE_DIR}/compatibility_forms.tap"
    RESULT_VARIABLE compatibility_result
    OUTPUT_VARIABLE compatibility_output
    ERROR_VARIABLE compatibility_error
)
if(NOT compatibility_result EQUAL 0)
    message(FATAL_ERROR
        "Compatibility forms fixture failed (${compatibility_result}):\n"
        "${compatibility_output}${compatibility_error}")
endif()
set(expected_compatibility_output [=[7
5
42
3
]=])
if(NOT compatibility_output STREQUAL expected_compatibility_output)
    message(FATAL_ERROR
        "Unexpected output from compatibility forms fixture.\n"
        "Expected:\n${expected_compatibility_output}\n"
        "Actual:\n${compatibility_output}")
endif()

execute_process(
    COMMAND "${TAPAS}" "${FIXTURE_DIR}/ast_functions.tap"
    RESULT_VARIABLE ast_functions_result
    OUTPUT_VARIABLE ast_functions_output
    ERROR_VARIABLE ast_functions_error
)
if(NOT ast_functions_result EQUAL 0)
    message(FATAL_ERROR
        "AST function fixture failed (${ast_functions_result}):\n"
        "${ast_functions_output}${ast_functions_error}")
endif()
set(expected_ast_functions_output [=[11
12
3
8
]=])
if(NOT ast_functions_output STREQUAL expected_ast_functions_output)
    message(FATAL_ERROR
        "Unexpected AST function output.\n"
        "Expected:\n${expected_ast_functions_output}\n"
        "Actual:\n${ast_functions_output}")
endif()

execute_process(
    COMMAND "${TAPAS}" "${FIXTURE_DIR}/ast_import.tap"
    RESULT_VARIABLE ast_import_result
    OUTPUT_VARIABLE ast_import_output
    ERROR_VARIABLE ast_import_error
)
if(NOT ast_import_result EQUAL 0)
    message(FATAL_ERROR
        "AST import fixture failed (${ast_import_result}):\n"
        "${ast_import_output}${ast_import_error}")
endif()
if(NOT ast_import_output STREQUAL "42\ntrue\n2.5\n")
    message(FATAL_ERROR
        "Unexpected AST import output: ${ast_import_output}")
endif()

set(invalid_fixtures
    chained_comparison.tap
    chained_range.tap
    chained_membership.tap
    top_level_break.tap
    top_level_continue.tap
    nested_function_break.tap
    leading_zero.tap
    hexadecimal.tap
    binary.tap
    octal.tap
    removed_eig_package.tap
    removed_double_star.tap
    type_annotation_initializer.tap
    type_annotation_assignment.tap
    type_annotation_unknown.tap
    type_annotation_shadow.tap
    type_annotation_constructor.tap
    type_annotation_mutable_type.tap
    type_mutation.tap
    type_empty.tap
    type_invalid_field.tap
    type_structure_missing.tap
    type_structure_unknown.tap
    type_structure_duplicate.tap
    type_structure_mismatch.tap
    type_inferred_field_read.tap
    type_list_arity.tap
    type_pair_arity.tap
    type_dictionary_arity.tap
    type_cycle_a.tap
    type_constructor_indirect.tap
    type_constructor_package_alias.tap
    type_push_mismatch.tap
    type_delete_required.tap
    type_append_unknown.tap
    type_append_dictionary_mismatch.tap
)

foreach(fixture IN LISTS invalid_fixtures)
    execute_process(
        COMMAND "${TAPAS}" "${FIXTURE_DIR}/${fixture}"
        RESULT_VARIABLE invalid_result
        OUTPUT_VARIABLE invalid_output
        ERROR_VARIABLE invalid_error
    )
    if(invalid_result EQUAL 0)
        message(FATAL_ERROR "Invalid fixture ${fixture} unexpectedly compiled")
    endif()
    string(CONCAT diagnostic "${invalid_output}" "${invalid_error}")
    if(NOT diagnostic MATCHES "Compile Error")
        message(FATAL_ERROR
            "Invalid fixture ${fixture} failed without a compile diagnostic:\n${diagnostic}")
    endif()
endforeach()

set(runtime_error_fixtures
    array_modulo_shape.tap
    array_unary_plus.tap
    boolean_logic_type.tap
    boolean_logic_shape.tap
    boolean_array_short_circuit.tap
    time_float_offset.tap
    time_from_unix_float.tap
    time_reversed_add.tap
)

foreach(fixture IN LISTS runtime_error_fixtures)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
            "TAPAS_BLAS_LIBRARY=${TEST_BLAS}"
            "${TAPAS}" "${FIXTURE_DIR}/${fixture}"
        RESULT_VARIABLE runtime_result
        OUTPUT_VARIABLE runtime_output
        ERROR_VARIABLE runtime_error
    )
    if(runtime_result EQUAL 0)
        message(FATAL_ERROR "Runtime-error fixture ${fixture} unexpectedly succeeded")
    endif()
    string(CONCAT diagnostic "${runtime_output}" "${runtime_error}")
    if(NOT diagnostic MATCHES "Runtime Error")
        message(FATAL_ERROR
            "Fixture ${fixture} failed without a runtime diagnostic:\n${diagnostic}")
    endif()
endforeach()

set(no_blas_fixtures
    blas_required.tap
    blas_array_add.tap
    blas_array_sub.tap
    blas_scalar_mul.tap
    blas_array_neg.tap
)

foreach(fixture IN LISTS no_blas_fixtures)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
            "TAPAS_BLAS_LIBRARY=${FIXTURE_DIR}/missing-cblas-library"
            "${TAPAS}" "${FIXTURE_DIR}/${fixture}"
        RESULT_VARIABLE no_blas_result
        OUTPUT_VARIABLE no_blas_output
        ERROR_VARIABLE no_blas_error
    )
    if(no_blas_result EQUAL 0)
        message(FATAL_ERROR
            "BLAS-required fixture ${fixture} unexpectedly succeeded without BLAS")
    endif()
    string(CONCAT no_blas_diagnostic "${no_blas_output}" "${no_blas_error}")
    if(NOT no_blas_diagnostic MATCHES "Runtime Error" OR
       NOT no_blas_diagnostic MATCHES "BLAS")
        message(FATAL_ERROR
            "Missing-BLAS fixture ${fixture} did not report a BLAS runtime error:\n"
            "${no_blas_diagnostic}")
    endif()
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "TAPAS_BLAS_LIBRARY=${FIXTURE_DIR}/missing-cblas-library"
        "${TAPAS}" "${FIXTURE_DIR}/blas_independent.tap"
    RESULT_VARIABLE independent_result
    OUTPUT_VARIABLE independent_output
    ERROR_VARIABLE independent_error
)
if(NOT independent_result EQUAL 0)
    message(FATAL_ERROR
        "BLAS-independent array operations required BLAS unexpectedly:\n"
        "${independent_output}${independent_error}")
endif()
