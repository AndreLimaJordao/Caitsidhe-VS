#include "../src/platform/soc.h"
#include "../src/peripherals/uart.h"
#include "../src/peripherals/gpio.h"
#include "../src/peripherals/timer.h"
#include <cassert>
#include <iostream>
int main() {
    SoC soc;
    UART uart;
    GPIO gpio;
    Timer timer;
    soc.attach(0x90000000,8,uart);
    soc.attach(0x90001000,12,gpio);
    soc.attach(0x90002000,12,timer);
    assert(soc.bus().read(0x90000004)==1);
    soc.bus().write(0x90000000, 'C', 1);
    soc.bus().write(0x90000000, 'V', 1);
    assert(uart.output()=="CV");
    soc.bus().write(0x90001000,0xA5,15);
    soc.bus().write(0x90001004,0x0f,15);
    gpio.set_inputs(0x55);
    assert(gpio.outputs()==5);
    assert(soc.bus().read(0x90001008)==0x55);
    soc.bus().write(0x90001008,0,15);
    assert(soc.bus().read(0x90001008)==0x55);
    soc.bus().write(0x90002004,3,15);
    soc.bus().write(0x90002008,1,15);
    timer.tick(); timer.tick();
    assert(!timer.elapsed());
    timer.tick();
    assert(timer.elapsed());
    assert(soc.bus().read(0x90002000)==3);
    soc.bus().write(0x90002008,3,15);
    assert(soc.bus().read(0x90002000)==0);
    assert(!timer.elapsed());
    timer.tick();
    assert(soc.bus().read(0x90002000)==1);
    std::cout << "UART/GPIO/Timer tests passed\n";
}
