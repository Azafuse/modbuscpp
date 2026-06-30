/// modbus_tool — Interactive Modbus CLI REPL (fallback from Qt per §17)
#include "modbuscpp/client.hpp"
#include "modbuscpp/tcp_link.hpp"
#include "modbuscpp/rtu_link.hpp"
#include "modbuscpp/convert.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

using namespace modbus;
using namespace std::chrono_literals;

int main() {
  std::cout << "modbus_tool — Modbus CLI REPL\n";
  std::cout << "Commands: quit, tcp HOST PORT, rtu DEV BAUD,\n";
  std::cout << "  read (fc|hr|ir|co|di) ADDR QTY,\n";
  std::cout << "  write sr ADDR VAL(hex), write sc ADDR ON|OFF,\n";
  std::cout << "  poll (hr|co) ADDR QTY MS, raw on|off, stop\n\n";

  std::unique_ptr<ModbusClient> client;
  std::atomic<bool> polling{false};
  std::thread pollThread;
  std::string line;

  for (;;) {
    std::cout << "> " << std::flush;
    if (!std::getline(std::cin, line)) break;
    if (line.empty()) continue;

    std::istringstream iss(line);
    std::string cmd; iss >> cmd;

    if (cmd == "quit" || cmd == "exit") {
      polling = false; if (pollThread.joinable()) pollThread.join();
      break;
    }

    // TCP connect
    if (cmd == "tcp") {
      std::string host; uint16_t port;
      if (!(iss >> host >> port)) { std::cout << "usage: tcp HOST PORT\n"; continue; }
      polling = false; if (pollThread.joinable()) pollThread.join();
      client = std::make_unique<ModbusClient>(
        std::make_unique<TcpLink>(TcpConfig{host, port, 3000ms, true}));
      auto err = client->connect();
      if (err.ok()) std::cout << "Connected " << host << ":" << port << "\n";
      else std::cout << "Error: " << err.message << "\n";
      continue;
    }

    // RTU connect
    if (cmd == "rtu") {
      std::string dev; uint32_t baud;
      if (!(iss >> dev >> baud)) { std::cout << "usage: rtu DEV BAUD\n"; continue; }
      polling = false; if (pollThread.joinable()) pollThread.join();
      RtuConfig cfg; cfg.device = dev; cfg.baud = baud;
      client = std::make_unique<ModbusClient>(std::make_unique<RtuLink>(cfg));
      auto err = client->connect();
      if (err.ok()) std::cout << "Connected " << dev << " @" << baud << "\n";
      else std::cout << "Error: " << err.message << "\n";
      continue;
    }

    if (!client || !client->connected()) {
      std::cout << "Not connected. Use: tcp HOST PORT  or  rtu DEV BAUD\n";
      continue;
    }

    // read
    if (cmd == "read") {
      std::string type; uint16_t addr, qty;
      if (!(iss >> type >> addr >> qty)) { std::cout << "usage: read (hr|ir|co|di) ADDR QTY\n"; continue; }
      if (type == "hr") {
        auto r = client->readHoldingRegisters(addr, qty);
        if (r.ok()) { std::cout << "HR:"; for (auto v : r.value()) std::cout << ' ' << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << v; std::cout << std::dec << "\n"; }
        else std::cout << "Error: " << r.error().message << "\n";
      } else if (type == "ir") {
        auto r = client->readInputRegisters(addr, qty);
        if (r.ok()) { std::cout << "IR:"; for (auto v : r.value()) std::cout << ' ' << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << v; std::cout << std::dec << "\n"; }
        else std::cout << "Error: " << r.error().message << "\n";
      } else if (type == "co") {
        auto r = client->readCoils(addr, qty);
        if (r.ok()) { std::cout << "CO:"; for (auto b : r.value()) std::cout << ' ' << (b?'1':'0'); std::cout << "\n"; }
        else std::cout << "Error: " << r.error().message << "\n";
      } else if (type == "di") {
        auto r = client->readDiscreteInputs(addr, qty);
        if (r.ok()) { std::cout << "DI:"; for (auto b : r.value()) std::cout << ' ' << (b?'1':'0'); std::cout << "\n"; }
        else std::cout << "Error: " << r.error().message << "\n";
      } else std::cout << "Unknown type: " << type << " (use hr/ir/co/di)\n";
      continue;
    }

    // write
    if (cmd == "write") {
      std::string type; iss >> type;
      if (type == "sr") { uint16_t addr, val; if (iss >> addr >> std::hex >> val) { auto e = client->writeSingleRegister(addr, val); std::cout << (e.ok() ? "OK\n" : "Error: " + e.message + "\n"); } }
      else if (type == "sc") { uint16_t addr; std::string on; if (iss >> addr >> on) { auto e = client->writeSingleCoil(addr, on=="ON"||on=="on"); std::cout << (e.ok() ? "OK\n" : "Error: " + e.message + "\n"); } }
      else std::cout << "usage: write sr ADDR VAL(hex)  or  write sc ADDR ON|OFF\n";
      continue;
    }

    // poll
    if (cmd == "poll") {
      std::string type; uint16_t addr, qty, ms;
      if (!(iss >> type >> addr >> qty >> ms)) { std::cout << "usage: poll (hr|co) ADDR QTY MS\n"; continue; }
      polling = false; if (pollThread.joinable()) pollThread.join();
      polling = true;
      pollThread = std::thread([&client, &polling, type, addr, qty, ms]() {
        while (polling) { auto t0 = std::chrono::steady_clock::now();
          if (type == "hr") { auto r = client->readHoldingRegisters(addr, qty); if (r.ok()) std::cout << "\rHR[" << addr << "]=" << std::hex << r.value()[0] << std::dec << "   " << std::flush; }
          else if (type == "co") { auto r = client->readCoils(addr, qty); if (r.ok()) std::cout << "\rCO[" << addr << "]=" << (r.value()[0]?'1':'0') << "   " << std::flush; }
          auto dt = std::chrono::steady_clock::now() - t0;
          auto st = std::chrono::milliseconds(ms) - dt;
          if (st.count() > 0) std::this_thread::sleep_for(st);
        } });
      std::cout << "Polling started. Type 'stop' to end.\n";
      continue;
    }

    if (cmd == "stop") { polling = false; if (pollThread.joinable()) { pollThread.join(); std::cout << "Polling stopped.\n"; } continue; }
    std::cout << "Unknown: " << cmd << "\n";
  }

  polling = false; if (pollThread.joinable()) pollThread.join();
  if (client) client->disconnect();
  return 0;
}