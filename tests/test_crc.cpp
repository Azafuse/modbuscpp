#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "modbuscpp/crc.hpp"
#include <vector>

using namespace modbus;

// Helper: run crc16 on a vector and return result
static uint16_t crcOf(const std::vector<uint8_t>& data) {
  return crc16(data.data(), data.size());
}

TEST_CASE("CRC-16 known vectors") {
  // Test vector 1: The classic example from Modbus spec
  // Frame: 01 04 02 FF FF  →  CRC should be B8 80 (low byte 0x80, high byte 0xB8)
  // So crc16 of {0x01, 0x04, 0x02, 0xFF, 0xFF} should be 0xB880 (in host byte order, which
  // is little-endian on x86, meaning the uint16_t value is 0xB880)
  SUBCASE("Vector 1: 01 04 02 FF FF") {
    std::vector<uint8_t> data = {0x01, 0x04, 0x02, 0xFF, 0xFF};
    uint16_t crc = crcOf(data);
    // Wire CRC bytes: 0x80 0xB8 (C5 CD in Modbus doc).
    // crc16 returns the value where low byte = first wire byte.
    // So crc & 0xFF = 0x80, (crc >> 8) = 0xB8 → uint16_t = 0x80B8
    CHECK(crc == 0x80B8);
  }

  // Test vector 2: Read Holding Registers request
  // Frame: 01 03 00 00 00 0A  →  CRC wire bytes: C5 CD
  // Low byte 0xC5, high byte 0xCD → uint16_t value 0xCDC5
  SUBCASE("Vector 2: 01 03 00 00 00 0A") {
    std::vector<uint8_t> data = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    uint16_t crc = crcOf(data);
    CHECK(crc == 0xCDC5);
  }

  // Test vector 3: Write Single Register
  // Frame: 01 06 00 01 00 03  →  CRC wire bytes: 98 0B
  SUBCASE("Vector 3: 01 06 00 01 00 03") {
    std::vector<uint8_t> data = {0x01, 0x06, 0x00, 0x01, 0x00, 0x03};
    uint16_t crc = crcOf(data);
    CHECK(crc == 0x0B98);
  }

  // Test vector 4: Single byte (edge case)
  SUBCASE("Vector 4: Single byte 0x01") {
    std::vector<uint8_t> data = {0x01};
    uint16_t crc = crcOf(data);
    CHECK(crc == 0x807E);
  }

  // Test vector 5: Empty data
  SUBCASE("Vector 5: Empty data") {
    std::vector<uint8_t> data = {};
    uint16_t crc = crcOf(data);
    CHECK(crc == 0xFFFF);
  }

  // Test vector 6: All zeros
  SUBCASE("Vector 6: All zeros (4 bytes)") {
    std::vector<uint8_t> data = {0x00, 0x00, 0x00, 0x00};
    uint16_t crc = crcOf(data);
    CHECK(crc == 0x2400);
  }
}

TEST_CASE("CRC-16 consistency") {
  // If we append the CRC (low byte first) to a message, the CRC over the
  // entire frame (message + CRC) should be 0x0000.
  SUBCASE("CRC check property") {
    std::vector<uint8_t> msg = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    uint16_t crc = crcOf(msg);

    // Append CRC low byte first
    std::vector<uint8_t> frame = msg;
    frame.push_back(static_cast<uint8_t>(crc & 0xFF));        // low byte
    frame.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF)); // high byte

    uint16_t check = crcOf(frame);
    CHECK(check == 0x0000);
  }
}
