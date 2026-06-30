#include <doctest/doctest.h>
#include "modbuscpp/tcp_link.hpp"
#include "modbuscpp/pdu.hpp"
#include <chrono>
#include <thread>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <memory>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <signal.h>
#endif

using namespace modbus;
using namespace std::chrono_literals;

// Helper: start Python test server in background, returns PID/handle
struct TestServer {
#ifdef _WIN32
  HANDLE process = nullptr;
#else
  pid_t pid = 0;
#endif
  bool running_ = false;

  bool start() {
#ifdef _WIN32
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    std::string cmd = "python ../tests/modbus_test_server.py 1502";
    char* cmdBuf = new char[cmd.size() + 1];
    std::strcpy(cmdBuf, cmd.c_str());

    if (!CreateProcessA(nullptr, cmdBuf, nullptr, nullptr, FALSE,
                        0, nullptr, nullptr, &si, &pi)) {
      delete[] cmdBuf;
      return false;
    }
    delete[] cmdBuf;
    CloseHandle(pi.hThread);
    process = pi.hProcess;
    running_ = true;
    // Wait for server to start
    std::this_thread::sleep_for(1500ms);
    return true;
#else
    pid = fork();
    if (pid == 0) {
      execlp("python3", "python3", "tests/modbus_test_server.py", "1502", nullptr);
      _exit(1);
    }
    if (pid > 0) {
      running_ = true;
      std::this_thread::sleep_for(1500ms);
      return true;
    }
    return false;
#endif
  }

  void stop() {
    if (!running_) return;
    running_ = false;
#ifdef _WIN32
    TerminateProcess(process, 0);
    WaitForSingleObject(process, 5000);
    CloseHandle(process);
    process = nullptr;
#else
    kill(pid, SIGTERM);
    waitpid(pid, nullptr, 0);
    pid = 0;
#endif
  }

  ~TestServer() { stop(); }
};

TEST_CASE("TcpLink: MBAP framing verification") {
  // This test verifies the MBAP header construction indirectly by
  // checking that transact() builds the correct ADU for a simple request.
  // We test via a local Python Modbus server.

  TestServer server;
  if (!server.start()) {
    MESSAGE("Skipping TcpLink test: could not start test server");
    return;
  }

  TcpConfig cfg;
  cfg.host = "127.0.0.1";
  cfg.port = 1502;
  cfg.connectTimeout = 3000ms;

  TcpLink link(cfg);

  SUBCASE("Connect and disconnect") {
    auto err = link.open();
    CHECK(err.ok());
    CHECK(link.isOpen());
    link.close();
    CHECK(!link.isOpen());
  }

  SUBCASE("Read Holding Registers via TcpLink") {
    REQUIRE(link.open().ok());

    // Build a read holding registers request: start=0, qty=3
    auto reqResult = buildReadHoldingRegisters(0, 3);
    REQUIRE(reqResult.ok());

    auto respResult = link.transact(1, reqResult.value(), 2000ms);
    REQUIRE(respResult.ok());

    auto& resp = respResult.value();
    CHECK(!isExceptionResponse(resp));

    // Parse the response
    auto parsed = parseReadRegistersResponse(reqResult.value(), resp);
    REQUIRE(parsed.ok());
    auto regs = parsed.value();
    REQUIRE(regs.size() == 3);
    CHECK(regs[0] == 0x022B);
    CHECK(regs[1] == 0x0001);
    CHECK(regs[2] == 0x0064);

    link.close();
  }

  SUBCASE("Write Single Register via TcpLink") {
    REQUIRE(link.open().ok());

    auto reqResult = buildWriteSingleRegister(0, 0x0042);
    REQUIRE(reqResult.ok());

    auto respResult = link.transact(1, reqResult.value(), 2000ms);
    REQUIRE(respResult.ok());
    CHECK(respResult.value().functionCode == 0x06);

    // Read back to verify
    auto readReq = buildReadHoldingRegisters(0, 1);
    REQUIRE(readReq.ok());
    auto readResp = link.transact(1, readReq.value(), 2000ms);
    REQUIRE(readResp.ok());
    auto parsed = parseReadRegistersResponse(readReq.value(), readResp.value());
    REQUIRE(parsed.ok());
    CHECK(parsed.value()[0] == 0x0042);

    link.close();
  }

  SUBCASE("Transaction ID increments") {
    REQUIRE(link.open().ok());

    auto req = buildReadHoldingRegisters(0, 1);
    REQUIRE(req.ok());

    // Multiple transactions should succeed (txn id increments internally)
    for (int i = 0; i < 5; ++i) {
      auto resp = link.transact(1, req.value(), 2000ms);
      REQUIRE(resp.ok());
      CHECK(!isExceptionResponse(resp.value()));
    }

    link.close();
  }
}
