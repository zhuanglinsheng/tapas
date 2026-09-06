if(NOT DEFINED TAPAS OR NOT DEFINED FIXTURE_DIR OR
   NOT DEFINED DOC_EXAMPLE_DIR OR NOT DEFINED TEST_BLAS)
    message(FATAL_ERROR
        "TAPAS, FIXTURE_DIR, DOC_EXAMPLE_DIR, and TEST_BLAS are required")
endif()

function(run_documented_example fixture expected)
    execute_process(
        COMMAND "${TAPAS}" "${DOC_EXAMPLE_DIR}/${fixture}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0 OR NOT output STREQUAL expected)
        message(FATAL_ERROR
            "Documented example ${fixture} failed (${result}):\n"
            "Expected:\n${expected}Actual:\n${output}${error}")
    endif()
endfunction()

set(REGRESSION_DIR "${FIXTURE_DIR}/regression")
set(SUPPORT_FIXTURE_DIR "${FIXTURE_DIR}/fixtures")
set(COMPILE_INVALID_DIR "${FIXTURE_DIR}/invalid/compile")
set(RUNTIME_INVALID_DIR "${FIXTURE_DIR}/invalid/runtime")
set(ENVIRONMENT_INVALID_DIR "${FIXTURE_DIR}/invalid/environment")

foreach(mode IN ITEMS source bytecode)
    if(mode STREQUAL "source")
        set(dictionary_command "${REGRESSION_DIR}/dictionary_mutation.tap")
    else()
        execute_process(COMMAND "${TAPAS}" -c "${REGRESSION_DIR}/dictionary_mutation.tap"
            RESULT_VARIABLE dictionary_compile)
        if(NOT dictionary_compile EQUAL 0)
            message(FATAL_ERROR "Dictionary mutation bytecode compilation failed")
        endif()
        set(dictionary_command -e "${REGRESSION_DIR}/dictionary_mutation.tapc")
    endif()
    execute_process(COMMAND "${TAPAS}" ${dictionary_command}
        RESULT_VARIABLE dictionary_result OUTPUT_VARIABLE dictionary_output ERROR_VARIABLE dictionary_error)
    if(NOT dictionary_result EQUAL 0 OR NOT dictionary_output STREQUAL "changed\n1\nnew\n")
        message(FATAL_ERROR "Dictionary mutation ${mode} failed: ${dictionary_output}${dictionary_error}")
    endif()
endforeach()
file(REMOVE "${REGRESSION_DIR}/dictionary_mutation.tapc")

include("${CMAKE_CURRENT_LIST_DIR}/logical_not.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/rule_not.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/rule_logic.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/rule_domains.cmake")

foreach(mode IN ITEMS source bytecode)
    if(mode STREQUAL "source")
        set(instance_command "${REGRESSION_DIR}/instance_of.tap")
    else()
        execute_process(COMMAND "${TAPAS}" -c "${REGRESSION_DIR}/instance_of.tap"
            RESULT_VARIABLE instance_compile)
        if(NOT instance_compile EQUAL 0)
            message(FATAL_ERROR "InstanceOf bytecode compilation failed")
        endif()
        set(instance_command -e "${REGRESSION_DIR}/instance_of.tapc")
    endif()
    execute_process(COMMAND "${TAPAS}" ${instance_command}
        RESULT_VARIABLE instance_result OUTPUT_VARIABLE instance_output ERROR_VARIABLE instance_error)
    if(NOT instance_result EQUAL 0)
        message(FATAL_ERROR "InstanceOf ${mode} failed: ${instance_output}${instance_error}")
    endif()
endforeach()
file(REMOVE "${REGRESSION_DIR}/instance_of.tapc")
foreach(fixture IN ITEMS call declaration result rule nested factory copy)
    execute_process(COMMAND "${TAPAS}" "${RUNTIME_INVALID_DIR}/instance_of_${fixture}.tap"
        RESULT_VARIABLE instance_result OUTPUT_VARIABLE instance_output ERROR_VARIABLE instance_error)
    if(instance_result EQUAL 0 OR NOT instance_error MATCHES "InstanceOf|Type")
        message(FATAL_ERROR "InstanceOf ${fixture} should fail: ${instance_output}${instance_error}")
    endif()
endforeach()
foreach(fixture IN ITEMS reassign non_rule syntax signature)
    execute_process(COMMAND "${TAPAS}" -c "${COMPILE_INVALID_DIR}/instance_of_${fixture}.tap"
        RESULT_VARIABLE instance_result OUTPUT_VARIABLE instance_output ERROR_VARIABLE instance_error)
    if(instance_result EQUAL 0)
        message(FATAL_ERROR "InstanceOf ${fixture} should fail compilation")
    endif()
endforeach()

execute_process(COMMAND "${TAPAS}" "${REGRESSION_DIR}/function_parameters.tap"
    RESULT_VARIABLE function_parameters_result OUTPUT_VARIABLE function_parameters_output
    ERROR_VARIABLE function_parameters_error)
if(NOT function_parameters_result EQUAL 0)
    message(FATAL_ERROR "Function parameters failed: ${function_parameters_output}${function_parameters_error}")
endif()
execute_process(COMMAND "${TAPAS}" -c "${REGRESSION_DIR}/function_parameters.tap"
    RESULT_VARIABLE function_compile_result)
execute_process(COMMAND "${TAPAS}" -e "${REGRESSION_DIR}/function_parameters.tapc"
    RESULT_VARIABLE function_reload_result OUTPUT_VARIABLE function_reload_output
    ERROR_VARIABLE function_reload_error)
file(REMOVE "${REGRESSION_DIR}/function_parameters.tapc")
if(NOT function_compile_result EQUAL 0 OR NOT function_reload_result EQUAL 0)
    message(FATAL_ERROR "Function metadata bytecode roundtrip failed: ${function_reload_output}${function_reload_error}")
endif()

execute_process(COMMAND "${TAPAS}" "${REGRESSION_DIR}/argument_reflection.tap"
    RESULT_VARIABLE reflection_result OUTPUT_VARIABLE reflection_output ERROR_VARIABLE reflection_error)
if(NOT reflection_result EQUAL 0)
    message(FATAL_ERROR "Argument reflection failed: ${reflection_output}${reflection_error}")
endif()

execute_process(COMMAND "${TAPAS}" "${REGRESSION_DIR}/rule_instance_implies.tap"
    RESULT_VARIABLE instance_implies_result OUTPUT_VARIABLE instance_implies_output
    ERROR_VARIABLE instance_implies_error)
if(NOT instance_implies_result EQUAL 0)
    message(FATAL_ERROR "RuleInstance antecedent regression failed: ${instance_implies_output}${instance_implies_error}")
endif()

execute_process(
    COMMAND "${TAPAS}" "${REGRESSION_DIR}/rule_implies.tap"
    RESULT_VARIABLE implies_result
    OUTPUT_VARIABLE implies_output
    ERROR_VARIABLE implies_error
)
if(NOT implies_result EQUAL 0 OR NOT implies_output STREQUAL
   "false\nfalse\n后续条件仍保持定位\n1\ntrue\ntrue\n2\nfalse\nfalse\n前件为假时不执行后件\n3\nfalse\nImplication\n2\ntrue\ntrue\nfalse\ntrue\n")
    message(FATAL_ERROR "Implication regression failed:\n${implies_output}${implies_error}")
endif()

execute_process(
    COMMAND "${TAPAS}" "${REGRESSION_DIR}/rule_import.tap"
    RESULT_VARIABLE rule_import_result
    OUTPUT_VARIABLE rule_import_output
    ERROR_VARIABLE rule_import_error
)
if(NOT rule_import_result EQUAL 0 OR
   NOT rule_import_output STREQUAL "true\nfalse\ntrue\nfalse\ntrue\ntrue\nfalse\n")
    message(FATAL_ERROR
        "Imported Rule Type fixture failed (${rule_import_result}):\n"
        "${rule_import_output}${rule_import_error}")
endif()

set(loop_control_bytecode "${REGRESSION_DIR}/loop_control.tapc")
execute_process(
    COMMAND "${TAPAS}" "${SUPPORT_FIXTURE_DIR}/implies_serialize.tap"
    RESULT_VARIABLE serialize_result
    OUTPUT_VARIABLE serialized
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT serialize_result EQUAL 0 OR NOT serialized MATCHES "^TPIR2;")
    message(FATAL_ERROR "Implication serialization failed")
endif()
get_filename_component(tapas_bin_dir "${TAPAS}" DIRECTORY)
set(restore_fixture "${tapas_bin_dir}/implies_restore.tap")
file(WRITE "${restore_fixture}"
    "let Check = rules::deserialize('${serialized}')\n"
    "assert(Check(false, false))\n"
    "assert(Check(true, true))\n"
    "print(rules::check(Check(true, false))['passed'])\n")
execute_process(COMMAND "${TAPAS}" "${restore_fixture}"
    RESULT_VARIABLE restore_result OUTPUT_VARIABLE restore_output ERROR_VARIABLE restore_error)
if(NOT restore_result EQUAL 0 OR NOT restore_output STREQUAL "false\n")
    message(FATAL_ERROR "Cross-process implication restore failed: ${restore_output}${restore_error}")
endif()
execute_process(
    COMMAND "${TAPAS}" "${SUPPORT_FIXTURE_DIR}/implies_instance_serialize.tap"
    RESULT_VARIABLE serialize_result OUTPUT_VARIABLE serialized OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT serialize_result EQUAL 0 OR NOT serialized MATCHES "^TPIR3;")
    message(FATAL_ERROR "RuleInstance antecedent serialization failed")
endif()
set(instance_restore_fixture "${tapas_bin_dir}/implies_instance_restore.tap")
file(WRITE "${instance_restore_fixture}"
    "let Check = rules::deserialize('${serialized}')\n"
    "let Premise = rule (p: Bool) { p }\n"
    "assert(Check(Premise(false)))\n"
    "print(rules::check(Check(Premise(true)))['passed'])\n")
execute_process(COMMAND "${TAPAS}" "${instance_restore_fixture}"
    RESULT_VARIABLE restore_result OUTPUT_VARIABLE restore_output ERROR_VARIABLE restore_error)
if(NOT restore_result EQUAL 0 OR NOT restore_output STREQUAL "false\n")
    message(FATAL_ERROR "Cross-process RuleInstance antecedent restore failed: ${restore_output}${restore_error}")
endif()
set(imported_ast_bytecode "${SUPPORT_FIXTURE_DIR}/imported_ast_module.tapc")
set(recursive_types_bytecode "${REGRESSION_DIR}/recursive_types.tapc")
file(REMOVE "${loop_control_bytecode}" "${imported_ast_bytecode}"
    "${recursive_types_bytecode}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "TAPAS_BLAS_LIBRARY=${TEST_BLAS}"
        "${TAPAS}" "${REGRESSION_DIR}/valid.tap"
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
[1, 2, 3, 4]
1
4
[2, 3]
[front]
[back]
[]
]=])
if(NOT valid_output STREQUAL expected_output)
    message(FATAL_ERROR
        "Unexpected output from valid language-rules fixture.\n"
        "Expected:\n${expected_output}\nActual:\n${valid_output}")
