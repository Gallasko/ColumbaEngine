// test_networksystem_full.cpp
#include <gtest/gtest.h>

#include "Networking/network_system.h"
#include <tuple>
#include <queue>

using namespace pg;

//-----------------------------------------------------------------------------
// MockNetworkBackend: as before, but exposes control for tests
//-----------------------------------------------------------------------------
class MockNetworkBackend : public INetworkBackend
{
public:
    // Inbox entries: (tcpSock, udpSrc, data)
    std::queue<std::tuple<TCPsocket, IPaddress, std::vector<uint8_t>>> inbox;

    struct Out {
        bool isTcp;
        TCPsocket sock;
        IPaddress addr;
        std::vector<uint8_t> data;
    };
    std::vector<Out> outbox;

    TCPsocket fakeListener = reinterpret_cast<TCPsocket>(0x1);

    bool connectSucceeds = true;

    // INetworkBackend impl:
    TCPsocket acceptTcpClient() override {
        if (!inbox.empty()) {
            auto [sock, addr, dat] = inbox.front();
            if (sock == fakeListener && dat.empty()) {
                inbox.pop();
                return reinterpret_cast<TCPsocket>(0x42);
            }
        }
        return nullptr;
    }
    bool connectToServer() override { return connectSucceeds; }

    bool sendUdp(const IPaddress& dest, const std::vector<uint8_t>& data) override {
        outbox.push_back({false, nullptr, dest, data});
        return true;
    }
    bool sendTcp(TCPsocket sock, const std::vector<uint8_t>& data) override {
        outbox.push_back({true, sock, IPaddress{}, data});
        return true;
    }
    bool sendTcp(const std::vector<uint8_t>& data) override {
        // We won’t use this overload in tests—so just return true.
        return true;
    }

    bool receive(TCPsocket& tcpSock, IPaddress& srcUdp, std::vector<uint8_t>& out) override {
        if (inbox.empty()) return false;
        std::tie(tcpSock, srcUdp, out) = inbox.front();
        inbox.pop();
        return true;
    }
    bool receiveUdp(IPaddress& srcUdp, std::vector<uint8_t>& out) override {
        // Not used by NetworkSystem directly in this test suite.
        return false;
    }
    bool receiveTcp(TCPsocket& tcpSock, std::vector<uint8_t>& out, bool& socketClosed) override {
        // Not directly used: we let receive(...) be the main entrypoint.
        return false;
    }
    bool receiveTcp(std::vector<uint8_t>& out, bool& socketClosed) override {
        return false;
    }
};

//-----------------------------------------------------------------------------
// Test Fixture
//-----------------------------------------------------------------------------
struct NetSysTest : public testing::Test
{
    NetworkConfig cfg;
    MockNetworkBackend* mock;
    NetworkSystem* sys;

    // Helper: expose client state for tests
    struct ClientSnapshot
    {
        float rttMs;
        bool exists;
    };

    // Retrieve RTT from internal client state:
    ClientSnapshot getClientSnapshot(uint32_t clientId)
    {
        ClientSnapshot snap {0.0f, false};
        auto ids = sys->getClientIds();

        auto it = std::find(ids.begin(), ids.end(), clientId);

        if (it != ids.end())
        {
            snap.exists = true;
            snap.rttMs = sys->getClientState(clientId).rttMs;
        }

        return snap;
    }

    // Retrieve whether fragment buffer has a given key
    bool fragmentKeyExists(uint32_t packetNumber)
    {
        for (auto& [key, slots] : sys->getReassemblyBuffer())
        {
            if (std::get<2>(key) == packetNumber)
                return true;
        }

        return false;
    }

    void SetUp() override
    {
        cfg.peerAddress      = "127.0.0.1";
        cfg.tcpPort          = 9000;
        cfg.udpLocalPort     = 9001;
        cfg.udpPeerPort      = 9001;
        cfg.isServer         = false;

        // Default flags for timeouts (in ms):
        cfg.defaultSystemFlags.pingTimer = 100;      // 100 ms for ping in tests
        cfg.defaultSystemFlags.dropPacketTimeout = 500; // 500 ms timeout

        mock = new MockNetworkBackend();
        sys  = new NetworkSystem(mock, cfg);
    }

