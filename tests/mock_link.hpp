#ifndef MODBUS_TEST_MOCK_LINK_HPP
#define MODBUS_TEST_MOCK_LINK_HPP

#include "modbuscpp/link.hpp"
#include "modbuscpp/pdu.hpp"
#include <queue>
#include <mutex>

namespace modbus {
namespace test {

/// Mock IModbusLink for unit testing ModbusClient without real I/O.
/// Queue up canned Pdu responses (or Error results), which are consumed
/// in order by transact().
class MockLink : public IModbusLink {
public:
  MockLink() : open_(false) {}

  // Queue a successful response PDU
  void queueResponse(const Pdu& pdu) {
    std::lock_guard<std::mutex> lock(mutex_);
    responses_.push(Result<Pdu>{pdu});
  }

  // Queue an error
  void queueError(Error err) {
    std::lock_guard<std::mutex> lock(mutex_);
    responses_.push(Result<Pdu>{err});
  }

  Error open() override {
    open_ = true;
    return ok();
  }

  void close() override {
    open_ = false;
  }

  bool isOpen() const override {
    return open_;
  }

  Result<Pdu> transact(uint8_t /*unitId*/,
                       const Pdu& /*request*/,
                       std::chrono::milliseconds /*timeout*/) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (responses_.empty()) {
      return Error{ErrorCategory::Transport, 0, "MockLink: no queued response"};
    }
    auto result = responses_.front();
    responses_.pop();
    return result;
  }

  // Get captured requests (for verification)
  std::vector<Pdu> requests;
  std::vector<uint8_t> unitIds;

private:
  bool open_;
  std::mutex mutex_;
  std::queue<Result<Pdu>> responses_;
};

} // namespace test
} // namespace modbus

#endif // MODBUS_TEST_MOCK_LINK_HPP
