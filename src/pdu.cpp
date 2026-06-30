#include "modbuscpp/pdu.hpp"
#include <algorithm>

namespace modbus {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/// Write a big-endian uint16_t to a byte buffer at the given offset
static void writeU16(std::vector<uint8_t>& buf, uint16_t val) {
  buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
  buf.push_back(static_cast<uint8_t>(val & 0xFF));
}

/// Read a big-endian uint16_t from a byte buffer at the given offset
static uint16_t readU16(const uint8_t* data, size_t offset) {
  return static_cast<uint16_t>(
    (static_cast<uint16_t>(data[offset]) << 8) |
    static_cast<uint16_t>(data[offset + 1])
  );
}

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

Error validateCoilQuantity(uint16_t quantity) {
  if (quantity < 1 || quantity > 2000) {
    return Error{ErrorCategory::Usage, 0,
      "Coil quantity must be 1..2000, got " + std::to_string(quantity)};
  }
  return ok();
}

Error validateRegisterQuantity(uint16_t quantity) {
  if (quantity < 1 || quantity > 125) {
    return Error{ErrorCategory::Usage, 0,
      "Register quantity must be 1..125, got " + std::to_string(quantity)};
  }
  return ok();
}

Error validateWriteMultipleCoilQuantity(uint16_t quantity) {
  if (quantity < 1 || quantity > 1968) {
    return Error{ErrorCategory::Usage, 0,
      "Write multiple coils quantity must be 1..1968, got " + std::to_string(quantity)};
  }
  return ok();
}

Error validateWriteMultipleRegisterQuantity(uint16_t quantity) {
  if (quantity < 1 || quantity > 123) {
    return Error{ErrorCategory::Usage, 0,
      "Write multiple registers quantity must be 1..123, got " + std::to_string(quantity)};
  }
  return ok();
}

// ---------------------------------------------------------------------------
// Build helpers
// ---------------------------------------------------------------------------

Result<Pdu> buildReadCoils(uint16_t startAddress, uint16_t quantity) {
  if (auto err = validateCoilQuantity(quantity); !err.ok()) return err;
  Pdu pdu;
  pdu.functionCode = static_cast<uint8_t>(FunctionCode::ReadCoils);
  writeU16(pdu.data, startAddress);
  writeU16(pdu.data, quantity);
  return pdu;
}

Result<Pdu> buildReadDiscreteInputs(uint16_t startAddress, uint16_t quantity) {
  if (auto err = validateCoilQuantity(quantity); !err.ok()) return err;
  Pdu pdu;
  pdu.functionCode = static_cast<uint8_t>(FunctionCode::ReadDiscreteInputs);
  writeU16(pdu.data, startAddress);
  writeU16(pdu.data, quantity);
  return pdu;
}

Result<Pdu> buildReadHoldingRegisters(uint16_t startAddress, uint16_t quantity) {
  if (auto err = validateRegisterQuantity(quantity); !err.ok()) return err;
  Pdu pdu;
  pdu.functionCode = static_cast<uint8_t>(FunctionCode::ReadHoldingRegisters);
  writeU16(pdu.data, startAddress);
  writeU16(pdu.data, quantity);
  return pdu;
}

Result<Pdu> buildReadInputRegisters(uint16_t startAddress, uint16_t quantity) {
  if (auto err = validateRegisterQuantity(quantity); !err.ok()) return err;
  Pdu pdu;
  pdu.functionCode = static_cast<uint8_t>(FunctionCode::ReadInputRegisters);
  writeU16(pdu.data, startAddress);
  writeU16(pdu.data, quantity);
  return pdu;
}

Result<Pdu> buildWriteSingleCoil(uint16_t address, bool on) {
  Pdu pdu;
  pdu.functionCode = static_cast<uint8_t>(FunctionCode::WriteSingleCoil);
  writeU16(pdu.data, address);
  writeU16(pdu.data, on ? 0xFF00 : 0x0000);
  return pdu;
}

Result<Pdu> buildWriteSingleRegister(uint16_t address, uint16_t value) {
  Pdu pdu;
  pdu.functionCode = static_cast<uint8_t>(FunctionCode::WriteSingleRegister);
  writeU16(pdu.data, address);
  writeU16(pdu.data, value);
  return pdu;
}

Result<Pdu> buildWriteMultipleCoils(uint16_t startAddress, const std::vector<bool>& values) {
  if (auto err = validateWriteMultipleCoilQuantity(static_cast<uint16_t>(values.size())); !err.ok())
    return err;
  Pdu pdu;
  pdu.functionCode = static_cast<uint8_t>(FunctionCode::WriteMultipleCoils);
  writeU16(pdu.data, startAddress);
  writeU16(pdu.data, static_cast<uint16_t>(values.size()));
  size_t byteCount = (values.size() + 7) / 8;
  pdu.data.push_back(static_cast<uint8_t>(byteCount));
  for (size_t i = 0; i < byteCount; ++i) {
    uint8_t byteVal = 0;
    for (size_t bit = 0; bit < 8; ++bit) {
      size_t idx = i * 8 + bit;
      if (idx < values.size() && values[idx])
        byteVal |= static_cast<uint8_t>(1 << bit);
    }
    pdu.data.push_back(byteVal);
  }
  return pdu;
}

Result<Pdu> buildWriteMultipleRegisters(uint16_t startAddress, const std::vector<uint16_t>& values) {
  if (auto err = validateWriteMultipleRegisterQuantity(static_cast<uint16_t>(values.size())); !err.ok())
    return err;
  Pdu pdu;
  pdu.functionCode = static_cast<uint8_t>(FunctionCode::WriteMultipleRegisters);
  writeU16(pdu.data, startAddress);
  writeU16(pdu.data, static_cast<uint16_t>(values.size()));
  uint8_t byteCount = static_cast<uint8_t>(values.size() * 2);
  pdu.data.push_back(byteCount);
  for (uint16_t v : values)
    writeU16(pdu.data, v);
  return pdu;
}

// ---------------------------------------------------------------------------
// Parse helpers
// ---------------------------------------------------------------------------

Result<std::vector<bool>> parseReadBitsResponse(const Pdu& /*req*/, const Pdu& resp) {
  if (resp.data.size() < 1) {
    return Error{ErrorCategory::Protocol, 0, "Response too short for read bits"};
  }
  uint8_t byteCount = resp.data[0];
  if (resp.data.size() < static_cast<size_t>(1 + byteCount)) {
    return Error{ErrorCategory::Protocol, 0, "Response byte count mismatch"};
  }
  std::vector<bool> result;
  for (uint8_t i = 0; i < byteCount; ++i) {
    uint8_t byteVal = resp.data[1 + i];
    for (int bit = 0; bit < 8; ++bit)
      result.push_back((byteVal & (1 << bit)) != 0);
  }
  return result;
}

Result<std::vector<uint16_t>> parseReadRegistersResponse(const Pdu& /*req*/, const Pdu& resp) {
  if (resp.data.size() < 1) {
    return Error{ErrorCategory::Protocol, 0, "Response too short for read registers"};
  }
  uint8_t byteCount = resp.data[0];
  if (byteCount % 2 != 0) {
    return Error{ErrorCategory::Protocol, 0, "Response byte count must be even for registers"};
  }
  if (resp.data.size() < static_cast<size_t>(1 + byteCount)) {
    return Error{ErrorCategory::Protocol, 0, "Response byte count mismatch"};
  }
  uint16_t regCount = byteCount / 2;
  std::vector<uint16_t> result;
  result.reserve(regCount);
  for (uint16_t i = 0; i < regCount; ++i)
    result.push_back(readU16(resp.data.data(), 1 + i * 2));
  return result;
}

Result<std::pair<uint16_t, uint16_t>> parseWriteSingleResponse(const Pdu& /*req*/, const Pdu& resp) {
  if (resp.data.size() < 4) {
    return Error{ErrorCategory::Protocol, 0, "Write single response too short"};
  }
  uint16_t addr  = readU16(resp.data.data(), 0);
  uint16_t value = readU16(resp.data.data(), 2);
  return std::make_pair(addr, value);
}

Result<std::pair<uint16_t, uint16_t>> parseWriteMultipleCoilsResponse(const Pdu& /*req*/, const Pdu& resp) {
  if (resp.data.size() < 4) {
    return Error{ErrorCategory::Protocol, 0, "Write multiple coils response too short"};
  }
  uint16_t start = readU16(resp.data.data(), 0);
  uint16_t qty   = readU16(resp.data.data(), 2);
  return std::make_pair(start, qty);
}

Result<std::pair<uint16_t, uint16_t>> parseWriteMultipleRegistersResponse(const Pdu& /*req*/, const Pdu& resp) {
  if (resp.data.size() < 4) {
    return Error{ErrorCategory::Protocol, 0, "Write multiple registers response too short"};
  }
  uint16_t start = readU16(resp.data.data(), 0);
  uint16_t qty   = readU16(resp.data.data(), 2);
  return std::make_pair(start, qty);
}

// ---------------------------------------------------------------------------
// Exception handling
// ---------------------------------------------------------------------------

bool isExceptionResponse(const Pdu& pdu) {
  return (pdu.functionCode & 0x80) != 0;
}

Error parseException(const Pdu& pdu) {
  if (pdu.data.size() < 1) {
    return Error{ErrorCategory::Protocol, 0, "Exception response missing exception code"};
  }
  uint8_t exCode = pdu.data[0];
  const char* name = "Unknown";
  switch (static_cast<ExceptionCode>(exCode)) {
    case ExceptionCode::IllegalFunction:         name = "Illegal Function"; break;
    case ExceptionCode::IllegalDataAddress:      name = "Illegal Data Address"; break;
    case ExceptionCode::IllegalDataValue:        name = "Illegal Data Value"; break;
    case ExceptionCode::ServerDeviceFailure:     name = "Server Device Failure"; break;
    case ExceptionCode::Acknowledge:             name = "Acknowledge"; break;
    case ExceptionCode::ServerDeviceBusy:        name = "Server Device Busy"; break;
    case ExceptionCode::MemoryParityError:       name = "Memory Parity Error"; break;
    case ExceptionCode::GatewayPathUnavailable:  name = "Gateway Path Unavailable"; break;
    case ExceptionCode::GatewayTargetNoResponse: name = "Gateway Target No Response"; break;
    default: break;
  }
  return Error{ErrorCategory::Device, static_cast<int>(exCode), name};
}

} // namespace modbus