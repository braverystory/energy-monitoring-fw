#pragma once

#include <stdint.h>
#include "WS_Modbus.h"

// Measurement snapshot from the SDM230 meter
struct SDM230Data {
    float voltage;        // Volts
    float current;        // Amps
    float activePower;    // Watts
    float apparentPower;  // VA
    float reactivePower;  // VAr
    float powerFactor;
    float frequency;      // Hz
    float importEnergy;   // kWh
    float exportEnergy;   // kWh
    float totalEnergy;    // kWh
    bool  valid;          // false when a Modbus read failed
};

// SDM230 Modbus RTU reader (single-phase energy meter by Eastron)
// All registers are FC04 Input Registers, 32-bit IEEE-754 float, big-endian word order.
class SDM230 {
public:
    // port    – ESP-IDF UART port (e.g. UART_NUM_1)
    // txPin   – UART TX GPIO
    // rxPin   – UART RX GPIO
    // rtsPin  – RS485 TX-enable GPIO (DE/RE driver pin)
    // addr    – Modbus device address (default 1)
    // baud    – baud rate (default 9600)
    SDM230(uart_port_t port, int txPin, int rxPin, int rtsPin,
           uint8_t addr = 1, uint32_t baud = 9600);

    // Initialise the UART; call once from setup()
    bool begin();

    // Read all measurements into `out`; returns false on any Modbus error
    bool read(SDM230Data& out);

private:
    ModbusRTU _bus;
    int       _txPin, _rxPin, _rtsPin;
    uint8_t   _addr;
    uint32_t  _baud;

    // Combine two big-endian Modbus registers into an IEEE-754 float
    static float toFloat(const uint16_t* regs);
};