    void TearDown() override
    {
        delete sys;
    }
};

//-----------------------------------------------------------------------------
// 1. Client startup: failed connect -> no receive
//-----------------------------------------------------------------------------
TEST_F(NetSysTest, ClientStartupFailsBackoff) {
    cfg.isServer = false;
    mock->connectSucceeds = false;
    sys->init();
    EXPECT_TRUE(mock->outbox.empty());

    // Even after one execute, no UDP handshake should be sent
    sys->execute();
    EXPECT_TRUE(mock->outbox.empty());
}

//-----------------------------------------------------------------------------
// 2. Client receives malformed welcome (too short) -> ignore
//-----------------------------------------------------------------------------
TEST_F(NetSysTest, ClientIgnoresMalformedWelcome) {
    // Simulate successful connect
    cfg.isServer = false;
    mock->connectSucceeds = true;
    sys->init();

    // Enqueue a 4‐byte "malformed" TCP packet
    uint8_t shortBuf[4] = {0x00,0x01,0x02,0x03};
    mock->inbox.emplace(reinterpret_cast<TCPsocket>(1), IPaddress{}, std::vector<uint8_t>(shortBuf, shortBuf+4));

    // First execute (inside initClient) tries to read welcome; should ignore
    sys->execute();
    EXPECT_TRUE(mock->outbox.empty());
}

//-----------------------------------------------------------------------------
// 3. Client handshake sends UDP Handshake (PacketHeader type = Handshake)
//-----------------------------------------------------------------------------
TEST_F(NetSysTest, ClientHandshakeSendsUdpHello) {
    cfg.isServer = false;
    mock->connectSucceeds = true;

    // Build a proper "Connect" welcome: PacketHeader + zero‐length payload
    uint8_t headerBuf[HEADER_SIZE];
    PacketHeader welcomeHdr {
        123,          // clientId
        0xCAFE,       // token
        0,            // packetNumber (irrelevant here)
        0,            // timestamp
        NetMsgType::Connect,
        1,            // totalFragments
        0,            // fragmentIndex
        0             // payloadLen
    };
    writeHeader(headerBuf, welcomeHdr);

    // Enqueue that full  HEADER_SIZE‐byte welcome as a single TCP packet
    mock->inbox.emplace(reinterpret_cast<TCPsocket>(1), IPaddress{},
                        std::vector<uint8_t>(headerBuf, headerBuf + HEADER_SIZE));

    // Call init() → connectToServer(), receive welcome, send UDP Handshake
    sys->init();

    // Now exactly one fragment (Handshake) should be in mock->outbox
    ASSERT_EQ(mock->outbox.size(), 1u);
    auto& outPkt = mock->outbox[0];
    EXPECT_FALSE(outPkt.isTcp);

    // Reassemble (just one fragment) to verify header fields
    pg::ParsedPacket parsed;
    std::map<std::tuple<uint32_t,uint32_t,uint32_t,NetMsgType>, pg::NetFragmentedPayload> buffer;
    std::map<uint32_t,uint64_t> timers;
    bool ok = parseAndReassemble(outPkt.data, buffer, timers, parsed);
    ASSERT_TRUE(ok);

    EXPECT_EQ(parsed.header.clientId, 123u);
    EXPECT_EQ(parsed.header.token, 0xCAFEu);
    EXPECT_EQ(parsed.header.type, NetMsgType::Handshake);
    EXPECT_EQ(parsed.header.payloadLen, 0u);
}