endif()

set(values_example_expected [=[Tap
[1, 2, 3, 4]
Ada
9.5
language: Tapas
[1, 2, 3, 4]
[10, 2, 3, 4]
]=])
run_documented_example("values_and_collections.tap" "${values_example_expected}")

set(operators_example_expected [=[14
512
-4
3
1
true
false
]=])
run_documented_example("operators.tap" "${operators_example_expected}")

set(modules_example_expected "double(3) = 6\nHello, Tapas!\n")
run_documented_example("../modules/main.tap" "${modules_example_expected}")

set(control_flow_example_expected "-1\n0\n1\n8\n0\n")
run_documented_example("control_flow.tap" "${control_flow_example_expected}")

execute_process(
    COMMAND "${TAPAS}" "${REGRESSION_DIR}/rule_valid.tap"
    RESULT_VARIABLE rule_result
    OUTPUT_VARIABLE rule_output
    ERROR_VARIABLE rule_error
)
set(rule_expected [=[false
1
value must be below ten
rule (value: Int) {
    "value must be positive":
        value > 0
}
true
false
true
true
Unsupported
]=])
if(NOT rule_result EQUAL 0 OR NOT rule_output STREQUAL rule_expected)
    message(FATAL_ERROR
        "Rule fixture failed (${rule_result}):\n"
        "Expected:\n${rule_expected}\nActual:\n${rule_output}${rule_error}")
