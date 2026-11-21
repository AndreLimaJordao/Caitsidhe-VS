#pragma once
#include "soc.h"
#include "../peripherals/crc32.h"
#include "../peripherals/gpio.h"
#include "../peripherals/timer.h"
#include "../peripherals/uart.h"
#include <string>

class Board {
    UART uart_;
    GPIO gpio_;
    Timer timer_;
    CRC32 crc_;
    SoC soc_;
public:
    explicit Board(CPU::Options options = {}) : soc_(cvsim::MEM_SIZE, options) {
        soc_.attach(0x90000000, 8, uart_);
        soc_.attach(0x90001000, 12, gpio_);
        soc_.attach(0x90002000, 12, timer_);
        soc_.attach(0x90003000, 12, crc_);
        soc_.set_timer_interrupt_source(timer_);
    }

    SoC& soc() { return soc_; }
    const std::string& uart_output() const { return uart_.output(); }
};
