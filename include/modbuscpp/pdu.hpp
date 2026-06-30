#ifndef MODBUS_PDU_HPP
#define MODBUS_PDU_HPP

#include "modbuscpp/error.hpp"
#include <cstdint>
#include <vector>

namespace modbus {

// Function codes supported in v1
enum class FunctionCode : uint8_t {
  ReadCoils              = 0x01,
  ReadDiscreteInputs     = 0x02,
  ReadHoldingRegisters   = 0x03,
  ReadInputRegisters     = 0x04,
  WriteSingleCoil        = 0x05,
  WriteSingleRegister    = 0x06,
  WriteMultipleCoils     = 0x0F,
  WriteMultipleRegisters = 0x10,
};

// Exception codes
enum class ExceptionCode : uint8_t {
  IllegalFunction          = 0x01,
  IllegalDataAddress       = 0x02,
  IllegalDataValue         = 0x03,
  ServerDeviceFailure      = 0x04,
  Acknowledge              = 0x05,
  ServerDeviceBusy         = 0x06,
  MemoryParityError        = 0x08,
  GatewayPathUnavailable   = 0x0A,
  GatewayTargetNoResponse  = 0x0B,
};

// Max PDU data size (excl. function code): 252
constexpr size_t kMaxPduDataSize = 252;

// A PDU = function code + data bytes (0..252 bytes)
struct Pdu {
  uint8_t             functionCode = 0;
  std::vector<uint8_t> data;       // 0..252 bytes
};

// ---------------------------------------------------------------------------
// Build helpers: each returns a Pdu ready for transport, or Error on bad args
// ---------------------------------------------------------------------------

/// Build a Read Coils request (FC 0x01): start address + quantity (1..2000)
Result<Pdu> buildReadCoils(uint16_t startAddress, uint16_t quantity);

/// Build a Read Discrete Inputs request (FC 0x02): start address + quantity (1..2000)
Result<Pdu> buildReadDiscreteInputs(uint16_t startAddress, uint16_t quantity);

/// Build a Read Holding Registers request (FC 0x03): start address + quantity (1..125)
Result<Pdu> buildReadHoldingRegisters(uint16_t startAddress, uint16_t quantity);

/// Build a Read Input Registers request (FC 0x04): start address + quantity (1..125)
Result<Pdu> buildReadInputRegisters(uint16_t startAddress, uint16_t quantity);

/// Build a Write Single Coil request (FC 0x05): address + value (0xFF00 = ON, 0x0000 = OFF)
Result<Pdu> buildWriteSingleCoil(uint16_t address, bool on);

/// Build a Write Single Register request (FC 0x06): address + value
Result<Pdu> buildWriteSingleRegister(uint16_t address, uint16_t value);

/// Build a Write Multiple Coils request (FC 0x0F): start address + quantity (1..1968) + packed bits
Result<Pdu> buildWriteMultipleCoils(uint16_t startAddress, const std::vector<bool>& values);

/// Build a Write Multiple Registers request (FC 0x10): start address + registers (1..123)
Result<Pdu> buildWriteMultipleRegisters(uint16_t startAddress, const std::vector<uint16_t>& values);

// ---------------------------------------------------------------------------
// Parse helpers: each takes the request Pdu and the response Pdu, returns
// the parsed result or an Error (Device exception, Protocol error, etc.)
// ---------------------------------------------------------------------------

/// Parse a Read Coils/Read Discrete Inputs response → vector of bools
Result<std::vector<bool>> parseReadBitsResponse(const Pdu& req, const Pdu& resp);

/// Parse a Read Holding Registers/Read Input Registers response → vector of uint16_t
Result<std::vector<uint16_t>> parseReadRegistersResponse(const Pdu& req, const Pdu& resp);

/// Parse a Write Single Coil/Write Single Register response → echoed address and value
Result<std::pair<uint16_t, uint16_t>> parseWriteSingleResponse(const Pdu& req, const Pdu& resp);

/// Parse a Write Multiple Coils response → echoed start + quantity
Result<std::pair<uint16_t, uint16_t>> parseWriteMultipleCoilsResponse(const Pdu& req, const Pdu& resp);

/// Parse a Write Multiple Registers response → echoed start + quantity
Result<std::pair<uint16_t, uint16_t>> parseWriteMultipleRegistersResponse(const Pdu& req, const Pdu& resp);

// ---------------------------------------------------------------------------
// Low-level helpers (used by link layer for ADU wrapping)
// ---------------------------------------------------------------------------

/// Check whether a response PDU represents a Modbus exception.
/// Returns true if the high bit is set (FC | 0x80).
bool isExceptionResponse(const Pdu& pdu);

/// Parse an exception response into an Error{Device, code, name}.
/// Precondition: isExceptionResponse(pdu) == true.
Error parseException(const Pdu& pdu);

// ---------------------------------------------------------------------------
// Validation helpers
// ---------------------------------------------------------------------------

/// Validate quantity limits. Returns an Error{Usage} if out of range.
Error validateCoilQuantity(uint16_t quantity);
Error validateRegisterQuantity(uint16_t quantity);
Error validateWriteMultipleCoilQuantity(uint16_t quantity);
Error validateWriteMultipleRegisterQuantity(uint16_t quantity);

} // namespace modbus

#endif // MODBUS_PDU_HPP