endif()

execute_process(
    COMMAND "${TAPAS}" "${REGRESSION_DIR}/rule_dynamic.tap"
    RESULT_VARIABLE rule_dynamic_result
    OUTPUT_VARIABLE rule_dynamic_output
    ERROR_VARIABLE rule_dynamic_error
)
set(rule_dynamic_expected [=[true
false
1
5
true
true
true
7
Unsupported
1
number
true
true
true
1
4
]=])
if(NOT rule_dynamic_result EQUAL 0 OR
   NOT rule_dynamic_output STREQUAL rule_dynamic_expected)
    message(FATAL_ERROR
        "Dynamic Rule fixture failed (${rule_dynamic_result}):\n"
        "Expected:\n${rule_dynamic_expected}\n"
        "Actual:\n${rule_dynamic_output}${rule_dynamic_error}")
endif()

execute_process(
    COMMAND "${TAPAS}" "${REGRESSION_DIR}/rule_serialize.tap"
    RESULT_VARIABLE rule_serialize_result
    OUTPUT_VARIABLE rule_serialized
    ERROR_VARIABLE rule_serialize_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT rule_serialize_result EQUAL 0)
    message(FATAL_ERROR
        "Rule serialization failed (${rule_serialize_result}):\n"
        "${rule_serialize_error}")
