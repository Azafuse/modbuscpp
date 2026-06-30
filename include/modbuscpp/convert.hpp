#ifndef MODBUS_CONVERT_HPP
#define MODBUS_CONVERT_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace modbus {

/// Word order for multi-register values
enum class WordOrder {
  HighWordFirst,  // most significant word at lowest register address
  LowWordFirst    // least significant word at lowest register address
};

/// Byte order within each register word
enum class ByteOrder {
  BigEndian,    // high byte first (Modbus standard)
  LittleEndian  // low byte first
};

/// Convert 2 registers (4 bytes) to int32_t
int32_t  regsToInt32(uint16_t reg0, uint16_t reg1,
                     WordOrder wo = WordOrder::HighWordFirst,
                     ByteOrder bo = ByteOrder::BigEndian);

/// Convert 2 registers (4 bytes) to uint32_t
uint32_t regsToUint32(uint16_t reg0, uint16_t reg1,
                      WordOrder wo = WordOrder::HighWordFirst,
                      ByteOrder bo = ByteOrder::BigEndian);

/// Convert 2 registers (4 bytes) to float (IEEE 754 single)
float    regsToFloat32(uint16_t reg0, uint16_t reg1,
                       WordOrder wo = WordOrder::HighWordFirst,
                       ByteOrder bo = ByteOrder::BigEndian);

/// Convert 4 registers (8 bytes) to double (IEEE 754 double)
double   regsToFloat64(uint16_t reg0, uint16_t reg1,
                       uint16_t reg2, uint16_t reg3,
                       WordOrder wo = WordOrder::HighWordFirst,
                       ByteOrder bo = ByteOrder::BigEndian);

/// Convert int32_t to 2 registers
std::pair<uint16_t, uint16_t> int32ToRegs(int32_t value,
                                          WordOrder wo = WordOrder::HighWordFirst,
                                          ByteOrder bo = ByteOrder::BigEndian);

/// Convert uint32_t to 2 registers
std::pair<uint16_t, uint16_t> uint32ToRegs(uint32_t value,
                                           WordOrder wo = WordOrder::HighWordFirst,
                                           ByteOrder bo = ByteOrder::BigEndian);

/// Convert float to 2 registers
std::pair<uint16_t, uint16_t> float32ToRegs(float value,
                                            WordOrder wo = WordOrder::HighWordFirst,
                                            ByteOrder bo = ByteOrder::BigEndian);

/// Convert double to 4 registers (returned as vector of 4 uint16_t)
std::vector<uint16_t> float64ToRegs(double value,
                                    WordOrder wo = WordOrder::HighWordFirst,
                                    ByteOrder bo = ByteOrder::BigEndian);

/// Pack registers into a string (2 chars per register, big-endian char order)
std::string regsToString(const std::vector<uint16_t>& regs);

/// Unpack string into registers (big-endian char order)
std::vector<uint16_t> stringToRegs(const std::string& str);

} // namespace modbus

#endif // MODBUS_CONVERT_HPP
