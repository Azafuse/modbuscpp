#include "modbuscpp/client.hpp"
#include <thread>

namespace modbus {

ModbusClient::ModbusClient(std::unique_ptr<IModbusLink> link)
  : link_(std::move(link)) {}

Error ModbusClient::connect() {
  std::lock_guard<std::mutex> lock(mutex_);
  intentionalDisconnect_ = false;
  reconnecting_ = false;
  return link_->open();
}

void ModbusClient::disconnect() {
  std::lock_guard<std::mutex> lock(mutex_);
  intentionalDisconnect_ = true;
  reconnecting_ = false;
  link_->close();
}

bool ModbusClient::connected() const {
  return link_->isOpen();
}

bool ModbusClient::isReconnecting() const {
  return reconnecting_;
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

void ModbusClient::setAutoReconnect(bool on) {
  std::lock_guard<std::mutex> lock(mutex_);
  autoReconnect_ = on;
}

void ModbusClient::setReconnectBackoff(std::chrono::milliseconds minMs,
                                       std::chrono::milliseconds maxMs) {
  std::lock_guard<std::mutex> lock(mutex_);
  reconnectMinMs_ = minMs;
  reconnectMaxMs_ = maxMs;
}

Result<Pdu> ModbusClient::transactWithRetry(const Pdu& request) {
  int attempts = 0;
  while (attempts <= retries_) {
    if (!link_->isOpen()) {
      if (!autoReconnect_ || intentionalDisconnect_) {
        return Error{ErrorCategory::Transport, 0, "Not connected"};
      }
      reconnecting_ = true;
      auto delay = reconnectMinMs_;
      bool reconnected = false;
      for (int rcA = 0; rcA <= retries_ && !reconnected; ++rcA) {
        if (rcA > 0) {
          std::this_thread::sleep_for(delay);
          delay = std::min(delay * 2, reconnectMaxMs_);
        }
        if (link_->open().ok()) reconnected = true;
      }
      reconnecting_ = false;
      if (!reconnected) {
        return Error{ErrorCategory::Transport, 0,
          "Reconnect failed after " + std::to_string(retries_ + 1) + " attempts"};
      }
    }
    auto result = link_->transact(unitId_, request, timeout_);
    if (result.ok()) return result;
    if (result.error().category != ErrorCategory::Transport) return result;
    ++attempts;
    if (attempts <= retries_) std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return Error{ErrorCategory::Transport, 0, "All retries exhausted"};
}

Result<std::vector<bool>> ModbusClient::readCoils(uint16_t start, uint16_t count) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto pdu = buildReadCoils(start, count);
  if (!pdu.ok()) return pdu.error();
  auto resp = transactWithRetry(pdu.value());
  if (!resp.ok()) return resp.error();
  if (isExceptionResponse(resp.value())) return parseException(resp.value());
  return parseReadBitsResponse(pdu.value(), resp.value());
}

Result<std::vector<bool>> ModbusClient::readDiscreteInputs(uint16_t start, uint16_t count) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto pdu = buildReadDiscreteInputs(start, count);
  if (!pdu.ok()) return pdu.error();
  auto resp = transactWithRetry(pdu.value());
  if (!resp.ok()) return resp.error();
  if (isExceptionResponse(resp.value())) return parseException(resp.value());
  return parseReadBitsResponse(pdu.value(), resp.value());
}

Result<std::vector<uint16_t>> ModbusClient::readHoldingRegisters(uint16_t start, uint16_t count) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto pdu = buildReadHoldingRegisters(start, count);
  if (!pdu.ok()) return pdu.error();
  auto resp = transactWithRetry(pdu.value());
  if (!resp.ok()) return resp.error();
  if (isExceptionResponse(resp.value())) return parseException(resp.value());
  return parseReadRegistersResponse(pdu.value(), resp.value());
}

Result<std::vector<uint16_t>> ModbusClient::readInputRegisters(uint16_t start, uint16_t count) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto pdu = buildReadInputRegisters(start, count);
  if (!pdu.ok()) return pdu.error();
  auto resp = transactWithRetry(pdu.value());
  if (!resp.ok()) return resp.error();
  if (isExceptionResponse(resp.value())) return parseException(resp.value());
  return parseReadRegistersResponse(pdu.value(), resp.value());
}

Error ModbusClient::writeSingleCoil(uint16_t address, bool on) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto pdu = buildWriteSingleCoil(address, on);
  if (!pdu.ok()) return pdu.error();
  auto resp = transactWithRetry(pdu.value());
  if (!resp.ok()) return resp.error();
  if (isExceptionResponse(resp.value())) return parseException(resp.value());
  return ok();
}

Error ModbusClient::writeSingleRegister(uint16_t address, uint16_t value) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto pdu = buildWriteSingleRegister(address, value);
  if (!pdu.ok()) return pdu.error();
  auto resp = transactWithRetry(pdu.value());
  if (!resp.ok()) return resp.error();
  if (isExceptionResponse(resp.value())) return parseException(resp.value());
  return ok();
}

Error ModbusClient::writeMultipleCoils(uint16_t start, const std::vector<bool>& values) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto pdu = buildWriteMultipleCoils(start, values);
  if (!pdu.ok()) return pdu.error();
  auto resp = transactWithRetry(pdu.value());
  if (!resp.ok()) return resp.error();
  if (isExceptionResponse(resp.value())) return parseException(resp.value());
  return ok();
}

Error ModbusClient::writeMultipleRegisters(uint16_t start, const std::vector<uint16_t>& values) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto pdu = buildWriteMultipleRegisters(start, values);
  if (!pdu.ok()) return pdu.error();
  auto resp = transactWithRetry(pdu.value());
  if (!resp.ok()) return resp.error();
  if (isExceptionResponse(resp.value())) return parseException(resp.value());
  return ok();
}

} // namespace modbus