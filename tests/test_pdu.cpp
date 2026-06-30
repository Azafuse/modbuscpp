#include <doctest/doctest.h>
#include "modbuscpp/pdu.hpp"

using namespace modbus;

TEST_CASE("PDU build: Read Coils (FC 0x01)") {
  auto result = buildReadCoils(0, 10);
  REQUIRE(result.ok());
  auto& pdu = result.value();
  CHECK(pdu.functionCode == 0x01);
  REQUIRE(pdu.data.size() == 4);
  CHECK(pdu.data[0] == 0x00);
  CHECK(pdu.data[1] == 0x00);
  CHECK(pdu.data[2] == 0x00);
  CHECK(pdu.data[3] == 0x0A);
}

TEST_CASE("PDU build: Read Coils quantity limits") {
  auto r1 = buildReadCoils(0, 0);
  CHECK(!r1.ok());
  CHECK(r1.error().category == ErrorCategory::Usage);
  auto r2 = buildReadCoils(0, 2001);
  CHECK(!r2.ok());
  auto r3 = buildReadCoils(0, 2000);
  CHECK(r3.ok());
}

TEST_CASE("PDU build: Read Discrete Inputs (FC 0x02)") {
  auto result = buildReadDiscreteInputs(100, 50);
  REQUIRE(result.ok());
  auto& pdu = result.value();
  CHECK(pdu.functionCode == 0x02);
  CHECK(pdu.data[0] == 0x00);
  CHECK(pdu.data[1] == 0x64);
  CHECK(pdu.data[2] == 0x00);
  CHECK(pdu.data[3] == 0x32);
}

TEST_CASE("PDU build: Read Holding Registers (FC 0x03)") {
  auto result = buildReadHoldingRegisters(0, 2);
  REQUIRE(result.ok());
  auto& pdu = result.value();
  CHECK(pdu.functionCode == 0x03);
  CHECK(pdu.data[2] == 0x00);
  CHECK(pdu.data[3] == 0x02);
}

TEST_CASE("PDU build: Registers quantity limits") {
  CHECK(!buildReadHoldingRegisters(0, 0).ok());
  CHECK(!buildReadHoldingRegisters(0, 126).ok());
  CHECK(buildReadHoldingRegisters(0, 125).ok());
}

TEST_CASE("PDU build: Write Single Coil (FC 0x05)") {
  SUBCASE("Coil ON") {
    auto result = buildWriteSingleCoil(0x00A0, true);
    REQUIRE(result.ok());
    auto& pdu = result.value();
    CHECK(pdu.functionCode == 0x05);
    CHECK(pdu.data[0] == 0x00);
    CHECK(pdu.data[1] == 0xA0);
    CHECK(pdu.data[2] == 0xFF);
    CHECK(pdu.data[3] == 0x00);
  }
  SUBCASE("Coil OFF") {
    auto result = buildWriteSingleCoil(0x0001, false);
    REQUIRE(result.ok());
    auto& pdu = result.value();
    CHECK(pdu.data[2] == 0x00);
    CHECK(pdu.data[3] == 0x00);
  }
}

TEST_CASE("PDU build: Write Single Register (FC 0x06)") {
  auto result = buildWriteSingleRegister(0x0001, 0x0003);
  REQUIRE(result.ok());
  auto& pdu = result.value();
  CHECK(pdu.functionCode == 0x06);
  CHECK(pdu.data[0] == 0x00);
  CHECK(pdu.data[1] == 0x01);
  CHECK(pdu.data[2] == 0x00);
  CHECK(pdu.data[3] == 0x03);
}

TEST_CASE("PDU build: Write Multiple Coils (FC 0x0F)") {
  std::vector<bool> coils(10, true);
  auto result = buildWriteMultipleCoils(0, coils);
  REQUIRE(result.ok());
  auto& pdu = result.value();
  CHECK(pdu.functionCode == 0x0F);
  CHECK(pdu.data[3] == 0x0A);
  CHECK(pdu.data[4] == 0x02);
  CHECK(pdu.data[5] == 0xFF);
  CHECK(pdu.data[6] == 0x03);
}

TEST_CASE("PDU build: Write Multiple Coils mixed pattern") {
  std::vector<bool> coils = {true, false, true, false, true, false, true, false, true};
  auto result = buildWriteMultipleCoils(0, coils);
  REQUIRE(result.ok());
  auto& pdu = result.value();
  CHECK(pdu.data[5] == 0x55);
  CHECK(pdu.data[6] == 0x01);
}

TEST_CASE("PDU build: Write Multiple Registers (FC 0x10)") {
  std::vector<uint16_t> regs = {0x022B, 0x0001, 0x0064};
  auto result = buildWriteMultipleRegisters(0x0001, regs);
  REQUIRE(result.ok());
  auto& pdu = result.value();
  CHECK(pdu.functionCode == 0x10);
  CHECK(pdu.data[0] == 0x00);
  CHECK(pdu.data[1] == 0x01);
  CHECK(pdu.data[3] == 0x03);
  CHECK(pdu.data[4] == 0x06);
}

// -------------------------------------------------------------------------
// Parse tests
// -------------------------------------------------------------------------

