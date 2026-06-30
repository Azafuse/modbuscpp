#include "modbuscpp/convert.hpp"
#include <cstring>

namespace modbus {

static uint32_t regsToU32Internal(uint16_t reg0, uint16_t reg1,
                                  WordOrder wo, ByteOrder bo) {
  uint8_t bytes[4];
  if (bo == ByteOrder::BigEndian) {
    bytes[0] = static_cast<uint8_t>((reg0 >> 8) & 0xFF);
    bytes[1] = static_cast<uint8_t>(reg0 & 0xFF);
    bytes[2] = static_cast<uint8_t>((reg1 >> 8) & 0xFF);
    bytes[3] = static_cast<uint8_t>(reg1 & 0xFF);
  } else {
    bytes[0] = static_cast<uint8_t>(reg0 & 0xFF);
    bytes[1] = static_cast<uint8_t>((reg0 >> 8) & 0xFF);
    bytes[2] = static_cast<uint8_t>(reg1 & 0xFF);
    bytes[3] = static_cast<uint8_t>((reg1 >> 8) & 0xFF);
  }
  if (wo == WordOrder::HighWordFirst) {
    return (static_cast<uint32_t>(bytes[0]) << 24) |
           (static_cast<uint32_t>(bytes[1]) << 16) |
           (static_cast<uint32_t>(bytes[2]) << 8)  |
            static_cast<uint32_t>(bytes[3]);
  } else {
    return (static_cast<uint32_t>(bytes[2]) << 24) |
           (static_cast<uint32_t>(bytes[3]) << 16) |
           (static_cast<uint32_t>(bytes[0]) << 8)  |
            static_cast<uint32_t>(bytes[1]);
  }
}

static void u32ToRegsInternal(uint32_t val, uint16_t& reg0, uint16_t& reg1,
                              WordOrder wo, ByteOrder bo) {
  uint8_t b0 = static_cast<uint8_t>((val >> 24) & 0xFF);
  uint8_t b1 = static_cast<uint8_t>((val >> 16) & 0xFF);
  uint8_t b2 = static_cast<uint8_t>((val >> 8) & 0xFF);
  uint8_t b3 = static_cast<uint8_t>(val & 0xFF);
  if (wo == WordOrder::HighWordFirst) {
    if (bo == ByteOrder::BigEndian) {
      reg0 = static_cast<uint16_t>((b0 << 8) | b1);
      reg1 = static_cast<uint16_t>((b2 << 8) | b3);
    } else {
      reg0 = static_cast<uint16_t>((b1 << 8) | b0);
      reg1 = static_cast<uint16_t>((b3 << 8) | b2);
    }
  } else {
    if (bo == ByteOrder::BigEndian) {
      reg0 = static_cast<uint16_t>((b2 << 8) | b3);
      reg1 = static_cast<uint16_t>((b0 << 8) | b1);
    } else {
      reg0 = static_cast<uint16_t>((b3 << 8) | b2);
      reg1 = static_cast<uint16_t>((b1 << 8) | b0);
    }
  }
}

// --- 32-bit conversions ---

int32_t regsToInt32(uint16_t reg0, uint16_t reg1, WordOrder wo, ByteOrder bo) {
  uint32_t u = regsToU32Internal(reg0, reg1, wo, bo);
  int32_t result; std::memcpy(&result, &u, sizeof(result)); return result;
}

uint32_t regsToUint32(uint16_t reg0, uint16_t reg1, WordOrder wo, ByteOrder bo) {
  return regsToU32Internal(reg0, reg1, wo, bo);
}

float regsToFloat32(uint16_t reg0, uint16_t reg1, WordOrder wo, ByteOrder bo) {
  uint32_t u = regsToU32Internal(reg0, reg1, wo, bo);
  float result; std::memcpy(&result, &u, sizeof(result)); return result;
}

std::pair<uint16_t, uint16_t> int32ToRegs(int32_t value, WordOrder wo, ByteOrder bo) {
  uint32_t u; std::memcpy(&u, &value, sizeof(u)); return uint32ToRegs(u, wo, bo);
}

std::pair<uint16_t, uint16_t> uint32ToRegs(uint32_t value, WordOrder wo, ByteOrder bo) {
  uint16_t reg0, reg1;
  u32ToRegsInternal(value, reg0, reg1, wo, bo);
  return {reg0, reg1};
}

std::pair<uint16_t, uint16_t> float32ToRegs(float value, WordOrder wo, ByteOrder bo) {
  uint32_t u; std::memcpy(&u, &value, sizeof(u)); return uint32ToRegs(u, wo, bo);
}

// --- 64-bit (float64/double) conversions ---

double regsToFloat64(uint16_t reg0, uint16_t reg1,
                     uint16_t reg2, uint16_t reg3,
                     WordOrder wo, ByteOrder bo) {
  uint8_t bytes[8];
  uint16_t regs[4] = {reg0, reg1, reg2, reg3};
  for (int i = 0; i < 4; ++i) {
    if (bo == ByteOrder::BigEndian) {
      bytes[i * 2]     = static_cast<uint8_t>((regs[i] >> 8) & 0xFF);
      bytes[i * 2 + 1] = static_cast<uint8_t>(regs[i] & 0xFF);
    } else {
      bytes[i * 2]     = static_cast<uint8_t>(regs[i] & 0xFF);
      bytes[i * 2 + 1] = static_cast<uint8_t>((regs[i] >> 8) & 0xFF);
    }
  }
  uint8_t ordered[8];
  if (wo == WordOrder::HighWordFirst) {
    for (int i = 0; i < 8; ++i) ordered[i] = bytes[i];
  } else {
    for (int i = 0; i < 4; ++i) ordered[i] = bytes[4 + i];
    for (int i = 0; i < 4; ++i) ordered[4 + i] = bytes[i];
  }
  double result;
  std::memcpy(&result, ordered, sizeof(result));
  return result;
}

std::vector<uint16_t> float64ToRegs(double value, WordOrder wo, ByteOrder bo) {
  uint8_t bytes[8];
  std::memcpy(bytes, &value, sizeof(bytes));
  std::vector<uint16_t> result(4);
  if (wo == WordOrder::HighWordFirst) {
    for (int i = 0; i < 4; ++i) {
      uint8_t hi = bytes[i * 2], lo = bytes[i * 2 + 1];
      result[i] = (bo == ByteOrder::BigEndian)
        ? static_cast<uint16_t>((hi << 8) | lo)
        : static_cast<uint16_t>((lo << 8) | hi);
    }
  } else {
    for (int i = 0; i < 2; ++i) {
      uint8_t hi = bytes[4 + i * 2], lo = bytes[4 + i * 2 + 1];
      result[i] = (bo == ByteOrder::BigEndian)
        ? static_cast<uint16_t>((hi << 8) | lo)
        : static_cast<uint16_t>((lo << 8) | hi);
    }
    for (int i = 0; i < 2; ++i) {
      uint8_t hi = bytes[i * 2], lo = bytes[i * 2 + 1];
      result[2 + i] = (bo == ByteOrder::BigEndian)
        ? static_cast<uint16_t>((hi << 8) | lo)
        : static_cast<uint16_t>((lo << 8) | hi);
    }
  }
  return result;
}

// --- String conversions ---

std::string regsToString(const std::vector<uint16_t>& regs) {
  std::string result;
  result.reserve(regs.size() * 2);
  for (uint16_t r : regs) {
    result.push_back(static_cast<char>((r >> 8) & 0xFF));
    result.push_back(static_cast<char>(r & 0xFF));
  }
  return result;
}

std::vector<uint16_t> stringToRegs(const std::string& str) {
  std::vector<uint16_t> result;
  result.reserve((str.size() + 1) / 2);
  for (size_t i = 0; i < str.size(); i += 2) {
    uint16_t r = static_cast<uint16_t>(static_cast<uint8_t>(str[i])) << 8;
    if (i + 1 < str.size())
      r |= static_cast<uint8_t>(str[i + 1]);
    result.push_back(r);
  }
  return result;
}

} // namespace modbus