endif()
set(rule_restore_source "${CMAKE_CURRENT_BINARY_DIR}/rule_restore.tap")
file(WRITE "${rule_restore_source}"
    "let restored = rules::deserialize('${rule_serialized}')\n"
    "print(rules::check(restored(5))['passed'])\n"
    "print(rules::check(restored(-1))['passed'])\n")
execute_process(
    COMMAND "${TAPAS}" "${rule_restore_source}"
    RESULT_VARIABLE rule_restore_result
    OUTPUT_VARIABLE rule_restore_output
    ERROR_VARIABLE rule_restore_error
)
file(REMOVE "${rule_restore_source}")
if(NOT rule_restore_result EQUAL 0 OR
   NOT rule_restore_output STREQUAL "true\nfalse\n")
    message(FATAL_ERROR
        "Rule deserialization failed (${rule_restore_result}):\n"
        "${rule_restore_output}${rule_restore_error}")
endif()

execute_process(
    COMMAND "${TAPAS}" "${RUNTIME_INVALID_DIR}/rule_failure.tap"
    RESULT_VARIABLE rule_failure_result
    OUTPUT_VARIABLE rule_failure_output
    ERROR_VARIABLE rule_failure_error
)
if(rule_failure_result EQUAL 0 OR
   NOT rule_failure_error MATCHES "value must be positive")
    message(FATAL_ERROR
        "Failing Rule did not produce its diagnostic (${rule_failure_result}):\n"
        "${rule_failure_output}${rule_failure_error}")
endif()

execute_process(
    COMMAND "${TAPAS}" "${RUNTIME_INVALID_DIR}/rule_cycle.tap"
    RESULT_VARIABLE rule_cycle_result
    OUTPUT_VARIABLE rule_cycle_output
    ERROR_VARIABLE rule_cycle_error
)
if(rule_cycle_result EQUAL 0 OR
   NOT rule_cycle_error MATCHES "cyclic requirement")
    message(FATAL_ERROR
        "Cyclic Rule dependency was not rejected (${rule_cycle_result}):\n"
        "${rule_cycle_output}${rule_cycle_error}")
endif()

set(io_output "${CMAKE_CURRENT_BINARY_DIR}/tapas_io_roundtrip.txt")
set(io_source "${CMAKE_CURRENT_BINARY_DIR}/tapas_io_roundtrip.tap")
file(WRITE "${io_source}"
    "print(input())\n"
    "print(input('prompt: '))\n"
    "print(input())\n"
    "io::write_text('${io_output}', 'alpha')\n"
    "io::append_text('${io_output}', 'beta')\n"
    "print(io::read_text('${io_output}'))\n")
