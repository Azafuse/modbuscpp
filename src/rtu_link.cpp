#include "modbuscpp/rtu_link.hpp"
#include "modbuscpp/crc.hpp"

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <fileapi.h>
  #include <handleapi.h>
#else
  #include <unistd.h>
  #include <fcntl.h>
  #include <termios.h>
  #include <sys/select.h>
  #include <sys/time.h>
  #include <sys/types.h>
  #include <cerrno>
  #include <cstring>
#endif

#include <chrono>
#include <thread>
#include <vector>
#include <cstdio>

namespace modbus {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::chrono::milliseconds computeT35(uint32_t baud, uint8_t dataBits,
                                            Parity parity, uint8_t stopBits) {
  // Total bits per character: start(1) + data + parity + stop
  int bitsPerChar = 1 + dataBits + stopBits;
  if (parity != Parity::None) bitsPerChar += 1;
  // t3.5 = 3.5 character times, in microseconds, then to ms
  double charTimeUs = (static_cast<double>(bitsPerChar) / static_cast<double>(baud)) * 1'000'000.0;
  double t35Us = 3.5 * charTimeUs;
  auto ms = static_cast<long long>(t35Us / 1000.0 + 0.5);
  if (ms < 1) ms = 1;
  return std::chrono::milliseconds(ms);
}

// ---------------------------------------------------------------------------
// RtuLink
// ---------------------------------------------------------------------------

RtuLink::RtuLink(RtuConfig config)
  : cfg_(std::move(config))
  , t35_(computeT35(cfg_.baud, cfg_.dataBits, cfg_.parity, cfg_.stopBits))
{
#ifdef _WIN32
  isWin32_ = true;
#endif
}

RtuLink::~RtuLink() { close(); }

RtuLink::RtuLink(RtuLink&& other) noexcept
  : cfg_(std::move(other.cfg_)), fd_(other.fd_),
    isWin32_(other.isWin32_), t35_(other.t35_)
{
  other.fd_ = -1;
}

RtuLink& RtuLink::operator=(RtuLink&& other) noexcept {
  if (this != &other) {
    close();
    cfg_ = std::move(other.cfg_);
    fd_ = other.fd_;
    isWin32_ = other.isWin32_;
    t35_ = other.t35_;
    other.fd_ = -1;
  }
  return *this;
}

std::chrono::milliseconds RtuLink::t35Interval() const { return t35_; }

// ---------------------------------------------------------------------------
// open / close
// ---------------------------------------------------------------------------

Error RtuLink::open() {
  if (fd_ != -1) { close(); }

#ifdef _WIN32
  // Convert narrow string to wide for CreateFileW
  int wlen = MultiByteToWideChar(CP_UTF8, 0, cfg_.device.c_str(), -1, nullptr, 0);
  if (wlen <= 0) {
    return Error{ErrorCategory::Transport, 0, "Failed to convert device path"};
  }
  std::wstring wdev(wlen, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, cfg_.device.c_str(), -1, &wdev[0], wlen);

  HANDLE h = CreateFileW(
    wdev.c_str(),
    GENERIC_READ | GENERIC_WRITE,
    0, nullptr, OPEN_EXISTING,
    0,  // no attributes needed for serial
    nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    DWORD err = GetLastError();
    return Error{ErrorCategory::Transport, static_cast<int>(err),
                 "Cannot open " + cfg_.device + " (Win32 error " +
                 std::to_string(err) + ")"};
  }
  fd_ = reinterpret_cast<intptr_t>(h);
#else
  fd_ = ::open(cfg_.device.c_str(), O_RDWR | O_NOCTTY);
  if (fd_ < 0) {
    return Error{ErrorCategory::Transport, errno,
                 "Cannot open " + cfg_.device + ": " + std::strerror(errno)};
  }
#endif

  if (!configurePort()) {
    close();
    return Error{ErrorCategory::Transport, 0, "Failed to configure serial port"};
  }
  return ok();
}

void RtuLink::close() {
  if (fd_ == -1) return;
#ifdef _WIN32
  HANDLE h = reinterpret_cast<HANDLE>(fd_);
  CloseHandle(h);
#else
  ::close(fd_);
#endif
  fd_ = -1;
}

bool RtuLink::isOpen() const { return fd_ != -1; }

// ---------------------------------------------------------------------------
// Serial configuration
// ---------------------------------------------------------------------------

bool RtuLink::configurePort() {
#ifdef _WIN32
  HANDLE h = reinterpret_cast<HANDLE>(fd_);
  DCB dcb = {};
  dcb.DCBlength = sizeof(dcb);
  if (!GetCommState(h, &dcb)) return false;
  dcb.BaudRate = cfg_.baud;
  dcb.ByteSize = cfg_.dataBits;
  switch (cfg_.parity) {
    case Parity::None: dcb.Parity = NOPARITY; break;
    case Parity::Even: dcb.Parity = EVENPARITY; break;
    case Parity::Odd:  dcb.Parity = ODDPARITY;  break;
  }
  dcb.StopBits = (cfg_.stopBits == 2) ? TWOSTOPBITS : ONESTOPBIT;
  dcb.fBinary = TRUE;
  dcb.fOutxCtsFlow = FALSE; dcb.fOutxDsrFlow = FALSE;
  dcb.fDtrControl = DTR_CONTROL_DISABLE;
  dcb.fRtsControl = RTS_CONTROL_DISABLE;
  dcb.fOutX = FALSE; dcb.fInX = FALSE;
  dcb.fParity = (cfg_.parity != Parity::None) ? TRUE : FALSE;
  if (!SetCommState(h, &dcb)) return false;
  COMMTIMEOUTS ct = {};
  ct.ReadIntervalTimeout = MAXDWORD;
  ct.ReadTotalTimeoutConstant = 0;
  ct.ReadTotalTimeoutMultiplier = 0;
  if (!SetCommTimeouts(h, &ct)) return false;
  return true;
#else
  struct termios tty;
  if (tcgetattr(fd_, &tty) != 0) return false;
  speed_t speed = B9600;
  switch (cfg_.baud) {
    case 1200: speed = B1200; break; case 2400: speed = B2400; break;
    case 4800: speed = B4800; break; case 9600: speed = B9600; break;
    case 19200: speed = B19200; break; case 38400: speed = B38400; break;
    case 57600: speed = B57600; break; case 115200: speed = B115200; break;
    default: speed = B9600; break;
  }
  cfsetospeed(&tty, speed); cfsetispeed(&tty, speed);
  tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
  tty.c_cflag |= CREAD | CLOCAL;
  tty.c_lflag = 0; tty.c_iflag = IGNPAR; tty.c_oflag = 0;
  if (cfg_.parity == Parity::Even) {
    tty.c_cflag |= PARENB; tty.c_cflag &= ~PARODD;
  } else if (cfg_.parity == Parity::Odd) {
    tty.c_cflag |= PARENB | PARODD;
  } else { tty.c_cflag &= ~PARENB; }
  if (cfg_.stopBits == 2) tty.c_cflag |= CSTOPB; else tty.c_cflag &= ~CSTOPB;
  cfmakeraw(&tty);
  tty.c_cc[VMIN] = 0; tty.c_cc[VTIME] = 1;
  if (tcsetattr(fd_, TCSANOW, &tty) != 0) return false;
  return true;
#endif
}

// ---------------------------------------------------------------------------
// sendAll
// ---------------------------------------------------------------------------

bool RtuLink::sendAll(const uint8_t* data, size_t len) {
  size_t written = 0;
  while (written < len) {
#ifdef _WIN32
    HANDLE h = reinterpret_cast<HANDLE>(fd_);
    DWORD n = 0;
    if (!WriteFile(h, data + written,
                   static_cast<DWORD>(len - written), &n, nullptr))
      return false;
    written += n;
#else
    ssize_t n = ::write(fd_, data + written, len - written);
    if (n < 0 && errno != EAGAIN) return false;
    if (n > 0) written += static_cast<size_t>(n);
#endif
  }
  return true;
}

// ---------------------------------------------------------------------------
// recvByte with timeout
// ---------------------------------------------------------------------------

bool RtuLink::recvByte(uint8_t& b, std::chrono::milliseconds timeout) {
  using namespace std::chrono;
  auto deadline = steady_clock::now() + timeout;
  while (true) {
    auto rem = duration_cast<milliseconds>(deadline - steady_clock::now());
    if (rem.count() <= 0) return false;
#ifdef _WIN32
    HANDLE h = reinterpret_cast<HANDLE>(fd_);
    COMMTIMEOUTS ct = {};
    ct.ReadIntervalTimeout = MAXDWORD;
    ct.ReadTotalTimeoutConstant = static_cast<DWORD>(rem.count());
    ct.ReadTotalTimeoutMultiplier = 0;
    SetCommTimeouts(h, &ct);
    DWORD n = 0; uint8_t buf;
    if (!ReadFile(h, &buf, 1, &n, nullptr)) return false;
    if (n == 0) return false;
    b = buf; return true;
#else
    fd_set fds; FD_ZERO(&fds); FD_SET(fd_, &fds);
    struct timeval tv;
    tv.tv_sec  = static_cast<long>(rem.count() / 1000);
    tv.tv_usec = static_cast<long>((rem.count() % 1000) * 1000);
    if (select(fd_ + 1, &fds, nullptr, nullptr, &tv) <= 0) return false;
    ssize_t n = ::read(fd_, &b, 1);
    if (n <= 0) return false;
    return true;
#endif
  }
}

// ---------------------------------------------------------------------------
// transact — RTU framing with CRC
// ---------------------------------------------------------------------------

Result<Pdu> RtuLink::transact(uint8_t unitId,
                              const Pdu& request,
                              std::chrono::milliseconds timeout) {
  if (fd_ == -1)
    return Error{ErrorCategory::Transport, 0, "RTU port not open"};

  // Build frame: [addr][FC][data][CRC lo][CRC hi]
  std::vector<uint8_t> frame;
  frame.reserve(1 + 1 + request.data.size() + 2);
  frame.push_back(unitId);
  frame.push_back(request.functionCode);
  frame.insert(frame.end(), request.data.begin(), request.data.end());
  uint16_t crc = crc16(frame.data(), frame.size());
  frame.push_back(static_cast<uint8_t>(crc & 0xFF));
  frame.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));

  // t3.5 silence
  std::this_thread::sleep_for(t35_);

  // Flush buffers
