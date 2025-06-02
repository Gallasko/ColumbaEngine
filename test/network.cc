// __tests__/NetworkSystem_ipPortKey_test.cpp

#include <gtest/gtest.h>

#include "Networking/common.h"
#include "Networking/network_system.h"

using namespace pg;

TEST(netsys_common, u64_roundtrip)
{
    uint8_t buffer[8];

    uint64_t original = 0x1122334455667788ULL;
    writeU64BE(buffer, original);
    uint64_t readback = readU64BE(buffer);
    EXPECT_EQ(readback, original);
}

TEST(netsys_common, u16_u32_roundtrip)
{
    uint8_t buf16[2], buf32[4];

    uint16_t v16 = 0xABCD;
    writeU16BE(buf16, v16);
    EXPECT_EQ(readU16BE(buf16), v16);

    uint32_t v32 = 0xDEADBEEF;
    writeU32BE(buf32, v32);
    EXPECT_EQ(readU32BE(buf32), v32);
}

TEST(netsys_common, smallpayload_nofragments)
{
    // If payload size + header <= MAX_PAYLOAD, we get exactly one fragment.
    NetPayload payload = {0x01, 0x02, 0x03, 0x04};
    uint32_t clientId = 42, token = 0xCAFEBABE, packetNumber = 7;
    NetMsgType type = NetMsgType::Custom;

    auto frags = fragmentPayload(clientId, token, type, packetNumber, payload);
    ASSERT_EQ(frags.size(), 1u);

    NetPacketBuffer buffer;
    std::map<uint32_t, uint64_t> timers;
    ParsedPacket outPkt;

    // Feed the single fragment into parseAndReassemble
    bool complete = parseAndReassemble(frags[0], buffer, timers, outPkt);
    EXPECT_TRUE(complete);
    EXPECT_EQ(outPkt.header.clientId, clientId);
    EXPECT_EQ(outPkt.header.token, token);
    EXPECT_EQ(outPkt.header.packetNumber, packetNumber);
    EXPECT_EQ(outPkt.header.type, type);
    EXPECT_EQ(outPkt.payload, payload);
}

TEST(netsys_common, largepayload_multiplefragments_randomorder)
{
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

    auto frags = fragmentPayload(clientId, token, type, packetNumber, payload);
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
        complete = parseAndReassemble(raw, buffer, timers, reassembled);
    }

    EXPECT_TRUE(complete);
    EXPECT_EQ(reassembled.header.clientId, clientId);
    EXPECT_EQ(reassembled.header.token, token);
    EXPECT_EQ(reassembled.header.packetNumber, packetNumber);
    EXPECT_EQ(reassembled.header.type, type);
    EXPECT_EQ(reassembled.payload.size(), payload.size());
    EXPECT_EQ(reassembled.payload, payload);
}