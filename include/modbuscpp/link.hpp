#ifndef MODBUS_LINK_HPP
#define MODBUS_LINK_HPP

#include "modbuscpp/pdu.hpp"
#include <chrono>
#include <cstdint>
#include <string>

namespace modbus {

/// Owns the connection and ADU framing for one variant.
class IModbusLink {
public:
  virtual ~IModbusLink() = default;
  virtual Error open() = 0;
  virtual void  close() = 0;
  virtual bool  isOpen() const = 0;

  /// Send one request PDU to `unitId`, wait for and return the response PDU.
  virtual Result<Pdu> transact(uint8_t unitId,
                               const Pdu& request,
                               std::chrono::milliseconds timeout) = 0;
};

struct TcpConfig {
  std::string host = "127.0.0.1";
  uint16_t    port = 502;
  std::chrono::milliseconds connectTimeout{3000};
  bool        tcpNoDelay = true;   // REQUIRED default
};

enum class Parity { None, Even, Odd };

struct RtuConfig {
  std::string device;
  uint32_t    baud     = 9600;
  uint8_t     dataBits = 8;
  Parity      parity   = Parity::None;
  uint8_t     stopBits = 1;
};

} // namespace modbus

#endif // MODBUS_LINK_HPP
