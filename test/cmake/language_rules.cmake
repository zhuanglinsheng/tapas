if(NOT DEFINED TAPAS OR NOT DEFINED FIXTURE_DIR OR NOT DEFINED TEST_BLAS)
    message(FATAL_ERROR "TAPAS, FIXTURE_DIR, and TEST_BLAS are required")
endif()

set(loop_control_bytecode "${FIXTURE_DIR}/loop_control.tapc")
set(imported_ast_bytecode "${FIXTURE_DIR}/imported_ast_module.tapc")
set(recursive_types_bytecode "${FIXTURE_DIR}/recursive_types.tapc")
file(REMOVE "${loop_control_bytecode}" "${imported_ast_bytecode}"
    "${recursive_types_bytecode}")

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
    COMMAND "${TAPAS}" "${FIXTURE_DIR}/loop_control.tap"
    RESULT_VARIABLE loop_control_result
    OUTPUT_VARIABLE loop_control_output
    ERROR_VARIABLE loop_control_error
)
if(NOT loop_control_result EQUAL 0)
    message(FATAL_ERROR
        "Loop-control fixture failed (${loop_control_result}):\n"
        "${loop_control_output}${loop_control_error}")
endif()
if(NOT loop_control_output STREQUAL "106\n8\n")
    message(FATAL_ERROR
        "Unexpected loop-control output: ${loop_control_output}")
endif()

execute_process(
    COMMAND "${TAPAS}" -c "${FIXTURE_DIR}/loop_control.tap"
    RESULT_VARIABLE loop_control_compile_result
    OUTPUT_VARIABLE loop_control_compile_output
    ERROR_VARIABLE loop_control_compile_error
)
if(NOT loop_control_compile_result EQUAL 0)
    message(FATAL_ERROR
        "Loop-control bytecode compilation failed (${loop_control_compile_result}):\n"
        "${loop_control_compile_output}${loop_control_compile_error}")
endif()
execute_process(
    COMMAND "${TAPAS}" -e "${loop_control_bytecode}"
    RESULT_VARIABLE loop_control_bytecode_result
    OUTPUT_VARIABLE loop_control_bytecode_output
    ERROR_VARIABLE loop_control_bytecode_error
)
file(REMOVE "${loop_control_bytecode}")
if(NOT loop_control_bytecode_result EQUAL 0)
    message(FATAL_ERROR
        "Fresh loop-control bytecode failed (${loop_control_bytecode_result}):\n"
        "${loop_control_bytecode_output}${loop_control_bytecode_error}")
endif()
if(NOT loop_control_bytecode_output STREQUAL "106\n8\n")
    message(FATAL_ERROR
        "Unexpected fresh loop-control bytecode output: ${loop_control_bytecode_output}")
endif()

execute_process(
    COMMAND "${TAPAS}" "${FIXTURE_DIR}/runtime_cache.tap"
    RESULT_VARIABLE runtime_cache_result
    OUTPUT_VARIABLE runtime_cache_output
    ERROR_VARIABLE runtime_cache_error
)
if(NOT runtime_cache_result EQUAL 0)
    message(FATAL_ERROR
        "Runtime-cache fixture failed (${runtime_cache_result}):\n"
        "${runtime_cache_output}${runtime_cache_error}")
endif()
if(NOT runtime_cache_output STREQUAL "6\n6\n9\n6\n7\na\n9\n5\n5\n8\n4\n9\nfalse\ntrue\n")
    message(FATAL_ERROR
        "Unexpected runtime-cache output: ${runtime_cache_output}")
endif()

execute_process(
    COMMAND "${TAPAS}" "${FIXTURE_DIR}/runtime_cache_bytecode.tap"
    RESULT_VARIABLE runtime_cache_bytecode_result
    OUTPUT_VARIABLE runtime_cache_bytecode_output
    ERROR_VARIABLE runtime_cache_bytecode_error
)
if(NOT runtime_cache_bytecode_result EQUAL 0)
    message(FATAL_ERROR
        "Runtime-cache bytecode fixture failed (${runtime_cache_bytecode_result}):\n"
        "${runtime_cache_bytecode_output}${runtime_cache_bytecode_error}")