//-----------------------------------------------------------------------------
// 4. Server accepts TCP, sends Connect, and then links a valid UDP Handshake
//-----------------------------------------------------------------------------
TEST_F(NetSysTest, ServerAcceptsAndLinksUdp) {
    // Switch to server mode
    cfg.isServer = true;
    delete sys;
    mock = new MockNetworkBackend();
    sys  = new NetworkSystem(mock, cfg);
    sys->init();

    // Simulate an incoming TCP connection
    mock->inbox.emplace(mock->fakeListener, IPaddress{}, std::vector<uint8_t>{});
    sys->execute();  // Accept + send Connect

    // ASSERT_EQ(mock->outbox.size(), 1u);
    // auto& welcomeSend = mock->outbox[0];
    // EXPECT_TRUE(welcomeSend.isTcp);

    // // Parse the Connect header
    // pg::ParsedPacket parsedWelcome;
    // std::map<std::tuple<uint32_t,uint32_t,uint32_t,NetMsgType>, pg::NetFragmentedPayload> buffer1;
    // std::map<uint32_t,uint64_t> timers1;
    // bool ok1 = parseAndReassemble(welcomeSend.data, buffer1, timers1, parsedWelcome);
    // ASSERT_TRUE(ok1);

    // uint32_t clientId = parsedWelcome.header.clientId;
    // uint32_t token    = parsedWelcome.header.token;
    // mock->outbox.clear();

    // // Build a valid UDP Handshake fragment
    // auto handshakeFrags = fragmentPayload(
    //     clientId, token, NetMsgType::Handshake, 0, /*empty payload*/ {});

    // // There should be exactly 1 fragment for an empty payload
    // ASSERT_EQ(handshakeFrags.size(), 1u);
    // mock->inbox.emplace(nullptr,
    //                     // Use any UDP src; ipPortKey not checked here
    //                     IPaddress{},
    //                     handshakeFrags[0]);

    // sys->execute();  // Process Handshake + echo back

    // // Now we should have exactly one UDP echo in outbox
    // ASSERT_EQ(mock->outbox.size(), 1u);
    // auto& echoPkt = mock->outbox[0];
    // EXPECT_FALSE(echoPkt.isTcp);

    // // Reassemble to inspect header
    // pg::ParsedPacket parsedEcho;
    // std::map<std::tuple<uint32_t,uint32_t,uint32_t,NetMsgType>, pg::NetFragmentedPayload> buffer2;
    // std::map<uint32_t,uint64_t> timers2;
    // bool ok2 = parseAndReassemble(echoPkt.data, buffer2, timers2, parsedEcho);
    // ASSERT_TRUE(ok2);

    // EXPECT_EQ(parsedEcho.header.clientId, clientId);
    // EXPECT_EQ(parsedEcho.header.token, token);
    // EXPECT_EQ(parsedEcho.header.type, NetMsgType::Handshake);
}

//-----------------------------------------------------------------------------
// 5. ServerDropsInvalidUdpToken: sending Handshake with wrong token is ignored
//-----------------------------------------------------------------------------
TEST_F(NetSysTest, ServerDropsInvalidUdpToken) {
    cfg.isServer = true;
    delete sys;
    mock = new MockNetworkBackend();
    sys  = new NetworkSystem(mock, cfg);
    sys->init();

    // Step A: Accept TCP client → server sends Connect
    mock->inbox.emplace(mock->fakeListener, IPaddress{}, std::vector<uint8_t>{});
    sys->execute();
    ASSERT_EQ(mock->outbox.size(), 1u);

    // Parse Connect to get clientId, token
    pg::ParsedPacket parsedWelcome;
    std::map<std::tuple<uint32_t,uint32_t,uint32_t,NetMsgType>, pg::NetFragmentedPayload> bufW;
    std::map<uint32_t,uint64_t> timersW;
    bool okW = parseAndReassemble(mock->outbox[0].data, bufW, timersW, parsedWelcome);
    ASSERT_TRUE(okW);
    uint32_t clientId = parsedWelcome.header.clientId;
    uint32_t token    = parsedWelcome.header.token;
    mock->outbox.clear();

    // Step B: Valid Handshake to link the client
    auto validHandhake = fragmentPayload(clientId, token, NetMsgType::Handshake, 1, {});
    mock->inbox.emplace(nullptr, IPaddress{}, validHandhake[0]);
    sys->execute();
    mock->outbox.clear();  // clear the echo from the valid handshake

    // Step C: Now send an invalid‐token Handshake
    auto badHandshake = fragmentPayload(clientId, /*wrong token*/ 0xFFFF, NetMsgType::Handshake, 2, {});
    mock->inbox.emplace(nullptr, IPaddress{}, badHandshake[0]);
    sys->execute();

    // Because token is wrong, outbox should remain empty
    EXPECT_TRUE(mock->outbox.empty());
}

