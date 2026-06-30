#ifndef MODBUS_CRC_HPP
#define MODBUS_CRC_HPP

#include <cstdint>

namespace modbus {

/// Compute Modbus CRC-16 over the given data range.
/// Polynomial: 0xA001 (reflected 0x8005), init: 0xFFFF, no final XOR.
/// Uses a precomputed 256-entry lookup table.
/// @param data  pointer to start of data
/// @param len   number of bytes
/// @return 16-bit CRC (host byte order; append low byte first on the wire)
uint16_t crc16(const uint8_t* data, size_t len);

} // namespace modbus

#endif // MODBUS_CRC_HPP
