#include "WS_SDM230.h"
#include <string.h>

// ─── SDM230 input register map (base-0 Modbus addresses) ─────────────────────
// Each measurement is a 32-bit IEEE-754 float stored in 2 consecutive registers.
// Reference: Eastron SDM230 Modbus Protocol document.

static constexpr uint16_t REG_VOLTAGE        = 0x0000;
static constexpr uint16_t REG_CURRENT        = 0x0006;
static constexpr uint16_t REG_ACTIVE_POWER   = 0x000C;
static constexpr uint16_t REG_APPARENT_POWER = 0x0012;
static constexpr uint16_t REG_REACTIVE_POWER = 0x0018;
static constexpr uint16_t REG_POWER_FACTOR   = 0x001E;
// Batch 2: contiguous block starting at 0x0046
static constexpr uint16_t REG_FREQUENCY      = 0x0046;
static constexpr uint16_t REG_IMPORT_ENERGY  = 0x0048;
static constexpr uint16_t REG_EXPORT_ENERGY  = 0x004A;
// Batch 3: total energy
static constexpr uint16_t REG_TOTAL_ENERGY   = 0x0156;

// ─── Construction ────────────────────────────────────────────────────────────

SDM230::SDM230(uart_port_t port, int txPin, int rxPin, int rtsPin,
               uint8_t addr, uint32_t baud)
    : _bus(port), _txPin(txPin), _rxPin(rxPin), _rtsPin(rtsPin),
      _addr(addr), _baud(baud) {}

bool SDM230::begin() {
    return _bus.begin(_txPin, _rxPin, _rtsPin, _baud);
}

// ─── Read all measurements ───────────────────────────────────────────────────

bool SDM230::read(SDM230Data& out) {
    out.valid = false;

    uint16_t regs[32];

    // Batch 1: voltage … power factor  (registers 0x0000 – 0x001F, 32 words)
    if (!_bus.readInputRegisters(_addr, REG_VOLTAGE, 32, regs)) return false;
    out.voltage        = toFloat(regs + (REG_VOLTAGE        - 0x0000));
    out.current        = toFloat(regs + (REG_CURRENT        - 0x0000));
    out.activePower    = toFloat(regs + (REG_ACTIVE_POWER   - 0x0000));
    out.apparentPower  = toFloat(regs + (REG_APPARENT_POWER - 0x0000));
    out.reactivePower  = toFloat(regs + (REG_REACTIVE_POWER - 0x0000));
    out.powerFactor    = toFloat(regs + (REG_POWER_FACTOR   - 0x0000));

    // Batch 2: frequency + energy counters  (registers 0x0046 – 0x004B, 6 words)
    if (!_bus.readInputRegisters(_addr, REG_FREQUENCY, 6, regs)) return false;
    out.frequency     = toFloat(regs + (REG_FREQUENCY     - 0x0046));
    out.importEnergy  = toFloat(regs + (REG_IMPORT_ENERGY - 0x0046));
    out.exportEnergy  = toFloat(regs + (REG_EXPORT_ENERGY - 0x0046));

    // Batch 3: total active energy  (2 words)
    if (!_bus.readInputRegisters(_addr, REG_TOTAL_ENERGY, 2, regs)) return false;
    out.totalEnergy = toFloat(regs);

    out.valid = true;
    return true;
}

// ─── Private helpers ─────────────────────────────────────────────────────────

float SDM230::toFloat(const uint16_t* regs) {
    // SDM230 uses big-endian word order: regs[0] = high word, regs[1] = low word
    uint32_t raw = ((uint32_t)regs[0] << 16) | regs[1];
    float value;
    memcpy(&value, &raw, sizeof(value));
    return value;
}