//-----------------------------------------------------------------------------
// 6. ServerEchoesPayload: after linking, a real payload is echoed back
//-----------------------------------------------------------------------------
TEST_F(NetSysTest, ServerEchoesPayload) {
    cfg.isServer = true;
    delete sys;
    mock = new MockNetworkBackend();
    sys  = new NetworkSystem(mock, cfg);
    sys->init();

    // Step A: Accept + Connect
    mock->inbox.emplace(mock->fakeListener, IPaddress{}, std::vector<uint8_t>{});
    sys->execute();
    ASSERT_EQ(mock->outbox.size(), 1u);

    // Parse to get clientId, token
    pg::ParsedPacket parsedWelcome;
    std::map<std::tuple<uint32_t,uint32_t,uint32_t,NetMsgType>, pg::NetFragmentedPayload> bufW;
    std::map<uint32_t,uint64_t> timersW;
    bool okW = parseAndReassemble(mock->outbox[0].data, bufW, timersW, parsedWelcome);
    ASSERT_TRUE(okW);
    uint32_t clientId = parsedWelcome.header.clientId;
    uint32_t token    = parsedWelcome.header.token;
    mock->outbox.clear();

    // Step B: Valid Handshake
    auto handshakeFrags = fragmentPayload(clientId, token, NetMsgType::Handshake,  1, {});
    mock->inbox.emplace(nullptr, IPaddress{}, handshakeFrags[0]);
    sys->execute();
    mock->outbox.clear();  // drop the handshake echo

    // Step C: Send a real payload ("ABC")
    const char* msg = "ABC";
    std::vector<uint8_t> payload(msg, msg + 3);
    auto dataFrags = fragmentPayload(clientId, token, NetMsgType::Custom, 2, payload);
    // (Use NetMsgType::Custom for a generic “echoable” payload)
    mock->inbox.emplace(nullptr, IPaddress{}, dataFrags[0]);
    sys->execute();

    // Now exactly one echo should be in outbox
    ASSERT_EQ(mock->outbox.size(), 1u);
    auto& echoPkt = mock->outbox[0];
    EXPECT_FALSE(echoPkt.isTcp);

    // Reassemble to inspect header & payload
    pg::ParsedPacket parsedEcho;
    std::map<std::tuple<uint32_t,uint32_t,uint32_t,NetMsgType>, pg::NetFragmentedPayload> buf2;
    std::map<uint32_t,uint64_t> timers2;
    bool ok2 = parseAndReassemble(echoPkt.data, buf2, timers2, parsedEcho);
    ASSERT_TRUE(ok2);

    EXPECT_EQ(parsedEcho.header.clientId, clientId);
    EXPECT_EQ(parsedEcho.header.token,    token);
    EXPECT_EQ(parsedEcho.header.type,     NetMsgType::Custom);
    EXPECT_EQ(parsedEcho.payload.size(),  3u);

    std::string body(parsedEcho.payload.begin(), parsedEcho.payload.end());
    EXPECT_EQ(body, "ABC");
}

//-----------------------------------------------------------------------------
// 7. ServerHandlesMultiplePackets: two back‐to‐back Handshakes are echoed
//-----------------------------------------------------------------------------
TEST_F(NetSysTest, ServerHandlesMultiplePackets) {
    cfg.isServer = true;
    delete sys;
    mock = new MockNetworkBackend();
    sys  = new NetworkSystem(mock, cfg);
    sys->init();

    // Accept + Connect
    mock->inbox.emplace(mock->fakeListener, IPaddress{}, std::vector<uint8_t>{});
    sys->execute();
    ASSERT_EQ(mock->outbox.size(), 1u);

    // Parse to get clientId & token
    pg::ParsedPacket pw;
    std::map<std::tuple<uint32_t,uint32_t,uint32_t,NetMsgType>, pg::NetFragmentedPayload> bufW2;
    std::map<uint32_t,uint64_t> timersW2;
    bool okW2 = parseAndReassemble(mock->outbox[0].data, bufW2, timersW2, pw);
    ASSERT_TRUE(okW2);

    uint32_t clientId = pw.header.clientId;
    uint32_t token    = pw.header.token;
    mock->outbox.clear();

    // Build two Handshakes (same packetNumber=1 for simplicity)
    auto handshake1 = fragmentPayload(clientId, token, NetMsgType::Handshake, 1, {});
    auto handshake2 = fragmentPayload(clientId, token, NetMsgType::Handshake, 1, {});

    IPaddress src{};
    SDLNet_ResolveHost(&src, "1.2.3.4", 1111);

    mock->inbox.emplace(nullptr, src, handshake1[0]);
    mock->inbox.emplace(nullptr, src, handshake2[0]);

    sys->execute();

    // Should echo back *twice*
    EXPECT_EQ(mock->outbox.size(), 2u);
}