execute_process(
    COMMAND "${TAPAS}" "${io_source}"
    INPUT_FILE "${SUPPORT_FIXTURE_DIR}/io_input.txt"
    RESULT_VARIABLE io_result
    OUTPUT_VARIABLE io_actual
    ERROR_VARIABLE io_error
)
file(REMOVE "${io_source}" "${io_output}")
set(io_expected "first line\nprompt: second line\nnil\nalphabeta\n")
if(NOT io_result EQUAL 0 OR NOT io_actual STREQUAL io_expected)
    message(FATAL_ERROR
        "Console and text I/O fixture failed (${io_result}):\n"
        "Expected:\n${io_expected}\nActual:\n${io_actual}${io_error}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "TAPAS_BLAS_LIBRARY=${TEST_BLAS}"
        "${TAPAS}" "${DOC_EXAMPLE_DIR}/dense_arrays.tap"
    RESULT_VARIABLE dense_result
    OUTPUT_VARIABLE dense_output
    ERROR_VARIABLE dense_error
)
set(expected_dense_output [=[5
30
5
[[0.6, 0.8]]
[[3, 4],
 [6, 8]]
[[1, 0, 0],
 [0, 1, 0],
 [0, 0, 1]]
[[1, 2]]
[[39, 46],
 [89, 104]]
]=])
if(NOT dense_result EQUAL 0 OR
   NOT dense_output STREQUAL expected_dense_output)
    message(FATAL_ERROR
        "Dense linear-algebra fixture failed (${dense_result}):\n"
        "Expected:\n${expected_dense_output}\nActual:\n${dense_output}${dense_error}")
endif()

execute_process(
    COMMAND "${TAPAS}" "${REGRESSION_DIR}/loop_control.tap"
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
    COMMAND "${TAPAS}" -c "${REGRESSION_DIR}/loop_control.tap"
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
    COMMAND "${TAPAS}" "${REGRESSION_DIR}/runtime_cache.tap"
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
    COMMAND "${TAPAS}" "${REGRESSION_DIR}/runtime_cache_bytecode.tap"
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
    COMMAND "${TAPAS}" "${REGRESSION_DIR}/clock_ns.tap"
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
    COMMAND "${TAPAS}" "${REGRESSION_DIR}/recursive_frames.tap"
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
    COMMAND "${TAPAS}" "${REGRESSION_DIR}/runtime_types.tap"
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
    COMMAND "${TAPAS}" "${REGRESSION_DIR}/enum_types.tap"
    RESULT_VARIABLE enum_types_result
    OUTPUT_VARIABLE enum_types_output
    ERROR_VARIABLE enum_types_error
)
if(NOT enum_types_result EQUAL 0)
    message(FATAL_ERROR
        "Enum Type fixture failed (${enum_types_result}):\n"
        "${enum_types_output}${enum_types_error}")
endif()
set(expected_enum_types_output [=[Delivered
Pending
Delivered
Delivered
true
true
false
true
4
[Pending, Shipped, Delivered, Cancelled]
Pending
Shipped
Delivered
Cancelled
Cancelled
Shipped
Shipped
[Pending, Delivered]
true
true
]=])
if(NOT enum_types_output STREQUAL expected_enum_types_output)
    message(FATAL_ERROR
        "Unexpected Enum Type output.\n"
        "Expected:\n${expected_enum_types_output}\n"
        "Actual:\n${enum_types_output}")
endif()

set(types_example_expected [=[2.5
3
true
true
true
true
Delivered
true
true
]=])
run_documented_example("types.tap" "${types_example_expected}")