TEST_CASE("PDU parse: Read Holding Registers response") {
  auto reqResult = buildReadHoldingRegisters(0, 2);
  REQUIRE(reqResult.ok());
  Pdu resp;
  resp.functionCode = 0x03;
  resp.data = {0x04, 0x02, 0x2B, 0x00, 0x64};
  auto result = parseReadRegistersResponse(reqResult.value(), resp);
  REQUIRE(result.ok());
  auto regs = result.value();
  REQUIRE(regs.size() == 2);
  CHECK(regs[0] == 0x022B);
  CHECK(regs[1] == 0x0064);
}

TEST_CASE("PDU parse: Read Coils response") {
  auto reqResult = buildReadCoils(0, 10);
  REQUIRE(reqResult.ok());
  Pdu resp;
  resp.functionCode = 0x01;
  resp.data = {0x02, 0xFF, 0x03};
  auto result = parseReadBitsResponse(reqResult.value(), resp);
  REQUIRE(result.ok());
  auto bits = result.value();
  REQUIRE(bits.size() == 16);
  for (int i = 0; i < 10; ++i) CHECK(bits[i] == true);
}

TEST_CASE("PDU parse: Write Single Register response") {
  auto reqResult = buildWriteSingleRegister(0x0001, 0x0003);
  REQUIRE(reqResult.ok());
  Pdu resp;
  resp.functionCode = 0x06;
  resp.data = {0x00, 0x01, 0x00, 0x03};
  auto result = parseWriteSingleResponse(reqResult.value(), resp);
  REQUIRE(result.ok());
  auto [addr, value] = result.value();
  CHECK(addr == 0x0001);
  CHECK(value == 0x0003);
}

TEST_CASE("PDU parse: Write Multiple Registers response") {
  auto reqResult = buildWriteMultipleRegisters(1, {0x022B, 0x0001, 0x0064});
  REQUIRE(reqResult.ok());
  Pdu resp;
  resp.functionCode = 0x10;
  resp.data = {0x00, 0x01, 0x00, 0x03};
  auto result = parseWriteMultipleRegistersResponse(reqResult.value(), resp);
  REQUIRE(result.ok());
  auto [start, qty] = result.value();
  CHECK(start == 0x0001);
  CHECK(qty == 0x0003);
}

TEST_CASE("PDU parse: Write Multiple Coils response") {
  auto reqResult = buildWriteMultipleCoils(0, std::vector<bool>(10, true));
  REQUIRE(reqResult.ok());
  Pdu resp;
  resp.functionCode = 0x0F;
  resp.data = {0x00, 0x00, 0x00, 0x0A};
  auto result = parseWriteMultipleCoilsResponse(reqResult.value(), resp);
  REQUIRE(result.ok());
  auto [start, qty] = result.value();
  CHECK(start == 0);
  CHECK(qty == 10);
}

TEST_CASE("PDU parse: protocol error on malformed response") {
  auto reqResult = buildReadHoldingRegisters(0, 2);
  REQUIRE(reqResult.ok());
  auto& req = reqResult.value();

  Pdu resp;
  resp.functionCode = 0x03;
  resp.data = {};
  auto r1 = parseReadRegistersResponse(req, resp);
  CHECK(!r1.ok());
  CHECK(r1.error().category == ErrorCategory::Protocol);

  resp.data = {0x06, 0x00, 0x01};
  auto r2 = parseReadRegistersResponse(req, resp);
  CHECK(!r2.ok());
  CHECK(r2.error().category == ErrorCategory::Protocol);
}

// -------------------------------------------------------------------------
// Exception tests
// -------------------------------------------------------------------------

TEST_CASE("PDU exception: detection") {
  Pdu pdu;
  pdu.functionCode = 0x80; CHECK(isExceptionResponse(pdu));
  pdu.functionCode = 0x83; CHECK(isExceptionResponse(pdu));
  pdu.functionCode = 0x03; CHECK(!isExceptionResponse(pdu));
  pdu.functionCode = 0x10; CHECK(!isExceptionResponse(pdu));
}

TEST_CASE("PDU exception: parsing") {
  Pdu pdu;
  pdu.functionCode = 0x81;
  pdu.data = {0x01};
  Error err = parseException(pdu);
  CHECK(err.category == ErrorCategory::Device);
  CHECK(err.code == 0x01);
  CHECK(err.message == "Illegal Function");
}

TEST_CASE("PDU exception: all codes") {
  Pdu pdu;
  pdu.functionCode = 0x86;
  pdu.data = {0x04};
  Error err = parseException(pdu);
  CHECK(err.code == 0x04);

  pdu.data[0] = 0x0B;
  err = parseException(pdu);
  CHECK(err.code == 0x0B);
  CHECK(err.message == "Gateway Target No Response");
}

TEST_CASE("PDU: all 8 function codes build correctly") {
  CHECK(buildReadCoils(0, 1).ok());
  CHECK(buildReadDiscreteInputs(0, 1).ok());
  CHECK(buildReadHoldingRegisters(0, 1).ok());
  CHECK(buildReadInputRegisters(0, 1).ok());
  CHECK(buildWriteSingleCoil(0, true).ok());
  CHECK(buildWriteSingleRegister(0, 0).ok());
  CHECK(buildWriteMultipleCoils(0, {true}).ok());
  CHECK(buildWriteMultipleRegisters(0, {0x0001}).ok());
}