#include "modbuscpp/client.hpp"
#include <thread>

namespace modbus {

ModbusClient::ModbusClient(std::unique_ptr<IModbusLink> link)
  : link_(std::move(link)) {}

Error ModbusClient::connect() {
  std::lock_guard<std::mutex> lock(mutex_);
  return link_->open();
}

void ModbusClient::disconnect() {
  std::lock_guard<std::mutex> lock(mutex_);
  link_->close();
}

bool ModbusClient::connected() const {
  return link_->isOpen();
}

void ModbusClient::setUnitId(uint8_t id) {
  std::lock_guard<std::mutex> lock(mutex_);
  unitId_ = id;
}

void ModbusClient::setTimeout(std::chrono::milliseconds t) {
  std::lock_guard<std::mutex> lock(mutex_);
  timeout_ = t;
}

void ModbusClient::setRetries(int n) {
  std::lock_guard<std::mutex> lock(mutex_);
  retries_ = n;
}

// ---------------------------------------------------------------------------
// Internal: transact with retry
// ---------------------------------------------------------------------------

Result<Pdu> ModbusClient::transactWithRetry(const Pdu& request) {
  int attempts = 0;
  Result<Pdu> result{Error{ErrorCategory::Transport, 0, "No attempts"}};

  while (attempts <= retries_) {
    result = link_->transact(unitId_, request, timeout_);
    if (result.ok()) return result;

    // Only retry on Transport errors
    if (result.error().category != ErrorCategory::Transport)
      return result;

    ++attempts;
    if (attempts <= retries_) {
      // Small back-off before retry
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
  return result;
}

// ---------------------------------------------------------------------------
// Read operations
// ---------------------------------------------------------------------------

Result<std::vector<bool>> ModbusClient::readCoils(uint16_t start, uint16_t count) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto pdu = buildReadCoils(start, count);
  if (!pdu.ok()) return pdu.error();
  auto resp = transactWithRetry(pdu.value());
  if (!resp.ok()) return resp.error();
  if (isExceptionResponse(resp.value()))
    return parseException(resp.value());
  return parseReadBitsResponse(pdu.value(), resp.value());
}

Result<std::vector<bool>> ModbusClient::readDiscreteInputs(uint16_t start, uint16_t count) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto pdu = buildReadDiscreteInputs(start, count);
  if (!pdu.ok()) return pdu.error();
  auto resp = transactWithRetry(pdu.value());
  if (!resp.ok()) return resp.error();
  if (isExceptionResponse(resp.value()))
    return parseException(resp.value());
  return parseReadBitsResponse(pdu.value(), resp.value());
}

Result<std::vector<uint16_t>> ModbusClient::readHoldingRegisters(uint16_t start, uint16_t count) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto pdu = buildReadHoldingRegisters(start, count);
  if (!pdu.ok()) return pdu.error();
  auto resp = transactWithRetry(pdu.value());
  if (!resp.ok()) return resp.error();
  if (isExceptionResponse(resp.value()))
    return parseException(resp.value());
  return parseReadRegistersResponse(pdu.value(), resp.value());
}

Result<std::vector<uint16_t>> ModbusClient::readInputRegisters(uint16_t start, uint16_t count) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto pdu = buildReadInputRegisters(start, count);
  if (!pdu.ok()) return pdu.error();
  auto resp = transactWithRetry(pdu.value());
  if (!resp.ok()) return resp.error();
  if (isExceptionResponse(resp.value()))
    return parseException(resp.value());
  return parseReadRegistersResponse(pdu.value(), resp.value());
}

// ---------------------------------------------------------------------------
// Write operations
// ---------------------------------------------------------------------------

Error ModbusClient::writeSingleCoil(uint16_t address, bool on) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto pdu = buildWriteSingleCoil(address, on);
  if (!pdu.ok()) return pdu.error();
  auto resp = transactWithRetry(pdu.value());
  if (!resp.ok()) return resp.error();
  if (isExceptionResponse(resp.value()))
    return parseException(resp.value());
  return ok();
}

Error ModbusClient::writeSingleRegister(uint16_t address, uint16_t value) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto pdu = buildWriteSingleRegister(address, value);
  if (!pdu.ok()) return pdu.error();
  auto resp = transactWithRetry(pdu.value());
  if (!resp.ok()) return resp.error();
  if (isExceptionResponse(resp.value()))
    return parseException(resp.value());
  return ok();
}

Error ModbusClient::writeMultipleCoils(uint16_t start, const std::vector<bool>& values) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto pdu = buildWriteMultipleCoils(start, values);
  if (!pdu.ok()) return pdu.error();
  auto resp = transactWithRetry(pdu.value());
  if (!resp.ok()) return resp.error();
  if (isExceptionResponse(resp.value()))
    return parseException(resp.value());
  return ok();
}

Error ModbusClient::writeMultipleRegisters(uint16_t start, const std::vector<uint16_t>& values) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto pdu = buildWriteMultipleRegisters(start, values);
  if (!pdu.ok()) return pdu.error();
  auto resp = transactWithRetry(pdu.value());
  if (!resp.ok()) return resp.error();
  if (isExceptionResponse(resp.value()))
    return parseException(resp.value());
  return ok();
}

} // namespace modbus
