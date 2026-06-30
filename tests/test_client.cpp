#include <doctest/doctest.h>
#include "modbuscpp/client.hpp"
#include "mock_link.hpp"
#include <chrono>

using namespace modbus;
using namespace modbus::test;
using namespace std::chrono_literals;

TEST_CASE("ModbusClient: readHoldingRegisters via MockLink") {
  auto mock = std::make_unique<MockLink>();

  // Queue a successful response: FC=0x03, byteCount=4, regs=[0x022B, 0x0001]
  Pdu resp;
  resp.functionCode = 0x03;
  resp.data = {0x04, 0x02, 0x2B, 0x00, 0x01};
  mock->queueResponse(resp);

  ModbusClient client(std::move(mock));
  REQUIRE(client.connect().ok());

  auto result = client.readHoldingRegisters(0, 2);
  REQUIRE(result.ok());
  auto regs = result.value();
  REQUIRE(regs.size() == 2);
  CHECK(regs[0] == 0x022B);
  CHECK(regs[1] == 0x0001);
}

TEST_CASE("ModbusClient: exception response mapping") {
  auto mock = std::make_unique<MockLink>();

  // Queue an exception: FC=0x83 (0x03|0x80), code=0x02 (Illegal Data Address)
  Pdu exResp;
  exResp.functionCode = 0x83;
  exResp.data = {0x02};
  mock->queueResponse(exResp);

  ModbusClient client(std::move(mock));
  REQUIRE(client.connect().ok());

  auto result = client.readHoldingRegisters(0, 2);
  CHECK(!result.ok());
  CHECK(result.error().category == ErrorCategory::Device);
  CHECK(result.error().code == 0x02);
  CHECK(result.error().message == "Illegal Data Address");
}

TEST_CASE("ModbusClient: retry on Transport error") {
  auto mock = std::make_unique<MockLink>();

  // First: Transport error
  mock->queueError(Error{ErrorCategory::Transport, 0, "Timeout"});
  // Second: success
  Pdu resp;
  resp.functionCode = 0x03;
  resp.data = {0x02, 0x00, 0x42};
  mock->queueResponse(resp);

  ModbusClient client(std::move(mock));
  REQUIRE(client.connect().ok());
  client.setRetries(2);

  auto result = client.readHoldingRegisters(0, 1);
  REQUIRE(result.ok());
  CHECK(result.value()[0] == 0x0042);
}

TEST_CASE("ModbusClient: no retry on Protocol/Device errors") {
  auto mock = std::make_unique<MockLink>();

  // Protocol error (should NOT retry)
  mock->queueError(Error{ErrorCategory::Protocol, 0, "CRC mismatch"});

  ModbusClient client(std::move(mock));
  REQUIRE(client.connect().ok());
  client.setRetries(3);

  auto result = client.readHoldingRegisters(0, 1);
  CHECK(!result.ok());
  CHECK(result.error().category == ErrorCategory::Protocol);
}

TEST_CASE("ModbusClient: usage error (quantity) bypasses link") {
  auto mock = std::make_unique<MockLink>();
  ModbusClient client(std::move(mock));
  REQUIRE(client.connect().ok());

  auto result = client.readHoldingRegisters(0, 200); // too many
  CHECK(!result.ok());
  CHECK(result.error().category == ErrorCategory::Usage);
}

TEST_CASE("ModbusClient: all 8 functions via MockLink") {
  auto mock = std::make_unique<MockLink>();

  // Queue responses for all operations
  Pdu readBitsResp;
  readBitsResp.functionCode = 0x01;
  readBitsResp.data = {0x01, 0xFF}; // byteCount=1, 8 coils ON
  mock->queueResponse(readBitsResp);  // readCoils

  Pdu readInputBitsResp;
  readInputBitsResp.functionCode = 0x02;
  readInputBitsResp.data = {0x01, 0x00};
  mock->queueResponse(readInputBitsResp);  // readDiscreteInputs

  Pdu readRegResp;
  readRegResp.functionCode = 0x03;
  readRegResp.data = {0x02, 0x00, 0x01};
  mock->queueResponse(readRegResp);  // readHoldingRegisters

  Pdu readIRResp;
  readIRResp.functionCode = 0x04;
  readIRResp.data = {0x02, 0x00, 0x02};
  mock->queueResponse(readIRResp);  // readInputRegisters

  Pdu writeCoilResp;
  writeCoilResp.functionCode = 0x05;
  writeCoilResp.data = {0x00, 0x00, 0xFF, 0x00};
  mock->queueResponse(writeCoilResp);  // writeSingleCoil

  Pdu writeRegResp;
  writeRegResp.functionCode = 0x06;
  writeRegResp.data = {0x00, 0x00, 0x00, 0x42};
  mock->queueResponse(writeRegResp);  // writeSingleRegister

  Pdu writeMultiCoilResp;
  writeMultiCoilResp.functionCode = 0x0F;
  writeMultiCoilResp.data = {0x00, 0x00, 0x00, 0x0A};
  mock->queueResponse(writeMultiCoilResp);  // writeMultipleCoils

  Pdu writeMultiRegResp;
  writeMultiRegResp.functionCode = 0x10;
  writeMultiRegResp.data = {0x00, 0x00, 0x00, 0x01};
  mock->queueResponse(writeMultiRegResp);  // writeMultipleRegisters

  ModbusClient client(std::move(mock));
  REQUIRE(client.connect().ok());

  CHECK(client.readCoils(0, 8).ok());
  CHECK(client.readDiscreteInputs(0, 8).ok());
  CHECK(client.readHoldingRegisters(0, 1).ok());
  CHECK(client.readInputRegisters(0, 1).ok());
  CHECK(client.writeSingleCoil(0, true).ok());
  CHECK(client.writeSingleRegister(0, 0x0042).ok());
  CHECK(client.writeMultipleCoils(0, std::vector<bool>(10, true)).ok());
  CHECK(client.writeMultipleRegisters(0, {0x0001}).ok());
}
