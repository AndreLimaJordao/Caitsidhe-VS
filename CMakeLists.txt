cmake_minimum_required(VERSION 3.16)
project(Caitsidhe_V LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Caitsidhe_V src/main.cpp)
add_executable(caitsidhe_elf src/elf_main.cpp)
add_executable(matatabi_mocker src/matatabi_main.cpp)
target_include_directories(Caitsidhe_V PRIVATE src)
target_include_directories(caitsidhe_elf PRIVATE src)

include(CTest)
option(CAITSIDHE_REQUIRE_REFERENCE "Require Unicorn differential tests" OFF)
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
    add_executable(caitsidhe_wishbone_tests tests/test_wishbone.cpp)
    add_test(NAME caitsidhe_wishbone_tests COMMAND caitsidhe_wishbone_tests)
    foreach(device IN ITEMS crc32 timer)
        add_test(NAME matatabi_${device} COMMAND matatabi_mocker ${device}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/matatabi_${device}.txt")
    endforeach()
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

function(add_rv32i_elf name source arch)
    set(target_file "${CMAKE_CURRENT_BINARY_DIR}/${name}.elf")
    set(linker_script "${CMAKE_CURRENT_SOURCE_DIR}/tests/isa/link.ld")
    add_custom_command(OUTPUT "${target_file}"
        COMMAND "${CAITSIDHE_CLANG}" --target=riscv32 "-march=${arch}" -mabi=ilp32
            -nostdlib -Wl,-T,${linker_script} -Wl,--no-relax -Wl,-e,_start
            "${CMAKE_CURRENT_SOURCE_DIR}/${source}" -o "${target_file}"
        DEPENDS "${source}" "${linker_script}"
        VERBATIM)
    add_custom_target(${name}_elf ${ARGN} DEPENDS "${target_file}")
endfunction()

# The demo uses the same board in both command-line runners.
if(CAITSIDHE_CLANG)
    if(BUILD_TESTING)
        set(demo_build ALL)
    endif()
    add_rv32i_elf(uart_timer examples/uart_timer.S rv32i ${demo_build})
    add_rv32i_elf(timer_irq examples/timer_irq.S rv32i_zicsr ${demo_build})
    add_rv32i_elf(crc32 examples/crc32.S rv32i ${demo_build})
    if(CAITSIDHE_OBJCOPY)
        set(UART_TIMER_ELF "${CMAKE_CURRENT_BINARY_DIR}/uart_timer.elf")
        set(UART_TIMER_BIN "${CMAKE_CURRENT_BINARY_DIR}/uart_timer.bin")
        add_custom_command(OUTPUT "${UART_TIMER_BIN}"
            COMMAND "${CAITSIDHE_OBJCOPY}" -O binary "${UART_TIMER_ELF}" "${UART_TIMER_BIN}"
            DEPENDS "${UART_TIMER_ELF}"
            VERBATIM)
        add_custom_target(uart_timer_bin ${demo_build} DEPENDS "${UART_TIMER_BIN}")
        add_dependencies(uart_timer_bin uart_timer_elf)
    endif()
endif()

# Self-checking ELF regression: separate from official upstream ACT4/riscv-tests.
if(BUILD_TESTING AND CAITSIDHE_CLANG)
    foreach(case IN ITEMS tohost tohost_fail signature)
        add_rv32i_elf(${case} tests/isa/${case}.S rv32i ALL)
    endforeach()
    add_test(NAME caitsidhe_crc32_firmware COMMAND caitsidhe_elf "${CMAKE_CURRENT_BINARY_DIR}/crc32.elf" 200)
    foreach(wait IN ITEMS 0 5)
        add_test(NAME caitsidhe_timer_irq_${wait} COMMAND "${CMAKE_COMMAND}"
            "-DRUNNER=$<TARGET_FILE:caitsidhe_elf>" "-DIMAGE=${CMAKE_CURRENT_BINARY_DIR}/timer_irq.elf"
            "-DWAIT=${wait}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_timer_irq.cmake")
    endforeach()
    add_test(NAME caitsidhe_uart_timer_elf COMMAND "${CMAKE_COMMAND}"
        "-DRUNNER=$<TARGET_FILE:caitsidhe_elf>" "-DIMAGE=${CMAKE_CURRENT_BINARY_DIR}/uart_timer.elf"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_uart_timer.cmake")
    if(CAITSIDHE_OBJCOPY)
        add_test(NAME caitsidhe_uart_timer_binary COMMAND "${CMAKE_COMMAND}"
            "-DRUNNER=$<TARGET_FILE:Caitsidhe_V>" "-DIMAGE=${UART_TIMER_BIN}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_uart_timer.cmake")
    endif()
    add_test(NAME caitsidhe_tohost_pass COMMAND $<TARGET_FILE:caitsidhe_elf> "${CMAKE_CURRENT_BINARY_DIR}/tohost.elf" 200)
    add_test(NAME caitsidhe_signature_match COMMAND $<TARGET_FILE:caitsidhe_elf>
        "${CMAKE_CURRENT_BINARY_DIR}/signature.elf" 200
        --reference "${CMAKE_CURRENT_SOURCE_DIR}/tests/isa/signature.ref")
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME caitsidhe_tohost_fail_expected COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_expected_failure.py"
            $<TARGET_FILE:caitsidhe_elf> "${CMAKE_CURRENT_BINARY_DIR}/tohost_fail.elf" 200)
        execute_process(COMMAND "${Python3_EXECUTABLE}" -c
            "import unicorn as u; u.Uc(u.UC_ARCH_RISCV, u.UC_MODE_RISCV32)"
            RESULT_VARIABLE reference_status OUTPUT_QUIET ERROR_QUIET)
        if(reference_status STREQUAL "0")
            set(CAITSIDHE_REFERENCE_READY TRUE)
            add_rv32i_elf(differential tests/isa/differential.S rv32i ALL)
            foreach(wait IN ITEMS 0 5)
                add_test(NAME caitsidhe_reference_${wait} COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_reference.py"
                    --sim $<TARGET_FILE:caitsidhe_elf> --elf "${CMAKE_CURRENT_BINARY_DIR}/differential.elf"
                    --wait-states ${wait})
                set_tests_properties(caitsidhe_reference_${wait} PROPERTIES TIMEOUT 90)
            endforeach()
        endif()
    endif()
endif()
if(CAITSIDHE_REQUIRE_REFERENCE AND NOT CAITSIDHE_REFERENCE_READY)
    message(FATAL_ERROR "Reference tests need BUILD_TESTING, Clang/LLD, Python, and tools/reference-requirements.txt")
endif()
