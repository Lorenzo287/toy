cmake_minimum_required(VERSION 3.10)

foreach(REQUIRED_VARIABLE TOY_EXECUTABLE TEST_SOURCE TEST_SOURCE_DIR WORK_DIR)
    if(NOT DEFINED ${REQUIRED_VARIABLE} OR "${${REQUIRED_VARIABLE}}" STREQUAL "")
        message(FATAL_ERROR
            "RunDebugProtocol.cmake requires ${REQUIRED_VARIABLE}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${WORK_DIR}")
set(COMMANDS_FILE "${WORK_DIR}/commands.txt")
file(WRITE "${COMMANDS_FILE}"
    "clear-breakpoints\nbreak 4\ncontinue\nstep\ncontinue\n")

execute_process(
    COMMAND "${TOY_EXECUTABLE}" --debug-protocol "${TEST_SOURCE}"
    WORKING_DIRECTORY "${TEST_SOURCE_DIR}"
    INPUT_FILE "${COMMANDS_FILE}"
    OUTPUT_VARIABLE STDOUT
    ERROR_VARIABLE STDERR
    RESULT_VARIABLE RESULT
)

if(NOT "${RESULT}" STREQUAL "0")
    message(FATAL_ERROR
        "debug protocol exited with ${RESULT}\n"
        "--- stdout ---\n${STDOUT}\n"
        "--- stderr ---\n${STDERR}")
endif()

foreach(EXPECTED
        "\"event\":\"stopped\",\"reason\":\"entry\""
        "\"event\":\"stopped\",\"reason\":\"breakpoint\""
        "\"line\":4"
        "\"value\":\"0\""
        "\"event\":\"terminated\",\"exitCode\":0")
    string(FIND "${STDOUT}" "${EXPECTED}" POSITION)
    if(POSITION EQUAL -1)
        message(FATAL_ERROR
            "debug protocol output is missing ${EXPECTED}\n"
            "--- stdout ---\n${STDOUT}\n"
            "--- stderr ---\n${STDERR}")
    endif()
endforeach()