//-----------------------------------------------------------------------------
// 8. EmptyQueueNoAction: nothing in inbox → no outbox output
//-----------------------------------------------------------------------------
TEST_F(NetSysTest, EmptyQueueNoAction) {
    cfg.isServer = true; // or false, same effect
    sys->init();
    mock->outbox.clear();
    sys->execute();
    EXPECT_TRUE(mock->outbox.empty());
}

//-----------------------------------------------------------------------------
// 9. Server emits Ping after pingTimer elapses
//-----------------------------------------------------------------------------
// TEST_F(NetSysTest, ServerEmitsPingAfterTimer) {
//     cfg.isServer = true;
//     delete sys;
//     mock = new MockNetworkBackend();
//     sys  = new NetworkSystem(mock, cfg);
//     sys->init();

//     // Simulate client connect → server sends Connect
//     mock->inbox.emplace(mock->fakeListener, IPaddress{}, std::vector<uint8_t>{});
//     sys->runServerFrame(0.0f);
//     ASSERT_FALSE(mock->outbox.empty());
//     mock->outbox.clear();

//     // Advance time by pingTimer
//     sys->runServerFrame(cfg.defaultSystemFlags.pingTimer);
//     // There must now be a Ping send
//     ASSERT_FALSE(mock->outbox.empty());

//     // Verify it is a Ping (via parseAndReassemble)
//     pg::ParsedPacket parsedPing;
//     std::map<std::tuple<uint32_t,uint32_t,uint32_t,NetMsgType>, pg::NetFragmentedPayload> bufPing;
//     std::map<uint32_t,uint64_t> timersPing;
//     bool ok = parseAndReassemble(mock->outbox[0].data, bufPing, timersPing, parsedPing);
//     ASSERT_TRUE(ok);
//     EXPECT_EQ(parsedPing.header.type, NetMsgType::Ping);
// }

// //-----------------------------------------------------------------------------
// // 10. Server updates RTT on Pong
// //-----------------------------------------------------------------------------
// TEST_F(NetSysTest, ServerUpdatesRTTOnPong) {
//     cfg.isServer = true;
//     delete sys;
//     mock = new MockNetworkBackend();
//     sys  = new NetworkSystem(mock, cfg);
//     sys->init();

//     // Simulate client accept
//     mock->inbox.emplace(mock->fakeListener, IPaddress{}, std::vector<uint8_t>{});
//     sys->runServerFrame(0.0f);
//     ASSERT_EQ(mock->outbox.size(), 1u);

//     // Parse Connect to get clientId & token
//     pg::ParsedPacket parsedWelcome;
//     std::map<std::tuple<uint32_t,uint32_t,uint32_t,NetMsgType>, pg::NetFragmentedPayload> bufWW;
//     std::map<uint32_t,uint64_t> timersWW;
//     bool okW = parseAndReassemble(mock->outbox[0].data, bufWW, timersWW, parsedWelcome);
//     ASSERT_TRUE(okW);
//     uint32_t clientId = parsedWelcome.header.clientId;
//     uint32_t token    = parsedWelcome.header.token;
//     mock->outbox.clear();

