#include <doctest/doctest.h>
#include "modbuscpp/rtu_link.hpp"
#include "modbuscpp/crc.hpp"
#include "modbuscpp/pdu.hpp"
#include <chrono>

using namespace modbus;
using namespace std::chrono_literals;

TEST_CASE("RtuLink: t3.5 computation") {
  // 9600 baud, 8N1: bitsPerChar = 1+8+1 = 10
  // charTime = 10/9600 = 1.041667 ms
  // t3.5 = 3.5 * 1.041667 = 3.6458 ms → rounded to 4ms
  RtuConfig cfg;
  cfg.baud = 9600;
  cfg.dataBits = 8;
  cfg.parity = Parity::None;
  cfg.stopBits = 1;
  RtuLink link(cfg);
  auto t35 = link.t35Interval();
  CHECK(t35.count() >= 1);
  // For 9600 8N1: 3.5 * (10/9600) * 1000 = 3.6458ms → ~4ms
  CHECK(t35.count() <= 10); // sanity check
}

TEST_CASE("RtuLink: t3.5 higher baud") {
  // 115200 8N1: bitsPerChar = 10, charTime = 10/115200 = 0.0868ms
  // t3.5 = 3.5 * 0.0868 = 0.3038ms → rounded to 0 → clamped to 1ms
  RtuConfig cfg;
  cfg.baud = 115200;
  RtuLink link(cfg);
  auto t35 = link.t35Interval();
  // Clamped to minimum 1ms
  CHECK(t35.count() >= 1);
}

TEST_CASE("RtuLink: CRC roundtrip on RTU frame") {
  // Build a sample RTU frame: addr=1, FC=0x03, data=[0x00,0x00,0x00,0x03]
  // Total without CRC: 1+1+4 = 6 bytes
  std::vector<uint8_t> frame = {0x01, 0x03, 0x00, 0x00, 0x00, 0x03};
  uint16_t crc = crc16(frame.data(), frame.size());

  // Append CRC: low byte first (Modbus RTU convention)
  frame.push_back(static_cast<uint8_t>(crc & 0xFF));
  frame.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
  REQUIRE(frame.size() == 8);

  // Verify CRC on entire frame (all 8 bytes) → since CRC appends, crc16(all) should be 0
  uint16_t verify = crc16(frame.data(), frame.size());
  CHECK(verify == 0x0000);
}

TEST_CASE("RtuLink: CRC known vector") {
  // Known RTU frame: 01 03 00 00 00 01 → CRC = 84 0A
  std::vector<uint8_t> frame = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01};
  uint16_t crc = crc16(frame.data(), frame.size());
  // CRC should be 0x0A84 → low=0x84, high=0x0A → as uint16_t = 0x0A84
  CHECK(crc == 0x0A84);
}

TEST_CASE("RtuLink: exception response CRC") {
  // Exception: 01 83 02 → verify roundtrip
  std::vector<uint8_t> frame = {0x01, 0x83, 0x02};
  uint16_t crc = crc16(frame.data(), frame.size());

  // Append CRC: low byte first
  frame.push_back(static_cast<uint8_t>(crc & 0xFF));         // CRC low
  frame.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));  // CRC high

  // Full frame CRC should be 0 (CRC property)
  CHECK(crc16(frame.data(), frame.size()) == 0x0000);
}
