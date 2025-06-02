// __tests__/NetworkSystem_ipPortKey_test.cpp

#include <gtest/gtest.h>

#include "mocknetworking.h"

#include "Networking/common.h"
#include "Networking/network_system.h"

using namespace pg;

class NetworkSystemTest : public ::testing::Test
{
protected:
    MockBackend*      backendPtr;
    std::unique_ptr<NetworkSystem> netSys;

    void SetUp() override
    {
        backendPtr = new MockBackend();

        // Suppose your NetworkSystem constructor is:
        //    NetworkSystem(INetworkBackend* backend, const NetworkConfig& cfg);
        NetworkConfig cfg;
        // … fill cfg as needed for tests …

        netSys.reset(new NetworkSystem(backendPtr, cfg));
    }

    void TearDown() override
    {
        // NetworkSystem’s destructor will delete backendPtr,
        // so we don’t delete backendPtr here.
        netSys.reset();
    }
};

TEST(netsys_common, u64_roundtrip)
{
    MockBackend backend;

    uint8_t buffer[8];

    uint64_t original = 0x1122334455667788ULL;
    backend.writeU64BE(buffer, original);
    uint64_t readback = backend.readU64BE(buffer);
    EXPECT_EQ(readback, original);
}

TEST(netsys_common, u16_u32_roundtrip)
{
    MockBackend backend;
    uint8_t buf16[2], buf32[4];

    uint16_t v16 = 0xABCD;
    backend.writeU16BE(buf16, v16);
    EXPECT_EQ(backend.readU16BE(buf16), v16);

    uint32_t v32 = 0xDEADBEEF;
    backend.writeU32BE(buf32, v32);
    EXPECT_EQ(backend.readU32BE(buf32), v32);
}

TEST(netsys_common, smallpayload_nofragments)
{
    MockBackend backend;

    // If payload size + header <= MAX_PAYLOAD, we get exactly one fragment.
    NetPayload payload = {0x01, 0x02, 0x03, 0x04};
    uint32_t clientId = 42, token = 0xCAFEBABE, packetNumber = 7;
    NetMsgType type = NetMsgType::Custom;

    auto frags = fragmentPayload(clientId, token, type, packetNumber, &backend, payload);
    ASSERT_EQ(frags.size(), 1u);

    NetPacketBuffer buffer;
    std::map<uint32_t, uint64_t> timers;
    ParsedPacket outPkt;

    // Feed the single fragment into parseAndReassemble
    bool complete = parseAndReassemble(frags[0], buffer, timers, &backend, outPkt);
    EXPECT_TRUE(complete);
    EXPECT_EQ(outPkt.header.clientId, clientId);
    EXPECT_EQ(outPkt.header.token, token);
    EXPECT_EQ(outPkt.header.packetNumber, packetNumber);
    EXPECT_EQ(outPkt.header.type, type);
    EXPECT_EQ(outPkt.payload, payload);
}

TEST(netsys_common, largepayload_multiplefragments_randomorder)
{
    MockBackend backend;

    // Construct a payload slightly larger than chunkMax so that it splits.
    // chunkMax = MAX_PAYLOAD - HEADER_SIZE.
    size_t chunkMax = MAX_PAYLOAD - HEADER_SIZE;
    // Make payload length = chunkMax * 2 + 10 → 3 fragments total.
    NetPayload payload(chunkMax * 2 + 10);
    // Fill with some pattern.
    for (size_t i = 0; i < payload.size(); i++)
    {
        payload[i] = static_cast<uint8_t>(i & 0xFF);
    }

    uint32_t clientId = 99, token = 0xFEEDFACE, packetNumber = 1234;
    NetMsgType type = NetMsgType::Custom;

    auto frags = fragmentPayload(clientId, token, type, packetNumber, &backend, payload);
    ASSERT_EQ(frags.size(), 3u);

    // Shuffle them so reassembly must handle out‐of‐order.
    std::vector<NetPayload> rawFrags;
    for (auto& f : frags)
        rawFrags.push_back(f);
    std::random_shuffle(rawFrags.begin(), rawFrags.end());

    NetPacketBuffer buffer;
    std::map<uint32_t, uint64_t> timers;
    ParsedPacket reassembled;

    bool complete = false;
    for (auto& raw : rawFrags)
    {
        complete = parseAndReassemble(raw, buffer, timers, &backend, reassembled);
    }

    EXPECT_TRUE(complete);
    EXPECT_EQ(reassembled.header.clientId, clientId);
    EXPECT_EQ(reassembled.header.token, token);
    EXPECT_EQ(reassembled.header.packetNumber, packetNumber);
    EXPECT_EQ(reassembled.header.type, type);
    EXPECT_EQ(reassembled.payload.size(), payload.size());
    EXPECT_EQ(reassembled.payload, payload);
}