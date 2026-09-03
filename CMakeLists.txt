cmake_minimum_required(VERSION 3.21)
project(tapas VERSION 0.1.0 LANGUAGES C)

include(CTest)
include(GNUInstallDirs)

# ============================================================================
# 1. Project Configuration
# ============================================================================

# C standard settings - enforce strict C23 compliance
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# glibc hides POSIX and X/Open declarations in strict ISO C modes unless the
# application requests the corresponding interface levels.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_definitions(
        _POSIX_C_SOURCE=200809L
        _XOPEN_SOURCE=700
    )
endif()

configure_file(
    include/tapas/version.h.in
    generated/tapas/version.h
    @ONLY
)

# ============================================================================
# 2. Compiler Flags
# ============================================================================

set(STRICT_C_FLAGS
    "-Wall"
    "-Wextra"
    "-Wpedantic"
    #"-Werror"                  # Treat all warnings as errors
    "-Wvla"                    # Warn about variable-length array usage
    "-Wimplicit-int"           # Warn about implicit int declarations
    "-Werror=implicit-function-declaration"  # Treat implicit fn decl as errors
)

# Apply compiler flags
string(REPLACE ";" " " STRICT_C_FLAGS_STRING "${STRICT_C_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${STRICT_C_FLAGS_STRING}")

# ============================================================================
# 3. Include Directories
# ============================================================================

# Make project headers available (header-only library in src/)
include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_path(READLINE_INCLUDE_DIR readline/readline.h)
find_library(READLINE_LIBRARY readline)
find_library(MATH_LIBRARY m)
find_program(NODE_EXECUTABLE node)

if(NOT READLINE_INCLUDE_DIR OR NOT READLINE_LIBRARY)
    message(FATAL_ERROR "GNU readline is required for the Tapas REPL")
endif()

# ============================================================================
# 4. Libraries
# ============================================================================

set(TAPAS_RUNTIME_SOURCES
    src/tbasis.c
    src/tbycs.c
    src/tval.c
    src/tenv.c
    src/tblas.c
    src/textension.c
    src/ds/tobj_vec.c
    src/ds/tobj_array.c
    src/ds/thashtbl.c
    src/runtime/capability.c
    src/runtime/tstr.c
    src/runtime/tlist.c
    src/runtime/tpair.c
    src/runtime/tdict.c
    src/runtime/titer.c
    src/runtime/tarray.c
    src/runtime/ttime.c
    src/runtime/tcfn.c
    src/runtime/ttype.c
    src/runtime/trule.c
    src/runtime/trule_ir.c
    src/runtime/tevaluator.c
)

set(TAPAS_STDLIB_SOURCES
    src/stdlib/tstdlib.c
    src/stdlib/builtins/console.c
    src/stdlib/builtins/conversion.c
    src/stdlib/builtins/list.c
    src/stdlib/builtins/array.c
    src/stdlib/builtins/pair.c
    src/stdlib/builtins/capability.c
    src/stdlib/builtins/iterator.c
    src/stdlib/builtins/dict.c
    src/stdlib/builtins/sort.c
    src/stdlib/builtins/objects.c
    src/stdlib/builtins/time.c
    src/stdlib/builtins/session.c
    src/stdlib/builtins/rules.c
    src/stdlib/dense/dense.c
    src/stdlib/io/io.c
    src/stdlib/syntax/syntax.c
    src/stdlib/math/math.c
    src/stdlib/time/time.c
    src/stdlib/types/types.c
    src/stdlib/rules/rules.c
    src/stdlib/evaluators/evaluators.c
)

# Front-end sources do not include VM or runtime object headers. The target
# links the standard extension because signatures and implementations share
# one descriptor by design.
set(TAPAS_FRONTEND_SOURCES
    src/compile/frontend/diagnostic.c
    src/compile/frontend/ast.c
    src/compile/frontend/control_flow.c
    src/compile/frontend/definite_assignment.c
    src/compile/frontend/parser.c
    src/compile/frontend/semantic.c
    src/compile/frontend/frontend.c
    src/compile/frontend/module.c
    src/compile/frontend/workspace.c
    src/compile/frontend/workspace_reference.c
    src/compile/types/static_type.c
    src/compile/types/type_constructor.c
    src/compile/types/type_info.c
    src/compile/types/type_check.c
)

