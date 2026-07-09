cmake_minimum_required(VERSION 3.8)

project(spreadsheet)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ANTLR_BUILD_CPP_TESTS OFF CACHE BOOL "Disable ANTLR tests" FORCE)
set(ANTLR_BUILD_SHARED OFF CACHE BOOL "Disable shared runtime" FORCE)
set(ANTLR_BUILD_STATIC ON CACHE BOOL "Build static runtime" FORCE)

if(MSVC)
    add_definitions(
        -DANTLR4CPP_STATIC
        -D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
    )
else()
    add_compile_options(
        -Wall
        -Wextra
        -pedantic
    )
endif()

# --------------------------------------------------------------------------
# ANTLR
# --------------------------------------------------------------------------

set(ANTLR_EXECUTABLE "C:/ANTLR/antlr-4.13.2-complete.jar")

include(${CMAKE_CURRENT_SOURCE_DIR}/FindANTLR.cmake)

set(WITH_STATIC_CRT OFF CACHE BOOL "" FORCE)

add_subdirectory(
    "C:/ANTLR/runtime"
    "${CMAKE_BINARY_DIR}/antlr4_runtime"
)

target_include_directories(
    antlr4_static
    PUBLIC
    "C:/ANTLR/runtime/src"
)

antlr_target(
    FormulaParser
    ${CMAKE_CURRENT_SOURCE_DIR}/Formula.g4
    LEXER
    PARSER
    LISTENER
)

include_directories(
    ${ANTLR4_INCLUDE_DIRS}
    ${ANTLR_FormulaParser_OUTPUT_DIR}
    "C:/ANTLR/runtime/src"
)

file(GLOB sources
    "*.cpp"
    "*.h"
)

list(FILTER sources EXCLUDE REGEX ".*Formula\\.(g4|interp|tokens)")

add_executable(
    spreadsheet
    ${sources}
    ${ANTLR_FormulaParser_CXX_OUTPUTS}
)

target_link_libraries(
    spreadsheet
    PRIVATE
    antlr4_static
)

if(MSVC)
    target_compile_options(antlr4_static PRIVATE /W0)
endif()

install(
    TARGETS spreadsheet
    DESTINATION bin
)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT spreadsheet)