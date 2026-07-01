#ifndef MODBUS_TEST_MOCK_LINK_HPP
#define MODBUS_TEST_MOCK_LINK_HPP

#include "modbuscpp/link.hpp"
#include "modbuscpp/pdu.hpp"
#include <queue>
#include <mutex>

namespace modbus {
namespace test {

class MockLink : public IModbusLink {
public:
  MockLink() : open_(false) {}

  void queueResponse(const Pdu& pdu) {
    std::lock_guard<std::mutex> lock(mutex_);
    responses_.push(Result<Pdu>{pdu});
  }

  void queueError(Error err) {
    std::lock_guard<std::mutex> lock(mutex_);
    responses_.push(Result<Pdu>{err});
  }

  Error open() override {
    open_ = true;
    ++openCount_;
    return ok();
  }

  void close() override {
    open_ = false;
  }

  bool isOpen() const override {
    return open_;
  }

  void simulateDisconnect() { open_ = false; }
  int openCount() const { return openCount_; }

  Result<Pdu> transact(uint8_t, const Pdu&, std::chrono::milliseconds) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (responses_.empty()) {
      return Error{ErrorCategory::Transport, 0, "MockLink: no queued response"};
    }
    auto result = responses_.front();
    responses_.pop();
    return result;
  }

  std::vector<Pdu> requests;
  std::vector<uint8_t> unitIds;

private:
  bool open_;
  int openCount_ = 0;
  std::mutex mutex_;
  std::queue<Result<Pdu>> responses_;
};

} // namespace test
} // namespace modbus

#endif