execute_process(COMMAND "${RUNNER}" "${IMAGE}" 2000 --machine --wait-states "${WAIT}"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT status STREQUAL "0" OR NOT out STREQUAL "Meow!\nRESULT: PASS (tohost=1)\n")
    message(FATAL_ERROR "Timer interrupt demo failed (${status}):\n${out}${err}")
endif()
