#ifndef MODBUS_RTU_LINK_HPP
#define MODBUS_RTU_LINK_HPP

#include "modbuscpp/link.hpp"

namespace modbus {

class RtuLink final : public IModbusLink {
public:
  explicit RtuLink(RtuConfig config);
  ~RtuLink() override;
  RtuLink(const RtuLink&) = delete;
  RtuLink& operator=(const RtuLink&) = delete;
  RtuLink(RtuLink&&) noexcept;
  RtuLink& operator=(RtuLink&&) noexcept;

  Error open() override;
  void  close() override;
  bool  isOpen() const override;

  Result<Pdu> transact(uint8_t unitId,
                       const Pdu& request,
                       std::chrono::milliseconds timeout) override;

  /// Returns the computed t3.5 silent interval (in milliseconds) for the
  /// configured baud rate and character format.
  std::chrono::milliseconds t35Interval() const;

private:
  RtuConfig cfg_;
  int       fd_ = -1;       // -1 = closed (Win32: HANDLE cast to intptr_t)
  bool      isWin32_ = false; // true on Windows (file handle vs fd)

  std::chrono::milliseconds t35_; // cached t3.5 interval

  bool configurePort();
  bool setSilentInterval();

  bool sendAll(const uint8_t* data, size_t len);
  bool recvByte(uint8_t& b, std::chrono::milliseconds timeout);
};

} // namespace modbus

#endif // MODBUS_RTU_LINK_HPP
