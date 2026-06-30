/// modbus_bench — Modbus library benchmark harness
/// Measures latency (p50/p95/p99) and throughput (tx/sec).
#include "modbuscpp/client.hpp"
#include "modbuscpp/tcp_link.hpp"
#include "modbuscpp/rtu_link.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <thread>

using namespace modbus;
using namespace std::chrono;
using namespace std::chrono_literals;

static void printStats(std::vector<double>& usLatencies, size_t count, double elapsedSec) {
  std::sort(usLatencies.begin(), usLatencies.end());
  double p50 = usLatencies[usLatencies.size() * 50 / 100];
  double p95 = usLatencies[usLatencies.size() * 95 / 100];
  double p99 = usLatencies[usLatencies.size() * 99 / 100];
  double avg = std::accumulate(usLatencies.begin(), usLatencies.end(), 0.0) / usLatencies.size();

  std::cout << std::fixed << std::setprecision(1);
  std::cout << "  Count:     " << count << "\n";
  std::cout << "  Elapsed:   " << elapsedSec << " s\n";
  std::cout << "  Throughput: " << (count / elapsedSec) << " tx/s\n";
  std::cout << "  Latency (us):\n";
  std::cout << "    avg: " << avg << "\n";
  std::cout << "    p50: " << p50 << "\n";
  std::cout << "    p95: " << p95 << "\n";
  std::cout << "    p99: " << p99 << "\n\n";
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: modbus_bench tcp HOST PORT [COUNT]\n";
    std::cerr << "       modbus_bench rtu DEV BAUD [COUNT]\n";
    return 1;
  }

  std::string mode = argv[1];
  std::unique_ptr<ModbusClient> client;
  int count = (argc >= 4) ? std::atoi(argv[argc - 1]) : 1000;

  if (mode == "tcp") {
    if (argc < 4) { std::cerr << "Usage: modbus_bench tcp HOST PORT [COUNT]\n"; return 1; }
    std::string host = argv[2];
    uint16_t port = static_cast<uint16_t>(std::atoi(argv[3]));
    if (argc >= 5) count = std::atoi(argv[4]);
    client = std::make_unique<ModbusClient>(
      std::make_unique<TcpLink>(TcpConfig{host, port, 3000ms, true}));
  }
  else if (mode == "rtu") {
    if (argc < 4) { std::cerr << "Usage: modbus_bench rtu DEV BAUD [COUNT]\n"; return 1; }
    RtuConfig cfg;
    cfg.device = argv[2];
    cfg.baud = static_cast<uint32_t>(std::atoi(argv[3]));
    if (argc >= 5) count = std::atoi(argv[4]);
    client = std::make_unique<ModbusClient>(std::make_unique<RtuLink>(cfg));
  }
  else {
    std::cerr << "Unknown mode: " << mode << "\n";
    return 1;
  }

  auto err = client->connect();
  if (!err.ok()) { std::cerr << "Connect error: " << err.message << "\n"; return 1; }

  std::cout << "Running benchmark: " << count << " iterations...\n";

  std::vector<double> usLatencies;
  usLatencies.reserve(count);

  auto start = high_resolution_clock::now();

  for (int i = 0; i < count; ++i) {
    auto t0 = high_resolution_clock::now();
    auto r = client->readHoldingRegisters(0, 1);
    auto t1 = high_resolution_clock::now();

    if (!r.ok()) {
      std::cerr << "Error at iteration " << i << ": " << r.error().message << "\n";
      return 1;
    }

    double us = duration_cast<microseconds>(t1 - t0).count();
    usLatencies.push_back(us);
  }

  auto end = high_resolution_clock::now();
  double elapsedSec = duration_cast<microseconds>(end - start).count() / 1'000'000.0;

  printStats(usLatencies, count, elapsedSec);
  client->disconnect();
  return 0;
}
