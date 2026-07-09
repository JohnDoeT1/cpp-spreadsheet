find_package(Java REQUIRED COMPONENTS Runtime)

if(NOT DEFINED ANTLR_EXECUTABLE)
    find_program(
        ANTLR_EXECUTABLE
        NAMES
            antlr-4.13.2-complete.jar
            antlr-4.13-complete.jar
            antlr-4.12.0-complete.jar
            antlr4.jar
            antlr.jar
    )
endif()

if(NOT EXISTS "${ANTLR_EXECUTABLE}")
    message(FATAL_ERROR "ANTLR jar not found: ${ANTLR_EXECUTABLE}")
endif()

execute_process(
    COMMAND
        ${Java_JAVA_EXECUTABLE}
        -jar
        ${ANTLR_EXECUTABLE}
    OUTPUT_VARIABLE ANTLR_COMMAND_OUTPUT
    ERROR_VARIABLE ANTLR_COMMAND_ERROR
    RESULT_VARIABLE ANTLR_COMMAND_RESULT
)

if(NOT ANTLR_COMMAND_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Unable to execute ANTLR:\n${ANTLR_COMMAND_ERROR}")
endif()

string(REGEX MATCH "Version [0-9]+(\\.[0-9]+)*"
       ANTLR_VERSION
       "${ANTLR_COMMAND_OUTPUT}")

string(REPLACE
       "Version "
       ""
       ANTLR_VERSION
       "${ANTLR_VERSION}")

macro(ANTLR_TARGET Name InputFile)

    cmake_parse_arguments(
        ANTLR
        "LEXER;PARSER;LISTENER;VISITOR"
        "PACKAGE;OUTPUT_DIRECTORY"
        "COMPILE_FLAGS"
        ${ARGN}
    )

    get_filename_component(INPUT_WE ${InputFile} NAME_WE)

    if(ANTLR_OUTPUT_DIRECTORY)
        set(OUTDIR ${ANTLR_OUTPUT_DIRECTORY})
    else()
        set(OUTDIR
            ${CMAKE_CURRENT_BINARY_DIR}/antlr4cpp_generated_src/${INPUT_WE})
    endif()

    set(outputs
        ${OUTDIR}/${INPUT_WE}Lexer.cpp
        ${OUTDIR}/${INPUT_WE}Lexer.h
        ${OUTDIR}/${INPUT_WE}Parser.cpp
        ${OUTDIR}/${INPUT_WE}Parser.h
    )

    if(ANTLR_LISTENER)
        list(APPEND outputs
            ${OUTDIR}/${INPUT_WE}BaseListener.cpp
            ${OUTDIR}/${INPUT_WE}BaseListener.h
            ${OUTDIR}/${INPUT_WE}Listener.cpp
            ${OUTDIR}/${INPUT_WE}Listener.h)
        list(APPEND ANTLR_COMPILE_FLAGS -listener)
    endif()

    if(ANTLR_VISITOR)
        list(APPEND outputs
            ${OUTDIR}/${INPUT_WE}BaseVisitor.cpp
            ${OUTDIR}/${INPUT_WE}BaseVisitor.h
            ${OUTDIR}/${INPUT_WE}Visitor.cpp
            ${OUTDIR}/${INPUT_WE}Visitor.h)
        list(APPEND ANTLR_COMPILE_FLAGS -visitor)
    endif()

    add_custom_command(
        OUTPUT ${outputs}

        COMMAND
            ${Java_JAVA_EXECUTABLE}
            -jar
            ${ANTLR_EXECUTABLE}
            ${InputFile}
            -Dlanguage=Cpp
            -o
            ${OUTDIR}
            ${ANTLR_COMPILE_FLAGS}

        DEPENDS
            ${InputFile}

        WORKING_DIRECTORY
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    set(ANTLR_${Name}_CXX_OUTPUTS ${outputs} PARENT_SCOPE)
    set(ANTLR_${Name}_OUTPUT_DIR ${OUTDIR} PARENT_SCOPE)

endmacro()

set(ANTLR4_INCLUDE_DIRS "C:/ANTLR/runtime/src")

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
    ANTLR
    REQUIRED_VARS
        ANTLR_EXECUTABLE
        Java_JAVA_EXECUTABLE
    VERSION_VAR
        ANTLR_VERSION
)