//     // Force a Ping send (advance time)
//     sys->runServerFrame(cfg.defaultSystemFlags.pingTimer);
//     ASSERT_FALSE(mock->outbox.empty());
//     mock->outbox.clear();

//     // Craft a Pong payload with a fake timestamp (50 ms ago)
//     uint64_t fakeSendTs = sys->getCurrentTime() - 50;
//     uint8_t tsBuf[8];
//     writeU64BE(tsBuf, fakeSendTs);
//     NetPayload pongPayload(tsBuf, tsBuf + 8);

//     auto pongFrags = fragmentPayload(clientId, token, NetMsgType::Pong, 1, pongPayload);
//     for (auto& frag : pongFrags) {
//         mock->inbox.emplace(reinterpret_cast<TCPsocket>(clientId), IPaddress{}, frag);
//     }

//     sys->runServerFrame(0.0f);

//     // Check the RTT has been updated (~50ms)
//     auto snap = getClientSnapshot(clientId);
//     ASSERT_TRUE(snap.exists);
//     EXPECT_GE(snap.rttMs, 40.0f);
//     EXPECT_LE(snap.rttMs, 60.0f);
// }

// //-----------------------------------------------------------------------------
// // 11. FragmentTimeoutPurgesStale
// //-----------------------------------------------------------------------------
// TEST_F(NetSysTest, FragmentTimeoutPurgesStale) {
//     cfg.isServer = false;
//     delete sys;
//     mock = new MockNetworkBackend();
//     sys  = new NetworkSystem(mock, cfg);
//     sys->init();

//     // Manually insert incomplete fragments for a packetNumber = 99
//     uint32_t fakeClientId = 123;
//     uint32_t fakeToken    = 0xBBBB;
//     uint32_t packetNum    = 99;
//     auto key = std::make_tuple(fakeClientId, fakeToken, packetNum, NetMsgType::EntityData);

//     // Ensure buffer has 2 slots but leave them empty
//     sys->getReassemblyBuffer()[key].resize(2);
//     // Set its first‐seen time to far past timeout
//     sys->getFragmentTimers()[packetNum] = sys->getCurrentTime() - (cfg.defaultSystemFlags.dropPacketTimeout + 1);

//     // Call cleanup
//     sys->cleanReassemblyBuffer();

//     // It must be removed
//     EXPECT_FALSE(sys->getReassemblyBuffer().count(key));
//     EXPECT_FALSE(sys->getFragmentTimers().count(packetNum));
// }

// //-----------------------------------------------------------------------------
// // 12. CleanDisconnectRemovesClient
// //-----------------------------------------------------------------------------
// TEST_F(NetSysTest, CleanDisconnectRemovesClient) {
//     cfg.isServer = true;
//     delete sys;
//     mock = new MockNetworkBackend();
//     sys  = new NetworkSystem(mock, cfg);
//     sys->init();

//     // Accept + Connect
//     mock->inbox.emplace(mock->fakeListener, IPaddress{}, std::vector<uint8_t>{});
//     sys->runServerFrame(0.0f);
//     ASSERT_FALSE(mock->outbox.empty());

//     // Parse Connect to get clientId & token
//     pg::ParsedPacket parsedWelcome;
//     std::map<std::tuple<uint32_t,uint32_t,uint32_t,NetMsgType>, pg::NetFragmentedPayload> bufW3;
//     std::map<uint32_t,uint64_t> timersW3;
//     bool okW3 = parseAndReassemble(mock->outbox[0].data, bufW3, timersW3, parsedWelcome);
//     ASSERT_TRUE(okW3);

//     uint32_t clientId = parsedWelcome.header.clientId;
//     uint32_t token    = parsedWelcome.header.token;
//     mock->outbox.clear();

//     // Build a Disconnect packet (no payload)
//     auto discFrags = fragmentPayload(clientId, token, NetMsgType::Disconnect, 42, {});

//     for (auto& frag : discFrags) {
//         mock->inbox.emplace(reinterpret_cast<TCPsocket>(clientId), IPaddress{}, frag);
//     }

//     sys->runServerFrame(0.0f);

//     // After processing Disconnect, client should no longer exist
//     EXPECT_FALSE(sys->clientExists(clientId));
// }