#ifdef _WIN32
  PurgeComm(reinterpret_cast<HANDLE>(fd_), PURGE_RXCLEAR | PURGE_TXCLEAR);
#else
  tcflush(fd_, TCIOFLUSH);
#endif

  if (!sendAll(frame.data(), frame.size())) {
    close();
    return Error{ErrorCategory::Transport, 0, "RTU send failed"};
  }

  // --- Read response ---
  using namespace std::chrono;
  auto deadline = steady_clock::now() + timeout;

  // First byte
  uint8_t b;
  auto rem = duration_cast<milliseconds>(deadline - steady_clock::now());
  if (rem.count() <= 0 || !recvByte(b, rem)) {
    close();
    return Error{ErrorCategory::Transport, 0, "RTU response timeout"};
  }

  std::vector<uint8_t> resp;
  resp.push_back(b);

  // t1.5 ≈ t3.5 / 2, min 2ms
  auto t15 = t35_ / 2;
  if (t15.count() < 2) t15 = std::chrono::milliseconds(2);

  while (true) {
    rem = duration_cast<milliseconds>(deadline - steady_clock::now());
    if (rem.count() <= 0) break;
    auto ib = t15;
    if (ib.count() > rem.count()) ib = rem;
    if (!recvByte(b, ib)) break;
    resp.push_back(b);
  }

  if (resp.size() < 4) {
    close();
    return Error{ErrorCategory::Transport, 0,
      "RTU response too short: " + std::to_string(resp.size()) + " bytes"};
  }

  // CRC: Modbus RTU sends CRC as [low][high]
  uint16_t rxCrc = static_cast<uint16_t>(resp[resp.size() - 1]) << 8 |
                   static_cast<uint16_t>(resp[resp.size() - 2]);
  uint16_t calcCrc = crc16(resp.data(), resp.size() - 2);
  if (rxCrc != calcCrc) {
    return Error{ErrorCategory::Protocol, 0, "RTU CRC mismatch"};
  }

  if (resp[0] != unitId) {
    return Error{ErrorCategory::Protocol, 0, "RTU address mismatch"};
  }

  Pdu response;
  response.functionCode = resp[1];
  response.data.assign(resp.begin() + 2, resp.end() - 2);
  return response;
}

} // namespace modbus

