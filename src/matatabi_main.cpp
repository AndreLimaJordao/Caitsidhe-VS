#include "memory/bus.h"
#include "peripherals/crc32.h"
#include "peripherals/gpio.h"
#include "peripherals/timer.h"
#include "peripherals/uart.h"
#include <charconv>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>

static uint32_t number(std::istream& input) {
    std::string token;
    input >> token;
    std::string_view digits = token;
    const bool hex = digits.starts_with("0x") || digits.starts_with("0X");
    if (hex) digits.remove_prefix(2);
    const auto end = digits.data() + digits.size();
    uint32_t result = 0;
    const auto parsed = std::from_chars(digits.data(), end, result, hex ? 16 : 10);
    if (parsed.ec != std::errc{} || parsed.ptr != end)
        throw std::runtime_error("Expected a 32-bit number");
    return result;
}

static void run(std::istream& script, Peripheral& device, uint32_t size) {
    constexpr cvsim::addr_t base = 0x90000000;
    RAM ram(0);
    Bus bus(ram);
    bus.attach(base, size, device);
    std::string line;
    unsigned line_number = 0;
    while (std::getline(script, line)) {
        ++line_number;
        std::istringstream input(line.substr(0, line.find('#')));
        std::string command;
        if (!(input >> command)) continue;
        try {
            const auto first = number(input);
            if (command == "tick") {
                std::string extra;
                if (input >> extra) throw std::runtime_error("Unexpected argument: " + extra);
                for (uint32_t i = 0; i < first; ++i) device.tick();
                continue;
            }
            if (first >= size || (first & 3u)) throw std::runtime_error("Invalid register offset");
            uint32_t value = 0, lanes = 0;
            if (command == "write" || command == "expect") value = number(input);
            if (command == "write") {
                lanes = number(input);
                if (lanes > 15) throw std::runtime_error("Byte mask must be between 0 and 15");
            }
            std::string extra;
            if (input >> extra) throw std::runtime_error("Unexpected argument: " + extra);
            if (command == "write") bus.write(base + first, value, uint8_t(lanes));
            else if (command == "read") std::cout << "0x" << std::hex << bus.read(base + first) << '\n';
            else if (command == "expect") {
                const auto actual = bus.read(base + first);
                if (actual != value) {
                    std::ostringstream message;
                    message << std::hex << "Expected 0x" << value << ", got 0x" << actual;
                    throw std::runtime_error(message.str());
                }
            } else throw std::runtime_error("Unknown command: " + command);
        } catch (const std::exception& error) {
            throw std::runtime_error("Line " + std::to_string(line_number) + ": " + error.what());
        }
    }
    if (script.bad()) throw std::runtime_error("Error reading script");
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: matatabi_mocker <crc32|timer|gpio|uart> <script>\n";
        return 2;
    }
    try {
        std::ifstream script(argv[2]);
        if (!script) throw std::runtime_error("Cannot open script");
        const std::string_view name = argv[1];
        if (name == "crc32") { CRC32 crc; run(script, crc, 12); }
        else if (name == "timer") { Timer timer; run(script, timer, 12); }
        else if (name == "gpio") { GPIO gpio; run(script, gpio, 12); }
        else if (name == "uart") { UART uart; run(script, uart, 8); std::cout << uart.output(); }
        else throw std::runtime_error("Unknown peripheral");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[Matatabi] " << error.what() << '\n';
        return 1;
    }
}
