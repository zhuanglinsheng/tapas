cmake_minimum_required(VERSION 3.10)
project(tapas C)

include(CTest)

# ============================================================================
# 1. Project Configuration
# ============================================================================

# C standard settings - enforce strict C23 compliance
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Project version information
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 1)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})

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
# 4. Reusable Compiler Front End
# ============================================================================

# Kept independent from VM/compiler state so editor tooling can parse and
# analyze source without linking the Tapas runtime.
add_library(tapas_frontend STATIC
    src/ds/tstring.c
    src/compile/document.c
    src/compile/diagnostic.c
    src/compile/syntax.c
    src/compile/ast.c
    src/compile/parser.c
    src/compile/semantic.c
    src/compile/type_info.c
    src/compile/frontend.c
    src/compile/module.c
    src/compile/workspace.c
)
target_include_directories(tapas_frontend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ============================================================================
# 5. Executables
# ============================================================================

# Main executable - compile tapas.c as the CLI entry point and link the C
# implementation files under src/ against the public headers in include/tapas.
add_executable(tapas
    src/tapas.c
    src/tbasis.c
    src/tbycs.c
    src/tval.c
    src/tenv.c
    src/compile/lexer.c
    src/compile/ast_emit.c
    src/compile/ast_typecheck.c
    src/compile/statement_ast_emit.c
    src/compile/context.c
    src/compile/binding.c
    src/compile/typecheck.c
    src/compile/expression.c
    src/compile/statement.c
    src/compile/dispatch.c
    src/compile/source.c
    src/tvm.c
    src/tblas.c
    src/tsess.c
    src/ds/tobj_vec.c
    src/ds/tobj_array.c
    src/ds/thashtbl.c
    src/runtime/tstr.c
    src/runtime/tlist.c
    src/runtime/tpair.c
    src/runtime/tdict.c
    src/runtime/titer.c
    src/runtime/tarray.c
    src/runtime/ttime.c
    src/runtime/tcfn.c
    src/runtime/ttype.c
)
target_include_directories(tapas PRIVATE ${READLINE_INCLUDE_DIR})
target_link_libraries(tapas PRIVATE tapas_frontend ${READLINE_LIBRARY} ${CMAKE_DL_LIBS})
if(MATH_LIBRARY)
    target_link_libraries(tapas PRIVATE ${MATH_LIBRARY})
endif()

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
    add_library(test_cblas SHARED test/test_cblas.c)

    add_executable(test_compile_frontend test/test_compile_frontend.c)
    target_link_libraries(test_compile_frontend PRIVATE tapas_frontend)
    add_test(NAME compile_frontend COMMAND test_compile_frontend)

    add_executable(test_language_server
        test/test_language_server.c
        src/lsp/json.c
        src/lsp/server.c
    )
    target_link_libraries(test_language_server PRIVATE tapas_frontend)
    add_test(NAME language_server COMMAND test_language_server)
    if(NODE_EXECUTABLE)
        add_test(
            NAME vscode_protocol
            COMMAND ${NODE_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/editors/vscode/test/protocol.test.js
                $<TARGET_FILE:tapas-language-server>
        )
    endif()

    add_executable(test_data_structures
        test/test_data_structures.c
        src/tbasis.c
        src/tval.c
        src/tblas.c
        src/ds/tstring.c
        src/ds/tobj_vec.c
        src/ds/tobj_array.c
        src/ds/thashtbl.c
        src/runtime/tstr.c
        src/runtime/tlist.c
        src/runtime/tpair.c
        src/runtime/tdict.c
        src/runtime/titer.c
        src/runtime/tarray.c
        src/runtime/ttime.c
        src/runtime/tcfn.c
        src/runtime/ttype.c
        )
    if(MATH_LIBRARY)
        target_link_libraries(test_data_structures PRIVATE ${MATH_LIBRARY})
    endif()
    target_link_libraries(test_data_structures PRIVATE ${CMAKE_DL_LIBS})
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
            -DTEST_BLAS=$<TARGET_FILE:test_cblas>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/test/test_language_rules.cmake
    )

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
            COMMAND ${CMAKE_COMMAND} -E env
                "TAPAS_BLAS_LIBRARY=$<TARGET_FILE:test_cblas>"
                $<TARGET_FILE:tapas> --stdout "${DOCUMENT}"
        )
        set_tests_properties("documentation_${DOCUMENT_TEST_NAME}" PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        )
    endforeach()
endif()

set_target_properties(tapas PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ============================================================================
# 6. Installation Targets
# ============================================================================

install(TARGETS tapas tapas-language-server
    RUNTIME DESTINATION bin
)