execute_process(
    COMMAND "${TAPAS}" "${REGRESSION_DIR}/compatibility_forms.tap"
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

set(functions_example_expected "7\n120\n11\n12\n3\n")
run_documented_example("functions.tap" "${functions_example_expected}")

execute_process(
    COMMAND "${TAPAS}" "${REGRESSION_DIR}/function_types.tap"
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
    COMMAND "${TAPAS}" "${REGRESSION_DIR}/deferred_declarations.tap"
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
    COMMAND "${TAPAS}" "${REGRESSION_DIR}/definite_initialization.tap"
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
    COMMAND "${TAPAS}" "${REGRESSION_DIR}/recursive_types.tap"
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
    COMMAND "${TAPAS}" "${REGRESSION_DIR}/type_optional_iterator_narrowing.tap"
    RESULT_VARIABLE optional_iterator_result
    OUTPUT_VARIABLE optional_iterator_output
    ERROR_VARIABLE optional_iterator_error
)
if(NOT optional_iterator_result EQUAL 0 OR
   NOT optional_iterator_output STREQUAL "true\nage\ntrue\ntrue\n7\n3\n")
    message(FATAL_ERROR
        "Optional/Iterator/narrowing fixture failed:\n"
        "${optional_iterator_output}${optional_iterator_error}")
endif()

execute_process(
    COMMAND "${TAPAS}" -c "${REGRESSION_DIR}/recursive_types.tap"
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
    COMMAND "${TAPAS}" -c "${SUPPORT_FIXTURE_DIR}/imported_ast_module.tap"
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
    COMMAND "${TAPAS}" "${REGRESSION_DIR}/ast_import.tap"
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
if(NOT ast_import_output STREQUAL "42\ntrue\n2.5\nDelivered\nPending\n")
    message(FATAL_ERROR
        "Unexpected AST import output: ${ast_import_output}")
endif()

set(invalid_fixtures
    chained_comparison.tap
    chained_range.tap
    chained_membership.tap
    conditional_orphan_else.tap
    conditional_branch_after_else.tap
    conditional_semicolon_else.tap
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
    type_list_arity.tap
    type_pair_arity.tap
    type_dictionary_arity.tap
    type_dictionary_field_read.tap
    type_application_arity.tap
    type_application_base.tap
    type_application_argument.tap
    type_application_shadow.tap
    type_application_mismatch.tap
    type_cycle_a.tap
    type_constructor_indirect.tap
    type_constructor_package_alias.tap
    type_push_back_mismatch.tap
    type_delete_required.tap
    type_append_unknown.tap
    type_append_dictionary_mismatch.tap
    type_dictionary_key_write_mismatch.tap
    type_dictionary_index_mismatch.tap
    type_list_index_mismatch.tap
    type_union_syntax_mismatch.tap
    removed_function_literal_keyword.tap
    removed_kappa.tap
    function_argument_mismatch.tap
    function_return_mismatch.tap
    function_missing_return.tap
    function_type_mismatch.tap
    function_context_return_mismatch.tap
    uninitialized_read.tap
    uninitialized_after_if.tap
    uninitialized_after_while.tap
    uninitialized_after_for.tap
    recursive_type_alias_cycle.tap
    conditional_recursive_type.tap
    let_capture.tap
    time_from_unix_float.tap
)

list(APPEND invalid_fixtures
    implies_rule_value.tap implies_instance_body.tap
    retired_require.tap
    implies_guard_type.tap implies_body_type.tap implies_empty.tap
    implies_nested.tap implies_require.tap implies_outside.tap implies_chain.tap)
foreach(fixture IN LISTS invalid_fixtures)
    execute_process(
        COMMAND "${TAPAS}" "${COMPILE_INVALID_DIR}/${fixture}"
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
    dictionary_structure_argument.tap
    arguments_unbound.tap parameters_unsupported.tap
    implies_instance_error.tap implies_instance_recursion.tap
    implies_instance_if.tap
    implies_triggered_error.tap
    implies_dynamic_empty.tap
    implies_dynamic_type.tap
    rule_import_argument.tap
    array_modulo_shape.tap
    array_unary_plus.tap
    boolean_logic_type.tap
    boolean_logic_shape.tap
    boolean_array_short_circuit.tap
    time_float_offset.tap
    time_reversed_add.tap
    dense_inner_shape.tap
    dense_outer_matrix.tap
    dense_normalize_zero.tap
)

foreach(fixture IN LISTS runtime_error_fixtures)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
            "TAPAS_BLAS_LIBRARY=${TEST_BLAS}"
            "${TAPAS}" "${RUNTIME_INVALID_DIR}/${fixture}"
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
    blas_dense.tap
)

foreach(fixture IN LISTS no_blas_fixtures)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
            "TAPAS_BLAS_LIBRARY=${ENVIRONMENT_INVALID_DIR}/missing-cblas-library"
            "${TAPAS}" "${ENVIRONMENT_INVALID_DIR}/${fixture}"
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

set(blas_independent_fixtures
    blas_independent.tap
    blas_array_add.tap
    blas_array_sub.tap
    blas_scalar_mul.tap
    blas_array_neg.tap
)

foreach(fixture IN LISTS blas_independent_fixtures)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
            "TAPAS_BLAS_LIBRARY=${ENVIRONMENT_INVALID_DIR}/missing-cblas-library"
            "${TAPAS}" "${REGRESSION_DIR}/${fixture}"
        RESULT_VARIABLE independent_result
        OUTPUT_VARIABLE independent_output
        ERROR_VARIABLE independent_error
    )
    if(NOT independent_result EQUAL 0)
        message(FATAL_ERROR
            "BLAS-independent fixture ${fixture} required BLAS unexpectedly:\n"
            "${independent_output}${independent_error}")
    endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/rule_restrict.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/rule_transform.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/rule_expression_ir.cmake")