endif()
string(FIND "${runtime_cache_bytecode_output}" "OP_LOOPAS" loopas_position)
if(loopas_position EQUAL -1)
    message(FATAL_ERROR
        "Loop bytecode is missing OP_LOOPAS:\n${runtime_cache_bytecode_output}")
endif()

execute_process(
    COMMAND "${TAPAS}" "${FIXTURE_DIR}/clock_ns.tap"
    RESULT_VARIABLE clock_ns_result
    OUTPUT_VARIABLE clock_ns_output
    ERROR_VARIABLE clock_ns_error
)
if(NOT clock_ns_result EQUAL 0)
    message(FATAL_ERROR
        "clock_ns fixture failed (${clock_ns_result}):\n"
        "${clock_ns_output}${clock_ns_error}")
endif()
if(NOT clock_ns_output STREQUAL "int\ntrue\n")
    message(FATAL_ERROR
        "Unexpected clock_ns output: ${clock_ns_output}")
endif()

execute_process(
    COMMAND "${TAPAS}" "${FIXTURE_DIR}/recursive_frames.tap"
    RESULT_VARIABLE recursive_frames_result
    OUTPUT_VARIABLE recursive_frames_output
    ERROR_VARIABLE recursive_frames_error
)
if(NOT recursive_frames_result EQUAL 0)
    message(FATAL_ERROR
        "Recursive-frame fixture failed (${recursive_frames_result}):\n"
        "${recursive_frames_output}${recursive_frames_error}")
endif()
if(NOT recursive_frames_output STREQUAL "24\n1\n2\n1\n50000\n10000\n")
    message(FATAL_ERROR
        "Unexpected recursive-frame output: ${recursive_frames_output}")
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
    COMMAND "${TAPAS}" "${FIXTURE_DIR}/type_applications.tap"
    RESULT_VARIABLE type_applications_result
    OUTPUT_VARIABLE type_applications_output
    ERROR_VARIABLE type_applications_error
)
if(NOT type_applications_result EQUAL 0)
    message(FATAL_ERROR
        "Parameterized Type application fixture failed (${type_applications_result}):\n"
        "${type_applications_output}${type_applications_error}")
endif()
set(expected_type_applications_output [=[true
true
true
true
true
]=])
if(NOT type_applications_output STREQUAL expected_type_applications_output)
    message(FATAL_ERROR
        "Unexpected parameterized Type application output.\n"
        "Expected:\n${expected_type_applications_output}\n"
        "Actual:\n${type_applications_output}")
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
    COMMAND "${TAPAS}" "${FIXTURE_DIR}/function_annotations.tap"
    RESULT_VARIABLE function_annotations_result
    OUTPUT_VARIABLE function_annotations_output
    ERROR_VARIABLE function_annotations_error
)
if(NOT function_annotations_result EQUAL 0)
    message(FATAL_ERROR
        "Function annotation fixture failed (${function_annotations_result}):\n"
        "${function_annotations_output}${function_annotations_error}")
endif()
if(NOT function_annotations_output STREQUAL "7\n120\n3\n9\n")
    message(FATAL_ERROR
        "Unexpected function annotation output: ${function_annotations_output}")
endif()

execute_process(
    COMMAND "${TAPAS}" "${FIXTURE_DIR}/function_types.tap"
    RESULT_VARIABLE function_types_result
    OUTPUT_VARIABLE function_types_output
    ERROR_VARIABLE function_types_error
)
if(NOT function_types_result EQUAL 0)
    message(FATAL_ERROR
        "Function Type fixture failed (${function_types_result}):\n"
        "${function_types_output}${function_types_error}")
endif()
if(NOT function_types_output STREQUAL "7\nvalue\n")
    message(FATAL_ERROR
        "Unexpected Function Type output: ${function_types_output}")
endif()

execute_process(
    COMMAND "${TAPAS}" "${FIXTURE_DIR}/deferred_declarations.tap"
    RESULT_VARIABLE deferred_declarations_result
    OUTPUT_VARIABLE deferred_declarations_output
    ERROR_VARIABLE deferred_declarations_error
)
if(NOT deferred_declarations_result EQUAL 0 OR
   NOT deferred_declarations_output STREQUAL "3\n4\nready\n11\n")
    message(FATAL_ERROR
        "Deferred declaration fixture failed:\n"
        "${deferred_declarations_output}${deferred_declarations_error}")