set(TAPAS_BACKEND_SOURCES
    src/compile/backend/emit_helpers.c
    src/compile/backend/ast_emit.c
    src/compile/backend/type_emit.c
    src/compile/backend/statement_ast_emit.c
    src/compile/backend/context.c
    src/compile/backend/binding.c
    src/compile/backend/type_bridge.c
    src/compile/backend/source.c
)

add_library(tapas_strings STATIC src/ds/tstring.c src/tbuiltin.c)
target_include_directories(tapas_strings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(tapas_syntax STATIC
    src/compile/frontend/document.c
    src/compile/frontend/syntax.c
)
target_include_directories(tapas_syntax PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(tapas_syntax PUBLIC tapas_strings)

add_library(tapas_runtime STATIC ${TAPAS_RUNTIME_SOURCES})
target_include_directories(tapas_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(tapas_runtime PUBLIC tapas_strings ${CMAKE_DL_LIBS})
if(MATH_LIBRARY)
    target_link_libraries(tapas_runtime PUBLIC ${MATH_LIBRARY})
endif()

add_library(tapas_stdlib STATIC ${TAPAS_STDLIB_SOURCES})
target_include_directories(tapas_stdlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(tapas_stdlib PUBLIC tapas_runtime tapas_syntax)

add_library(tapas_frontend STATIC ${TAPAS_FRONTEND_SOURCES})
target_include_directories(tapas_frontend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(tapas_frontend PUBLIC tapas_strings tapas_syntax tapas_stdlib)

# ============================================================================
# 5. Executables
# ============================================================================

# Main executable - compile tapas.c as the CLI entry point and link the C
# implementation files under src/ against the public headers in include/tapas.
add_executable(tapas
    src/tapas.c
    src/cli/input_state.c
    ${TAPAS_BACKEND_SOURCES}
    src/tvm.c
    src/tsess.c
)
target_include_directories(tapas PRIVATE ${READLINE_INCLUDE_DIR})
target_link_libraries(tapas PRIVATE tapas_frontend ${READLINE_LIBRARY})
target_compile_definitions(tapas PRIVATE
    TAPAS_RELATIVE_STDLIB_DIR="../${CMAKE_INSTALL_DATADIR}/tapas/stdlib"
)

add_executable(tapas-language-server
    src/lsp/main.c
    src/lsp/json.c
    src/lsp/server.c
)
target_link_libraries(tapas-language-server PRIVATE tapas_frontend)
set_target_properties(tapas-language-server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(BUILD_TESTING)
    function(tapas_enable_test_assertions target)
        if(MSVC)
            target_compile_options(${target} PRIVATE /UNDEBUG)
        else()
            target_compile_options(${target} PRIVATE -UNDEBUG)
        endif()
    endfunction()

    add_library(test_cblas SHARED test/support/test_cblas.c)
    if(MATH_LIBRARY)
        target_link_libraries(test_cblas PRIVATE ${MATH_LIBRARY})
    endif()

    add_executable(test_compile_frontend test/unit/test_compile_frontend.c)
    target_link_libraries(test_compile_frontend PRIVATE tapas_frontend)
    tapas_enable_test_assertions(test_compile_frontend)
    add_test(NAME compile_frontend COMMAND test_compile_frontend)

    add_executable(test_language_server
        test/unit/test_language_server.c
        src/lsp/json.c
        src/lsp/server.c
    )
    target_link_libraries(test_language_server PRIVATE tapas_frontend)
    tapas_enable_test_assertions(test_language_server)
    add_test(NAME language_server COMMAND test_language_server)
    if(NODE_EXECUTABLE)
        add_test(
            NAME vscode_protocol
            COMMAND ${NODE_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/editors/vscode/test/protocol.test.js
                $<TARGET_FILE:tapas-language-server>
                $<TARGET_FILE:tapas>
        )
    endif()

    add_executable(test_data_structures test/unit/test_data_structures.c)
    target_link_libraries(test_data_structures PRIVATE tapas_stdlib)
    tapas_enable_test_assertions(test_data_structures)
    add_test(NAME data_structures
        COMMAND ${CMAKE_COMMAND} -E env
            "TAPAS_BLAS_LIBRARY=$<TARGET_FILE:test_cblas>"
            $<TARGET_FILE:test_data_structures>
    )

    add_test(
        NAME language_rules
        COMMAND ${CMAKE_COMMAND}
            -DTAPAS=$<TARGET_FILE:tapas>
            -DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/test/language_rules
            -DDOC_EXAMPLE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/docs/examples/syntax
            -DTEST_BLAS=$<TARGET_FILE:test_cblas>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/test/cmake/language_rules.cmake
    )

    add_test(
        NAME format_package
        COMMAND ${CMAKE_COMMAND}
            -DTAPAS=$<TARGET_FILE:tapas>
            -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/test-format
            -P ${CMAKE_CURRENT_SOURCE_DIR}/test/cmake/format.cmake
    )

    # General examples are standalone programs and form part of the public
    # language surface. Product examples live in sibling directories and have
    # their own end-to-end test harnesses.
    file(GLOB TAPAS_EXAMPLES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/general/*.tap"
    )
    foreach(EXAMPLE IN LISTS TAPAS_EXAMPLES)
        get_filename_component(EXAMPLE_NAME "${EXAMPLE}" NAME_WE)
        add_test(
            NAME "example_${EXAMPLE_NAME}"
            COMMAND ${CMAKE_COMMAND} -E env
                "TAPAS_BLAS_LIBRARY=$<TARGET_FILE:test_cblas>"
                $<TARGET_FILE:tapas> "${EXAMPLE}"
        )
        set_tests_properties("example_${EXAMPLE_NAME}" PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        )
    endforeach()

    # Markdown documentation is executable: all `tapas` fences in one file are
    # compiled and run in document order. The archived C++ implementation is
    # intentionally outside this documentation set.
    file(GLOB_RECURSE TAPAS_DOCUMENTATION CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/*.md"
    )
    list(APPEND TAPAS_DOCUMENTATION
        "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/CODE_OF_CONDUCT.md"
    )
    foreach(DOCUMENT IN LISTS TAPAS_DOCUMENTATION)
        file(RELATIVE_PATH DOCUMENT_RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${DOCUMENT}")
        string(REGEX REPLACE "[^A-Za-z0-9]+" "_" DOCUMENT_TEST_NAME "${DOCUMENT_RELATIVE}")
        add_test(
            NAME "documentation_${DOCUMENT_TEST_NAME}"
            COMMAND ${CMAKE_COMMAND}
                -DTAPAS=$<TARGET_FILE:tapas>
                -DDOCUMENT=${DOCUMENT}
                -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
                -DTEST_BLAS=$<TARGET_FILE:test_cblas>
                -P ${CMAKE_CURRENT_SOURCE_DIR}/test/cmake/documentation.cmake
        )
        set_tests_properties("documentation_${DOCUMENT_TEST_NAME}" PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            RESOURCE_LOCK source_bytecode
        )
    endforeach()
endif()

set_target_properties(tapas PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set(FORMAT_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_DATADIR}/tapas/stdlib/format")
set(FORMAT_BYTECODE "${FORMAT_BUILD_DIR}/format.tapc")
add_custom_command(
    OUTPUT "${FORMAT_BYTECODE}" "${FORMAT_BUILD_DIR}/__init__.tap"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${FORMAT_BUILD_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/src/stdlib/format/format.tap"
        "${FORMAT_BUILD_DIR}/format.tap"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/src/stdlib/format/__init__.tap"
        "${FORMAT_BUILD_DIR}/__init__.tap"
    COMMAND $<TARGET_FILE:tapas> -c "${FORMAT_BUILD_DIR}/format.tap"
    DEPENDS tapas
        "${CMAKE_CURRENT_SOURCE_DIR}/src/stdlib/format/format.tap"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/stdlib/format/__init__.tap"
    VERBATIM
)
add_custom_target(format_bytecode ALL DEPENDS
    "${FORMAT_BYTECODE}" "${FORMAT_BUILD_DIR}/__init__.tap"
)

# ============================================================================
# 6. Installation Targets
# ============================================================================

install(TARGETS tapas tapas-language-server
    RUNTIME DESTINATION bin
)
install(DIRECTORY src/stdlib/format
    DESTINATION ${CMAKE_INSTALL_DATADIR}/tapas/stdlib
    FILES_MATCHING PATTERN "*.tap"
)
install(FILES "${FORMAT_BYTECODE}"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/tapas/stdlib/format
)
