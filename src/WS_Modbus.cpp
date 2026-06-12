#include "WS_Modbus.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"

// ─── Construction ────────────────────────────────────────────────────────────

ModbusRTU::ModbusRTU(uart_port_t port)
    : _port(port), _interFrameMs(4) {}

// ─── Initialisation ──────────────────────────────────────────────────────────

bool ModbusRTU::begin(int txPin, int rxPin, int rtsPin, uint32_t baudRate) {
    // Modbus spec: inter-frame gap = 3.5 character times.
    // 1 char = 10 bits (8N1).  Add 1 ms margin.
    _interFrameMs = (uint32_t)((3.5 * 10.0 / (double)baudRate) * 1000.0) + 1;
    if (_interFrameMs < 2) _interFrameMs = 2;

    uart_config_t cfg = {};
    cfg.baud_rate = (int)baudRate;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity    = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;

    if (uart_param_config(_port, &cfg)                         != ESP_OK) return false;
    if (uart_set_pin(_port, txPin, rxPin, rtsPin, UART_PIN_NO_CHANGE) != ESP_OK) return false;
    if (uart_driver_install(_port, 256, 0, 0, NULL, 0)         != ESP_OK) return false;
    if (uart_set_mode(_port, UART_MODE_RS485_HALF_DUPLEX)      != ESP_OK) return false;

    return true;
}

// ─── Public API ──────────────────────────────────────────────────────────────

bool ModbusRTU::readInputRegisters(uint8_t deviceAddr, uint16_t regAddr,
                                    uint16_t count, uint16_t* values) {
    if (count == 0 || count > 125 || values == nullptr) return false;

    // Build request frame
    uint8_t req[8] = {
        deviceAddr,
        0x04,
        (uint8_t)(regAddr >> 8), (uint8_t)(regAddr & 0xFF),
        (uint8_t)(count  >> 8),  (uint8_t)(count  & 0xFF),
        0x00, 0x00
    };
    uint16_t crc = calcCRC(req, 6);
    req[6] = (uint8_t)(crc & 0xFF);
    req[7] = (uint8_t)(crc >> 8);

    // Expected response: addr + func + byte_count + data + CRC
    uint8_t  resp[256];
    uint16_t expectedLen = 3 + count * 2 + 2;
    int received = sendReceive(req, sizeof(req), resp, expectedLen, 500);

    if (received < 5)                          return false;
    if (resp[0] != deviceAddr)                 return false;
    if (resp[1] == 0x84)                       return false;  // exception response
    if (resp[1] != 0x04)                       return false;
    if (resp[2] != (uint8_t)(count * 2))       return false;

    // Validate CRC
    uint16_t calcCrc = calcCRC(resp, received - 2);
    uint16_t recvCrc = (uint16_t)(resp[received - 1] << 8) | resp[received - 2];
    if (calcCrc != recvCrc)                    return false;

    // Unpack big-endian register words
    for (uint16_t i = 0; i < count; i++) {
        values[i] = (uint16_t)(resp[3 + i * 2] << 8) | resp[3 + i * 2 + 1];
    }
    return true;
}

// ─── Private helpers ─────────────────────────────────────────────────────────

int ModbusRTU::sendReceive(const uint8_t* txBuf, size_t txLen,
                            uint8_t* rxBuf, size_t rxMaxLen, uint32_t timeoutMs) {
    uart_flush_input(_port);
    uart_write_bytes(_port, (const void*)txBuf, txLen);
    uart_wait_tx_done(_port, pdMS_TO_TICKS(100));   // wait for RS485 TX to finish

    // Brief inter-frame gap so the slave can switch to TX
    vTaskDelay(pdMS_TO_TICKS(_interFrameMs));

    return uart_read_bytes(_port, rxBuf, (uint32_t)rxMaxLen, pdMS_TO_TICKS(timeoutMs));
}

uint16_t ModbusRTU::calcCRC(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x0001) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
        }
    }
    return crc;
}
