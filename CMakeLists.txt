cmake_minimum_required(VERSION 3.16)
project(Caitsidhe_V LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Caitsidhe_V src/main.cpp)
add_executable(caitsidhe_elf src/elf_main.cpp)
target_include_directories(Caitsidhe_V PRIVATE src)
target_include_directories(caitsidhe_elf PRIVATE src)

include(CTest)
if(BUILD_TESTING)
    add_executable(caitsidhe_tests tests/test_cpu.cpp)
    target_include_directories(caitsidhe_tests PRIVATE src)
    add_test(NAME caitsidhe_smoke_tests COMMAND caitsidhe_tests)
    add_executable(caitsidhe_device_tests tests/test_devices.cpp)
    target_include_directories(caitsidhe_device_tests PRIVATE src)
    add_test(NAME caitsidhe_device_tests COMMAND caitsidhe_device_tests)
    add_executable(caitsidhe_peripheral_tests tests/test_peripherals.cpp)
    target_include_directories(caitsidhe_peripheral_tests PRIVATE src)
    add_test(NAME caitsidhe_peripheral_tests COMMAND caitsidhe_peripheral_tests)
endif()

# Optional external compiler path tests: independent assembler output through the CPU.
find_program(CAITSIDHE_CLANG NAMES clang)
find_program(CAITSIDHE_OBJCOPY NAMES llvm-objcopy)
if(BUILD_TESTING AND CAITSIDHE_CLANG AND CAITSIDHE_OBJCOPY)
    set(RV32_ELF "${CMAKE_CURRENT_BINARY_DIR}/rv32i_program.elf")
    set(RV32_BIN "${CMAKE_CURRENT_BINARY_DIR}/rv32i_program.bin")
    add_custom_command(OUTPUT "${RV32_BIN}"
        COMMAND "${CMAKE_COMMAND}"
            "-DCLANG=${CAITSIDHE_CLANG}" "-DOBJCOPY=${CAITSIDHE_OBJCOPY}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/rv32i_program.S"
            "-DELF=${RV32_ELF}" "-DBIN=${RV32_BIN}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/build_rv32i.cmake"
        DEPENDS tests/rv32i_program.S cmake/build_rv32i.cmake
        VERBATIM)
    add_custom_target(rv32i_binary ALL DEPENDS "${RV32_BIN}")
    add_test(NAME caitsidhe_rv32i_binary
        COMMAND $<TARGET_FILE:Caitsidhe_V> "${RV32_BIN}" 200)
endif()

# Self-checking ELF regression: separate from official upstream ACT4/riscv-tests.
if(BUILD_TESTING AND CAITSIDHE_CLANG)
    foreach(case IN ITEMS tohost tohost_fail signature)
        set(target_file "${CMAKE_CURRENT_BINARY_DIR}/${case}.elf")
        add_custom_command(OUTPUT "${target_file}"
            COMMAND "${CAITSIDHE_CLANG}" --target=riscv32 -march=rv32i -mabi=ilp32
                -nostdlib -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/tests/isa/link.ld -Wl,--no-relax -Wl,-e,_start
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/isa/${case}.S" -o "${target_file}"
            DEPENDS "tests/isa/${case}.S" "tests/isa/link.ld"
            VERBATIM)
        add_custom_target(${case}_elf ALL DEPENDS "${target_file}")
    endforeach()
    add_test(NAME caitsidhe_tohost_pass COMMAND $<TARGET_FILE:caitsidhe_elf> "${CMAKE_CURRENT_BINARY_DIR}/tohost.elf" 200)
    add_test(NAME caitsidhe_signature_match COMMAND $<TARGET_FILE:caitsidhe_elf>
        "${CMAKE_CURRENT_BINARY_DIR}/signature.elf" 200
        --reference "${CMAKE_CURRENT_SOURCE_DIR}/tests/isa/signature.ref")
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME caitsidhe_tohost_fail_expected COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_expected_failure.py"
            $<TARGET_FILE:caitsidhe_elf> "${CMAKE_CURRENT_BINARY_DIR}/tohost_fail.elf" 200)
    endif()
endif()
