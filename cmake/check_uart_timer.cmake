execute_process(COMMAND "${RUNNER}" "${IMAGE}" 200
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT status STREQUAL "0" OR NOT out MATCHES "^Meow!\n")
    message(FATAL_ERROR "UART/timer example failed (${status}):\n${out}${err}")
endif()
