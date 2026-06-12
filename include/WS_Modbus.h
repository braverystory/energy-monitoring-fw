#pragma once

#include <stdint.h>
#include <stddef.h>
#include "driver/uart.h"

// Minimal Modbus RTU master over ESP-IDF UART driver (no Arduino dependencies)
class ModbusRTU {
public:
    explicit ModbusRTU(uart_port_t port);

    // Configure and start the UART in RS485 half-duplex mode
    bool begin(int txPin, int rxPin, int rtsPin, uint32_t baudRate);

    // Function Code 0x04 – Read Input Registers (used by SDM230 and most meters)
    bool readInputRegisters(uint8_t deviceAddr, uint16_t regAddr,
                            uint16_t count, uint16_t* values);

private:
    uart_port_t _port;
    uint32_t    _interFrameMs;  // Modbus 3.5-character silent gap

    // Send a request frame and collect the response; returns bytes received
    int sendReceive(const uint8_t* txBuf, size_t txLen,
                    uint8_t* rxBuf, size_t rxMaxLen, uint32_t timeoutMs);

    static uint16_t calcCRC(const uint8_t* data, size_t len);
};
