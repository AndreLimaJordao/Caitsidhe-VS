execute_process(COMMAND "${CLANG}" --target=riscv32 -march=rv32i -mabi=ilp32
    -nostdlib -Wl,-Ttext=0x80000000 -Wl,--no-relax -Wl,-e,_start
    "${SOURCE}" -o "${ELF}"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "RV32I assembly failed: ${out} ${err}")
endif()
execute_process(COMMAND "${OBJCOPY}" -O binary "${ELF}" "${BIN}"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "RV32I objcopy failed: ${out} ${err}")
endif()
