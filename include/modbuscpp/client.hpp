#ifndef MODBUS_CLIENT_HPP
#define MODBUS_CLIENT_HPP

#include "modbuscpp/link.hpp"
#include "modbuscpp/pdu.hpp"
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>

namespace modbus {

class ModbusClient {
public:
  explicit ModbusClient(std::unique_ptr<IModbusLink> link);

  Error connect();
  void  disconnect();
  bool  connected() const;

  void setUnitId(uint8_t id);
  void setTimeout(std::chrono::milliseconds t);
  void setRetries(int n);

  // Reads
  Result<std::vector<bool>>     readCoils(uint16_t start, uint16_t count);
  Result<std::vector<bool>>     readDiscreteInputs(uint16_t start, uint16_t count);
  Result<std::vector<uint16_t>> readHoldingRegisters(uint16_t start, uint16_t count);
  Result<std::vector<uint16_t>> readInputRegisters(uint16_t start, uint16_t count);

  // Writes
  Error writeSingleCoil(uint16_t address, bool on);
  Error writeSingleRegister(uint16_t address, uint16_t value);
  Error writeMultipleCoils(uint16_t start, const std::vector<bool>& values);
  Error writeMultipleRegisters(uint16_t start, const std::vector<uint16_t>& values);

private:
  std::unique_ptr<IModbusLink> link_;
  std::mutex                   mutex_;
  uint8_t                      unitId_  = 1;
  std::chrono::milliseconds    timeout_{1000};
  int                          retries_ = 0;

  // Internal: execute one transaction with retry on Transport errors
  Result<Pdu> transactWithRetry(const Pdu& request);
};

} // namespace modbus

#endif // MODBUS_CLIENT_HPP
