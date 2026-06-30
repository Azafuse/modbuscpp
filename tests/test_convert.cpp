#include <doctest/doctest.h>
#include "modbuscpp/convert.hpp"
#include <cmath>
#include <cstring>

using namespace modbus;

// Helper: check float equality with tolerance
static bool floatNear(float a, float b) { return std::fabs(a - b) < 1e-6f; }
static bool doubleNear(double a, double b) { return std::fabs(a - b) < 1e-12; }

TEST_CASE("Convert: int32 roundtrip (all 4 word/byte order combos)") {
  int32_t original = -1234567;

  for (auto wo : {WordOrder::HighWordFirst, WordOrder::LowWordFirst}) {
    for (auto bo : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
      auto [r0, r1] = int32ToRegs(original, wo, bo);
      int32_t back = regsToInt32(r0, r1, wo, bo);
      CHECK(back == original);
    }
  }
}

TEST_CASE("Convert: uint32 roundtrip") {
  uint32_t original = 0xDEADBEEF;
  auto [r0, r1] = uint32ToRegs(original);
  uint32_t back = regsToUint32(r0, r1);
  CHECK(back == original);
}

TEST_CASE("Convert: float32 roundtrip") {
  float original = 3.14159f;
  auto [r0, r1] = float32ToRegs(original);
  float back = regsToFloat32(r0, r1);
  CHECK(floatNear(original, back));
}

TEST_CASE("Convert: float64 roundtrip") {
  double original = 3.14159265358979;
  auto regs = float64ToRegs(original);
  REQUIRE(regs.size() == 4);
  double back = regsToFloat64(regs[0], regs[1], regs[2], regs[3]);
  CHECK(doubleNear(original, back));
}

TEST_CASE("Convert: int32 known values BigEndian/HighWordFirst") {
  // Default: HighWordFirst, BigEndian
  auto [r0, r1] = int32ToRegs(0x12345678);
  // 0x12345678 → bytes: 12 34 56 78
  // HighWordFirst, BigEndian: reg0 = 0x1234, reg1 = 0x5678
  CHECK(r0 == 0x1234);
  CHECK(r1 == 0x5678);
  CHECK(regsToInt32(r0, r1) == 0x12345678);
}

TEST_CASE("Convert: int32 LowWordFirst/BigEndian") {
  auto wo = WordOrder::LowWordFirst;
  auto bo = ByteOrder::BigEndian;
  auto [r0, r1] = int32ToRegs(0x12345678, wo, bo);
  // LowWordFirst: reg0 = low word = 0x5678, reg1 = high word = 0x1234
  CHECK(r0 == 0x5678);
  CHECK(r1 == 0x1234);
  CHECK(regsToInt32(r0, r1, wo, bo) == 0x12345678);
}

TEST_CASE("Convert: int32 HighWordFirst/LittleEndian") {
  auto wo = WordOrder::HighWordFirst;
  auto bo = ByteOrder::LittleEndian;
  auto [r0, r1] = int32ToRegs(0x12345678, wo, bo);
  // reg0 bytes (LE): 0x34, 0x12 → uint16_t LE = 0x3412
  // reg1 bytes (LE): 0x78, 0x56 → uint16_t LE = 0x7856
  CHECK(r0 == 0x3412);
  CHECK(r1 == 0x7856);
  CHECK(regsToInt32(r0, r1, wo, bo) == 0x12345678);
}

TEST_CASE("Convert: string roundtrip") {
  std::string original = "Hello Modbus!";
  auto regs = stringToRegs(original);
  std::string back = regsToString(regs);
  // Trim potential null padding
  while (!back.empty() && back.back() == '\0') back.pop_back();
  CHECK(back == original);
}

TEST_CASE("Convert: string odd length") {
  std::string original = "ABC"; // 3 chars → 2 registers, second padded
  auto regs = stringToRegs(original);
  REQUIRE(regs.size() == 2);
  CHECK(regs[0] == (('A' << 8) | 'B'));
  CHECK(regs[1] == static_cast<uint16_t>('C' << 8)); // padded with 0
}

TEST_CASE("Convert: float32 known IEEE 754") {
  // 1.0f in IEEE 754 big-endian = 0x3F800000
  // HighWordFirst, BigEndian: reg0 = 0x3F80, reg1 = 0x0000
  auto [r0, r1] = float32ToRegs(1.0f);
  CHECK(r0 == 0x3F80);
  CHECK(r1 == 0x0000);
  CHECK(floatNear(regsToFloat32(r0, r1), 1.0f));
}
