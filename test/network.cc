// test_networksystem.cpp
#include <gtest/gtest.h>
#include "Networking/network_system.h"
#include <tuple>

//------------------------------------------------------------------------
// MockNetworkBackend: captures sends in an outbox and provides a scripted inbox.
//------------------------------------------------------------------------
class MockNetworkBackend : public INetworkBackend {
public:
    // Inbox entries: either TCP or UDP
    std::queue<std::tuple<TCPsocket, IPaddress, std::vector<uint8_t>>> inbox;
    // Outbox entries: record (isTcp, TCPsocket, IPaddress, data)
    struct Out { bool isTcp; TCPsocket sock; IPaddress addr; std::vector<uint8_t> data; };
    std::vector<Out> outbox;

    // Simulated “already listening” socket for server
    TCPsocket fakeListener = reinterpret_cast<TCPsocket>(0x1);

    // --- INetworkBackend impl ---
    TCPsocket acceptTcpClient() override {
        // If inbox has a special marker for accept, pop it
        if (!inbox.empty()) {
            auto [sock, addr, data] = inbox.front();
            // We encode accept by sock==fakeListener && data empty
            if (sock == fakeListener && data.empty()) {
                inbox.pop();
                return reinterpret_cast<TCPsocket>(0x42); // new client sock
            }
        }
        return nullptr;
    }

    bool connectToServer() override {
        // Always succeed
        return true;
    }

    bool sendUdp(const IPaddress& dest, const std::vector<uint8_t>& data) override {
        outbox.push_back({false, nullptr, dest, data});
        return true;
    }
    bool sendTcp(TCPsocket sock, const std::vector<uint8_t>& data) override {
        outbox.push_back({true, sock, IPaddress{}, data});
        return true;
    }

    bool receive(TCPsocket& tcpSock,
                 IPaddress& srcUdp,
                 std::vector<uint8_t>& out) override {
        if (inbox.empty()) return false;
        std::tie(tcpSock, srcUdp, out) = inbox.front();
        inbox.pop();
        return true;
    }

    bool recvUdpHeader(UdpHeader& hdr, IPaddress& src) override {
        std::vector<uint8_t> raw;
        TCPsocket dummy;
        if (!receive(dummy, src, raw) || dummy != nullptr) return false;
        hdr = readHeader(raw.data());
        return true;
    }
};

//------------------------------------------------------------------------
// Test: Client handshake produces one UDP send with correct header
//------------------------------------------------------------------------
TEST(NetworkSystemTest, ClientHandshakeSendsUdpHello) {
    NetworkConfig cfg;
    cfg.isServer    = false;
    cfg.peerAddress = "127.0.0.1";
    cfg.tcpPort     = 9000;
    cfg.udpLocalPort= 0;
    cfg.udpPeerPort = 9001;

    auto* mock = new MockNetworkBackend();
    // We need to feed the "welcome" TCP packet into the inbox for initClient()
    // Welcome is [clientId,token] in BE.
    uint8_t welcome[8];
    SDLNet_Write32(123, welcome+0);
    SDLNet_Write32(0xCAFE, welcome+4);
    // Enqueue a TCP receive: sock=non-null to indicate TCP data
    IPaddress dummyIp{}; // unused
    mock->inbox.emplace(reinterpret_cast<TCPsocket>(1), dummyIp,
                        std::vector<uint8_t>(welcome, welcome+8));

    // Create system & inject mock
    NetworkSystem sys(mock, cfg);
    sys.init();      // initClient triggers connect
    sys.execute();   // should consume the welcome and send UDP hello

    // Verify one UDP send
    ASSERT_EQ(mock->outbox.size(), 1u);
    auto& first = mock->outbox[0];
    EXPECT_FALSE(first.isTcp);  // UDP
    ASSERT_GE(first.data.size(), sizeof(UdpHeader));

    // Check header contents
    UdpHeader h = readHeader(first.data.data());
    EXPECT_EQ(h.clientId, 123);
    EXPECT_EQ(h.token, 0xCAFE);
    EXPECT_EQ(h.payloadLen, 0);
}

//------------------------------------------------------------------------
// Test: Server accepts TCP, sends welcome, then links UDP handshake
//------------------------------------------------------------------------
TEST(NetworkSystemTest, ServerLinksUdpHandshake) {
    NetworkConfig cfg;
    cfg.isServer     = true;
    cfg.tcpPort      = 9000;
    cfg.udpLocalPort = 9001;

    auto* mock = new MockNetworkBackend();
    // Tell acceptTcpClient() to fire: push an “accept” marker
    mock->inbox.emplace(mock->fakeListener, IPaddress{}, std::vector<uint8_t>{});

    // Create system & init
    NetworkSystem sys(mock, cfg);
    sys.init();
    sys.execute();  // handles TCP accept and sends 8-byte welcome

    // After this, mock->outbox should have one TCP send (the welcome)
    ASSERT_EQ(mock->outbox.size(), 1u);
    auto& welcomeSend = mock->outbox[0];
    EXPECT_TRUE(welcomeSend.isTcp);
    // welcomeSend.sock should be the client-socket repr (0x42)
    EXPECT_EQ(welcomeSend.sock, reinterpret_cast<TCPsocket>(0x42));
    ASSERT_EQ(welcomeSend.data.size(), 8u);
    uint32_t newId  = SDLNet_Read32(welcomeSend.data.data());
    uint32_t token  = SDLNet_Read32(welcomeSend.data.data()+4);
    EXPECT_GT(newId, 0u);
    EXPECT_NE(token, 0u);

    // Now simulate the client sending the UDP handshake
    mock->outbox.clear();
    uint8_t udpBuf[sizeof(UdpHeader)];
    SDLNet_Write32(newId, udpBuf+0);
    SDLNet_Write32(token, udpBuf+4);
    SDLNet_Write16(0, udpBuf+8);  // zero payloadLen
    IPaddress clientAddr{};
    // Feed into inbox as UDP: sock=nullptr
    mock->inbox.emplace(nullptr, clientAddr,
                        std::vector<uint8_t>(udpBuf, udpBuf+sizeof(UdpHeader)));

    // Run another frame
    sys.execute();

    // Now the server should have linked and echoed the header back via UDP
    ASSERT_EQ(mock->outbox.size(), 1u);
    auto& echoSend = mock->outbox[0];
    EXPECT_FALSE(echoSend.isTcp);
    // The echoed data should match the header we sent
    UdpHeader echoed = readHeader(echoSend.data.data());
    EXPECT_EQ(echoed.clientId, newId);
    EXPECT_EQ(echoed.token,    token);
    EXPECT_EQ(echoed.payloadLen, 0);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
