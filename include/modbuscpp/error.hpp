#ifndef MODBUS_ERROR_HPP
#define MODBUS_ERROR_HPP

#include <string>
#include <utility>

namespace modbus {

enum class ErrorCategory {
  None,       // success
  Usage,      // caller misused the API (bad count, not connected, etc.)
  Transport,  // socket/serial: connect failed, timeout, connection closed, read error
  Protocol,   // malformed reply: CRC fail, bad length, txn-id mismatch, unexpected FC
  Device      // server returned a Modbus exception response (see code)
};

struct Error {
  ErrorCategory category = ErrorCategory::None;
  int           code     = 0;   // category-specific. For Device: the Modbus exception code (§8.5).
  std::string   message;         // human-readable, for logs/UI

  bool ok()  const { return category == ErrorCategory::None; }
  explicit operator bool() const { return !ok(); } // truthy when there IS an error
};

inline Error ok() { return Error{}; }

template <typename T>
class Result {
public:
  Result(T value) : value_(std::move(value)) {}
  Result(Error err) : error_(std::move(err)) {}

  bool ok() const { return error_.ok(); }
  const T&     value() const { return value_; }   // precondition: ok()
  T&           value()       { return value_; }
  const Error& error() const { return error_; }
private:
  T     value_{};
  Error error_{};
};

} // namespace modbus

#endif // MODBUS_ERROR_HPP
