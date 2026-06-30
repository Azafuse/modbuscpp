#ifndef MODBUS_TCP_LINK_HPP
#define MODBUS_TCP_LINK_HPP

#include "modbuscpp/link.hpp"

namespace modbus {

/// Modbus TCP link implementation.
/// Handles MBAP framing, TCP_NODELAY, timeout via select.
class TcpLink : public IModbusLink {
public:
  explicit TcpLink(const TcpConfig& config);
  ~TcpLink() override;

  Error open() override;
  void  close() override;
  bool  isOpen() const override;

  Result<Pdu> transact(uint8_t unitId,
                       const Pdu& request,
                       std::chrono::milliseconds timeout) override;

private:
  TcpConfig config_;
  int       sockfd_ = -1;
  uint16_t  txnId_  = 1;  // incrementing transaction ID, wraps at uint16_t

  // Internal helpers
  bool sendAll(const uint8_t* data, size_t len);
  bool recvAll(uint8_t* data, size_t len, std::chrono::milliseconds timeout);
  bool waitReadable(std::chrono::milliseconds timeout);
};

} // namespace modbus

#endif // MODBUS_TCP_LINK_HPP
