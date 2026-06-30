#include "modbuscpp/tcp_link.hpp"

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using socklen_t = int;
  #define MODBUS_CLOSE_SOCKET(s) closesocket(s)
  #define MODBUS_LAST_ERROR() WSAGetLastError()
  #define MODBUS_INVALID_SOCKET INVALID_SOCKET
  #define MODBUS_SOCKET_ERROR SOCKET_ERROR
  #define MODBUS_WOULDBLOCK WSAEWOULDBLOCK
#else
  #include <sys/socket.h>
  #include <sys/select.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  #define MODBUS_CLOSE_SOCKET(s) close(s)
  #define MODBUS_LAST_ERROR() errno
  #define MODBUS_INVALID_SOCKET (-1)
  #define MODBUS_SOCKET_ERROR (-1)
  #define MODBUS_WOULDBLOCK EINPROGRESS
#endif

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <chrono>

namespace modbus {

// ---------------------------------------------------------------------------
// Windows Winsock init helper
// ---------------------------------------------------------------------------
#ifdef _WIN32
namespace {
  struct WinsockInit {
    WinsockInit() { WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa); }
    ~WinsockInit() { WSACleanup(); }
  };
  static WinsockInit gWinsock;
}
using socket_t = SOCKET;
#else
using socket_t = int;
#endif

// ---------------------------------------------------------------------------
// TcpLink
// ---------------------------------------------------------------------------

TcpLink::TcpLink(const TcpConfig& config) : config_(config) {}

TcpLink::~TcpLink() { close(); }

Error TcpLink::open() {
  if (sockfd_ >= 0) return ok();

  socket_t s = socket(AF_INET, SOCK_STREAM, 0);
  if (s == MODBUS_INVALID_SOCKET)
    return Error{ErrorCategory::Transport, MODBUS_LAST_ERROR(), "socket() failed"};

  // Set TCP_NODELAY
  int flag = config_.tcpNoDelay ? 1 : 0;
  setsockopt(s, IPPROTO_TCP, TCP_NODELAY,
    reinterpret_cast<const char*>(&flag), sizeof(flag));

  // Resolve host
  struct addrinfo hints = {};
  hints.ai_family   = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo* addr = nullptr;
  std::string portStr = std::to_string(config_.port);

  int rc = getaddrinfo(config_.host.c_str(), portStr.c_str(), &hints, &addr);
  if (rc != 0 || !addr) {
    MODBUS_CLOSE_SOCKET(s);
    return Error{ErrorCategory::Transport, rc, "getaddrinfo failed for " + config_.host};
  }

  // Non-blocking connect for timeout support
#ifdef _WIN32
  u_long mode = 1;
  ioctlsocket(s, FIONBIO, &mode);
#else
  int oldFlags = fcntl(s, F_GETFL, 0);
  fcntl(s, F_SETFL, oldFlags | O_NONBLOCK);
#endif

  rc = connect(s, addr->ai_addr, static_cast<socklen_t>(addr->ai_addrlen));
  freeaddrinfo(addr);

  if (rc == MODBUS_SOCKET_ERROR) {
    int err = MODBUS_LAST_ERROR();
    if (err != MODBUS_WOULDBLOCK) {
      MODBUS_CLOSE_SOCKET(s);
      return Error{ErrorCategory::Transport, err, "connect() failed"};
    }
    // Wait with select
    fd_set writeSet;
    FD_ZERO(&writeSet);
    FD_SET(s, &writeSet);
    struct timeval tv;
    auto ms = config_.connectTimeout.count();
    tv.tv_sec  = static_cast<long>(ms / 1000);
    tv.tv_usec = static_cast<long>((ms % 1000) * 1000);
    rc = select(static_cast<int>(s) + 1, nullptr, &writeSet, nullptr, &tv);
    if (rc <= 0) {
      MODBUS_CLOSE_SOCKET(s);
      return Error{ErrorCategory::Transport, 0,
        rc == 0 ? "Connection timeout" : "select() error during connect"};
    }
  }

  // Restore blocking mode
#ifdef _WIN32
  mode = 0;
  ioctlsocket(s, FIONBIO, &mode);
#else
  fcntl(s, F_SETFL, oldFlags);
#endif

  sockfd_ = static_cast<int>(s);
  txnId_  = 1;
  return ok();
}

void TcpLink::close() {
  if (sockfd_ >= 0) {
    MODBUS_CLOSE_SOCKET(sockfd_);
    sockfd_ = -1;
  }
}

