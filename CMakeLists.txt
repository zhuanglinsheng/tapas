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

if(NOT READLINE_INCLUDE_DIR OR NOT READLINE_LIBRARY)
    message(FATAL_ERROR "GNU readline is required for the Tapas REPL")
endif()

# ============================================================================
# 4. Tapas Executable
# ============================================================================

# Main executable - compile tapas.c as the CLI entry point and link the C
# implementation files under src/ against the public headers in include/tapas.
add_executable(tapas
    src/tapas.c
    src/tbasis.c
    src/tbycs.c
    src/tval.c
    src/tenv.c
    src/tlex.c
    src/tcompile.c
    src/tvm.c
    src/tsess.c
    src/ds/tstring.c
    src/ds/tobj_vec.c
    src/ds/thashtbl.c
    src/ds/tarray.c
)
target_include_directories(tapas PRIVATE ${READLINE_INCLUDE_DIR})
target_link_libraries(tapas PRIVATE ${READLINE_LIBRARY})
if(MATH_LIBRARY)
    target_link_libraries(tapas PRIVATE ${MATH_LIBRARY})
endif()

if(BUILD_TESTING)
    add_executable(test_data_structures
        test/test_data_structures.c
        src/tbasis.c
        src/tval.c
        src/ds/tstring.c
        src/ds/tobj_vec.c
        src/ds/thashtbl.c
        src/ds/tarray.c
    )
    if(MATH_LIBRARY)
        target_link_libraries(test_data_structures PRIVATE ${MATH_LIBRARY})
    endif()
    add_test(NAME data_structures COMMAND test_data_structures)
endif()

set_target_properties(tapas PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ============================================================================
# 5. Installation Targets
# ============================================================================

install(TARGETS tapas
    RUNTIME DESTINATION bin
)