endif()

execute_process(
    COMMAND "${TAPAS}" "${FIXTURE_DIR}/definite_initialization.tap"
    RESULT_VARIABLE definite_initialization_result
    OUTPUT_VARIABLE definite_initialization_output
    ERROR_VARIABLE definite_initialization_error
)
if(NOT definite_initialization_result EQUAL 0 OR
   NOT definite_initialization_output STREQUAL "1\n4\n6\n7\n")
    message(FATAL_ERROR
        "Definite-initialization fixture failed (${definite_initialization_result}):\n"
        "${definite_initialization_output}${definite_initialization_error}")
endif()

execute_process(
    COMMAND "${TAPAS}" "${FIXTURE_DIR}/recursive_types.tap"
    RESULT_VARIABLE recursive_types_result
    OUTPUT_VARIABLE recursive_types_output
    ERROR_VARIABLE recursive_types_error
)
if(NOT recursive_types_result EQUAL 0 OR
   NOT recursive_types_output STREQUAL "true\nfalse\ntrue\ntrue\n")
    message(FATAL_ERROR
        "Recursive Type fixture failed:\n"
        "${recursive_types_output}${recursive_types_error}")
endif()

execute_process(
    COMMAND "${TAPAS}" -c "${FIXTURE_DIR}/recursive_types.tap"
    RESULT_VARIABLE recursive_types_compile_result
    OUTPUT_VARIABLE recursive_types_compile_output
    ERROR_VARIABLE recursive_types_compile_error
)
execute_process(
    COMMAND "${TAPAS}" -e "${recursive_types_bytecode}"
    RESULT_VARIABLE recursive_types_bytecode_result
    OUTPUT_VARIABLE recursive_types_bytecode_output
    ERROR_VARIABLE recursive_types_bytecode_error
)
file(REMOVE "${recursive_types_bytecode}")
if(NOT recursive_types_compile_result EQUAL 0 OR
   NOT recursive_types_bytecode_result EQUAL 0 OR
   NOT recursive_types_bytecode_output STREQUAL "true\nfalse\ntrue\ntrue\n")
    message(FATAL_ERROR
        "Recursive Type bytecode fixture failed:\n"
        "${recursive_types_compile_output}${recursive_types_compile_error}"
        "${recursive_types_bytecode_output}${recursive_types_bytecode_error}")
endif()

execute_process(
    COMMAND "${TAPAS}" -c "${FIXTURE_DIR}/imported_ast_module.tap"
    RESULT_VARIABLE ast_module_compile_result
    OUTPUT_VARIABLE ast_module_compile_output
    ERROR_VARIABLE ast_module_compile_error
)
if(NOT ast_module_compile_result EQUAL 0)
    message(FATAL_ERROR
        "Imported AST module compilation failed (${ast_module_compile_result}):\n"
        "${ast_module_compile_output}${ast_module_compile_error}")
endif()

execute_process(
    COMMAND "${TAPAS}" "${FIXTURE_DIR}/ast_import.tap"
    RESULT_VARIABLE ast_import_result
    OUTPUT_VARIABLE ast_import_output
    ERROR_VARIABLE ast_import_error
)
file(REMOVE "${imported_ast_bytecode}")
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
    type_application_arity.tap
    type_application_base.tap
    type_application_argument.tap
    type_application_shadow.tap
    type_application_mismatch.tap
    type_cycle_a.tap
    type_constructor_indirect.tap
    type_constructor_package_alias.tap
    type_push_mismatch.tap
    type_delete_required.tap
    type_append_unknown.tap
    type_append_dictionary_mismatch.tap
    removed_function_literal_keyword.tap
    removed_kappa.tap
    function_argument_mismatch.tap
    function_return_mismatch.tap
    function_missing_return.tap
    function_type_mismatch.tap
    uninitialized_read.tap
    uninitialized_after_if.tap
    uninitialized_after_while.tap
    uninitialized_after_for.tap
    recursive_type_alias_cycle.tap
    conditional_recursive_type.tap
    let_capture.tap
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
    if(NOT diagnostic MATCHES "${fixture}" OR
       NOT diagnostic MATCHES "instruction: [0-9]+")
        message(FATAL_ERROR
            "Fixture ${fixture} lost its runtime source location:\n${diagnostic}")
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