bool TcpLink::isOpen() const {
  return sockfd_ >= 0;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

bool TcpLink::sendAll(const uint8_t* data, size_t len) {
  size_t sent = 0;
  while (sent < len) {
#ifdef _WIN32
    int n = send(sockfd_, reinterpret_cast<const char*>(data + sent),
                 static_cast<int>(len - sent), 0);
#else
    ssize_t n = ::send(sockfd_, data + sent, len - sent, MSG_NOSIGNAL);
#endif
    if (n <= 0) return false;
    sent += static_cast<size_t>(n);
  }
  return true;
}

bool TcpLink::waitReadable(std::chrono::milliseconds timeout) {
  fd_set readSet;
  FD_ZERO(&readSet);
  FD_SET(sockfd_, &readSet);
  struct timeval tv;
  auto ms = timeout.count();
  tv.tv_sec  = static_cast<long>(ms / 1000);
  tv.tv_usec = static_cast<long>((ms % 1000) * 1000);
  int rc = select(sockfd_ + 1, &readSet, nullptr, nullptr, &tv);
  return rc > 0;
}

bool TcpLink::recvAll(uint8_t* data, size_t len, std::chrono::milliseconds timeout) {
  size_t received = 0;
  auto deadline = std::chrono::steady_clock::now() + timeout;

  while (received < len) {
    if (!waitReadable(timeout)) return false;

#ifdef _WIN32
    int n = recv(sockfd_, reinterpret_cast<char*>(data + received),
                 static_cast<int>(len - received), 0);
#else
    ssize_t n = ::recv(sockfd_, data + received, len - received, 0);
#endif
    if (n <= 0) return false;
    received += static_cast<size_t>(n);

    auto now = std::chrono::steady_clock::now();
    if (now >= deadline) return false;
    timeout = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
  }
  return true;
}

// ---------------------------------------------------------------------------
// transact
// ---------------------------------------------------------------------------

Result<Pdu> TcpLink::transact(uint8_t unitId,
                              const Pdu& request,
                              std::chrono::milliseconds timeout) {
  if (sockfd_ < 0)
    return Error{ErrorCategory::Transport, 0, "Not connected"};

  // Build MBAP header + PDU
  uint16_t txnId = txnId_++;
  if (txnId_ == 0) txnId_ = 1;

  // Length = 1 (unitId) + 1 (functionCode) + data.size()
  uint16_t length = static_cast<uint16_t>(2 + request.data.size());
  size_t aduSize = 7 + 1 + request.data.size(); // MBAP(7) + FC(1) + data
  std::vector<uint8_t> adu;
  adu.reserve(aduSize);
  // Transaction ID (big-endian)
  adu.push_back(static_cast<uint8_t>((txnId >> 8) & 0xFF));
  adu.push_back(static_cast<uint8_t>(txnId & 0xFF));
  // Protocol ID (0)
  adu.push_back(0x00);
  adu.push_back(0x00);
  // Length (big-endian)
  adu.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
  adu.push_back(static_cast<uint8_t>(length & 0xFF));
  // Unit ID
  adu.push_back(unitId);
  // PDU
  adu.push_back(request.functionCode);
  adu.insert(adu.end(), request.data.begin(), request.data.end());

  // Send
  if (!sendAll(adu.data(), adu.size())) {
    close();
    return Error{ErrorCategory::Transport, 0, "Send failed"};
  }

  // Receive 7-byte MBAP header
  uint8_t mbap[7];
  if (!recvAll(mbap, 7, timeout)) {
    close();
    return Error{ErrorCategory::Transport, 0, "Receive MBAP header failed / timeout"};
  }

  // Parse MBAP
  uint16_t respTxnId = static_cast<uint16_t>((mbap[0] << 8) | mbap[1]);
  uint16_t respLength = static_cast<uint16_t>((mbap[4] << 8) | mbap[5]);

  if (mbap[2] != 0x00 || mbap[3] != 0x00) {
    close();
    return Error{ErrorCategory::Protocol, 0, "Protocol ID != 0 in response"};
  }
  if (respTxnId != txnId) {
    close();
    return Error{ErrorCategory::Protocol, 0,
      "Transaction ID mismatch: expected " + std::to_string(txnId)};
  }
  if (respLength < 2) {
    close();
    return Error{ErrorCategory::Protocol, 0,
      "Response length too small: " + std::to_string(respLength)};
  }

  // Read remaining bytes: Length includes the UnitId (byte 6 of MBAP),
  // which we already consumed. So remaining = respLength - 1.
  size_t remaining = static_cast<size_t>(respLength - 1);
  if (remaining == 0) {
    close();
    return Error{ErrorCategory::Protocol, 0, "Response PDU is empty"};
  }
  std::vector<uint8_t> pduData(remaining);
  if (!recvAll(pduData.data(), remaining, timeout)) {
    close();
    return Error{ErrorCategory::Transport, 0, "Receive PDU data failed / timeout"};
  }

  // Build response PDU
  Pdu resp;
  resp.functionCode = pduData[0];
  resp.data.assign(pduData.begin() + 1, pduData.end());
  return resp;
}

} // namespace modbus