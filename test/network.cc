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

    struct Out
    {
        bool isTcp;
        TCPsocket sock;
        IPaddress addr;
        std::vector<uint8_t> data;
    };

    std::vector<Out> outbox;

    TCPsocket fakeListener = reinterpret_cast<TCPsocket>(0x1);

    TCPsocket acceptTcpClient() override
    {
        // pop a marker if it matches
        if (not inbox.empty())
        {
            auto [sock, addr, dat] = inbox.front();

            if (sock == fakeListener and dat.empty())
            {
                inbox.pop();
                return reinterpret_cast<TCPsocket>(0x42);
            }
        }

        return nullptr;
    }

    bool connectToServer() override { return connectSucceeds; }

    bool sendUdp(const IPaddress& dest, const std::vector<uint8_t>& data) override
    {
        outbox.push_back({false, nullptr, dest, data});
        return true;
    }

    bool sendTcp(TCPsocket sock, const std::vector<uint8_t>& data) override
    {
        outbox.push_back({true, sock, IPaddress{}, data});
        return true;
    }

    bool receive(TCPsocket& tcpSock, IPaddress& srcUdp, std::vector<uint8_t>& out) override
    {
        if (inbox.empty())
            return false;

        std::tie(tcpSock, srcUdp, out) = inbox.front();
        inbox.pop();
        return true;
    }

    bool recvUdpHeader(UdpHeader& hdr, IPaddress& src) override
    {
        TCPsocket dummy; std::vector<uint8_t> raw;

        if (not receive(dummy, src, raw) or dummy != nullptr)
            return false;

        hdr = readHeader(raw.data());
        return true;
    }

    // Control flags for tests
    bool connectSucceeds = true;
};

//-----------------------------------------------------------------------------
// Test Fixture
//-----------------------------------------------------------------------------
struct NetSysTest : public testing::Test
{
    NetworkConfig cfg;
    MockNetworkBackend* mock;
    NetworkSystem* sys;

    void SetUp() override
    {
        cfg.peerAddress    = "127.0.0.1";
        cfg.tcpPort        = 9000;
        cfg.udpLocalPort   = 9001;
        cfg.udpPeerPort    = 9001;
        cfg.isServer       = false;

        mock = new MockNetworkBackend();
        sys  = new NetworkSystem(mock, cfg);
    }

    void TearDown() override
    {
        delete sys; // also deletes mock
    }
};

//-----------------------------------------------------------------------------
// 1. Client startup: failed connect -> no receive
//-----------------------------------------------------------------------------
TEST_F(NetSysTest, ClientStartupFailsBackoff)
{
    cfg.isServer = false;
    mock->connectSucceeds = false;
    sys->init();
    EXPECT_FALSE(mock->outbox.size());  // no UDP handshake yet

    sys->execute();
    // still not connected, no receive called
    EXPECT_FALSE(mock->outbox.size());
}

//-----------------------------------------------------------------------------
// 2. Client receives malformed welcome (too short) -> ignore
//-----------------------------------------------------------------------------
TEST_F(NetSysTest, ClientIgnoresMalformedWelcome)
{
    // simulate successful connect
    mock->connectSucceeds = true;
    sys->init();
    // enqueue 4-byte instead of 8
    uint8_t shortBuf[4] = {0,1,2,3};
    mock->inbox.emplace(reinterpret_cast<TCPsocket>(1), IPaddress{}, std::vector<uint8_t>(shortBuf, shortBuf+4));
    sys->execute();
    // no UDP send
    EXPECT_TRUE(mock->outbox.empty());
}

//-----------------------------------------------------------------------------
// 3. Client handshake sends UDP hello
//-----------------------------------------------------------------------------
TEST_F(NetSysTest, ClientHandshakeSendsUdpHello)
{
    cfg.isServer = false;
    mock->connectSucceeds = true;

    // 1) Enqueue the TCP welcome BEFORE init()
    uint8_t welcome[8];
    SDLNet_Write32(123,     welcome + 0);    // clientId
    SDLNet_Write32(0xCAFE,  welcome + 4);    // token
    mock->inbox.emplace((TCPsocket)1, IPaddress{},
                        std::vector<uint8_t>(welcome, welcome + 8));

    // 2) Call init(), which will:
    //    - connectToServer()
    //    - receive() the TCP welcome
    //    - sendUdpHandshake()
    sys->init();

    // 3) Inspect outbox for exactly one UDP send
    ASSERT_EQ(mock->outbox.size(), 1u);
    auto& pkt = mock->outbox[0];
    EXPECT_FALSE(pkt.isTcp);

    UdpHeader h = readHeader(pkt.data.data());
    EXPECT_EQ(h.clientId, 123u);
    EXPECT_EQ(h.token,    0xCAFEu);
    EXPECT_EQ(h.payloadLen, 0u);

    // No payload bytes beyond the header
    EXPECT_EQ(pkt.data.size(), sizeof(UdpHeader));
}

//-----------------------------------------------------------------------------
// 4. Server welcome and link UDP handshake
//-----------------------------------------------------------------------------
TEST_F(NetSysTest, ServerAcceptsAndLinksUdp)
{
    // reconfigure fixture as server
    cfg.isServer = true;
    delete sys;

    mock = new MockNetworkBackend();
    sys  = new NetworkSystem(mock, cfg);
    sys->init();

    // simulate accept marker
    mock->inbox.emplace(mock->fakeListener, IPaddress{}, std::vector<uint8_t>{});

    sys->execute(); // sends welcome

    ASSERT_EQ(mock->outbox.size(), 1u);
    auto& welcomeSend = mock->outbox[0];
    EXPECT_TRUE(welcomeSend.isTcp);
    uint32_t clientId = SDLNet_Read32(welcomeSend.data.data());
    uint32_t token    = SDLNet_Read32(welcomeSend.data.data()+4);

    // clear outbox, simulate UDP handshake
    mock->outbox.clear();

    uint8_t udpBuf[sizeof(UdpHeader)];
    SDLNet_Write32(clientId, udpBuf + 0);
    SDLNet_Write32(token,    udpBuf + 4);
    SDLNet_Write16(0,        udpBuf + 8);

    IPaddress src{}; SDLNet_ResolveHost(&src, "1.2.3.4", 5555);
    mock->inbox.emplace(nullptr, src, std::vector<uint8_t>(udpBuf, udpBuf+sizeof(udpBuf)));

    sys->execute(); // link + echo

    ASSERT_EQ(mock->outbox.size(), 1u);
    auto& echo = mock->outbox[0];
    EXPECT_FALSE(echo.isTcp);

    UdpHeader h2 = readHeader(echo.data.data());
    EXPECT_EQ(h2.clientId, clientId);
    EXPECT_EQ(h2.token, token);
}

//-----------------------------------------------------------------------------
// 5. Invalid UDP token is dropped
//-----------------------------------------------------------------------------
// -------------- ServerDropsInvalidUdpToken --------------
TEST_F(NetSysTest, ServerDropsInvalidUdpToken) {
    // 1) Server mode

    cfg.isServer = true;
    delete sys;
    mock = new MockNetworkBackend();
    sys  = new NetworkSystem(mock, cfg);
    sys->init();

    // 2) Simulate TCP accept → server sends welcome
    mock->inbox.emplace(mock->fakeListener, IPaddress{}, std::vector<uint8_t>{});
    sys->execute();
    ASSERT_EQ(mock->outbox.size(), 1u);
    // Extract clientId and token from the welcome
    auto& welcomeSend = mock->outbox[0];
    uint32_t clientId = SDLNet_Read32(welcomeSend.data.data());
    uint32_t token    = SDLNet_Read32(welcomeSend.data.data()+4);

    // drop that welcome
    mock->outbox.clear();

    // 3) First do a valid UDP handshake to link the client
    {
        uint8_t buf[sizeof(UdpHeader)];
        SDLNet_Write32(clientId, buf+0);
        SDLNet_Write32(token,    buf+4);
        SDLNet_Write16(0,        buf+8);
        IPaddress src{}; SDLNet_ResolveHost(&src,"1.2.3.4",1234);
        mock->inbox.emplace(nullptr, src,
            std::vector<uint8_t>(buf, buf+sizeof(buf)));
    }
    sys->execute();
    // drop the echo from the valid mapping
    mock->outbox.clear();

    // 4) Now inject the *invalid* handshake (same clientId, wrong token)
    {
        uint8_t buf[sizeof(UdpHeader)];
        SDLNet_Write32(clientId, buf+0);
        SDLNet_Write32(9999,     buf+4);  // WRONG token
        SDLNet_Write16(0,        buf+8);
        IPaddress src{}; SDLNet_ResolveHost(&src,"1.2.3.4",1234);
        mock->inbox.emplace(nullptr, src,
            std::vector<uint8_t>(buf, buf+sizeof(buf)));
    }
    sys->execute();

    // 5) Because token is invalid, no echo should be sent
    EXPECT_TRUE(mock->outbox.empty());
}

//-----------------------------------------------------------------------------
// 6. Echo payload back correctly
//-----------------------------------------------------------------------------
TEST_F(NetSysTest, ServerEchoesPayload) {
    // 1) Server mode
    cfg.isServer = true;
    delete sys;
    mock = new MockNetworkBackend();
    sys  = new NetworkSystem(mock, cfg);
    sys->init();

    // 2) Accept → server sends welcome
    mock->inbox.emplace(mock->fakeListener, IPaddress{}, std::vector<uint8_t>{});
    sys->execute();
    ASSERT_EQ(mock->outbox.size(), 1u);
    auto& welcomeSend = mock->outbox[0];
    uint32_t clientId = SDLNet_Read32(welcomeSend.data.data());
    uint32_t token    = SDLNet_Read32(welcomeSend.data.data()+4);
    mock->outbox.clear();

    // 3) Valid UDP handshake mapping
    {
        uint8_t buf[sizeof(UdpHeader)];
        SDLNet_Write32(clientId, buf+0);
        SDLNet_Write32(token,    buf+4);
        SDLNet_Write16(0,        buf+8);
        IPaddress src{}; SDLNet_ResolveHost(&src,"9.9.9.9",9999);
        mock->inbox.emplace(nullptr, src,
            std::vector<uint8_t>(buf, buf+sizeof(buf)));
    }
    sys->execute();
    // drop the echo from the handshake
    mock->outbox.clear();

    // 4) Now send a real payload ("ABC")
    const char* msg = "ABC";
    {
        uint8_t buf[sizeof(UdpHeader) + 3];
        SDLNet_Write32(clientId, buf+0);
        SDLNet_Write32(token,    buf+4);
        SDLNet_Write16(3,        buf+8);
        memcpy(buf+sizeof(UdpHeader), msg, 3);
        IPaddress src{}; SDLNet_ResolveHost(&src,"9.9.9.9",9999);
        mock->inbox.emplace(nullptr, src,
            std::vector<uint8_t>(buf, buf+sizeof(buf)));
    }
    sys->execute();

    // 5) Echo should be sent back exactly once
    ASSERT_EQ(mock->outbox.size(), 1u);
    auto& echoSend = mock->outbox[0];
    EXPECT_FALSE(echoSend.isTcp);
    UdpHeader h = readHeader(echoSend.data.data());
    EXPECT_EQ(h.clientId,    clientId);
    EXPECT_EQ(h.token,       token);
    EXPECT_EQ(h.payloadLen, 3u);

    std::string body(
      reinterpret_cast<const char*>(echoSend.data.data() + sizeof(UdpHeader)),
      3
    );
    EXPECT_EQ(body, "ABC");
}

//-----------------------------------------------------------------------------
// 7. Back‐to‐back packets are all processed
//-----------------------------------------------------------------------------
TEST_F(NetSysTest, ServerHandlesMultiplePackets)
{
    cfg.isServer = true;
    delete sys;
    mock = new MockNetworkBackend();
    sys  = new NetworkSystem(mock, cfg);

    sys->init();

    // accept marker
    mock->inbox.emplace(mock->fakeListener, IPaddress{}, std::vector<uint8_t>{});

    sys->execute();

    mock->outbox.clear();

    // create two handshake pkt for same client
    uint32_t id = SDLNet_Read32(mock->outbox[0].data.data());
    uint32_t token = SDLNet_Read32(mock->outbox[0].data.data()+4);
    uint8_t buf[sizeof(UdpHeader)];
    SDLNet_Write32(id, buf+0);
    SDLNet_Write32(token, buf+4);
    SDLNet_Write16(0, buf+8);

    IPaddress src{};
    SDLNet_ResolveHost(&src, "1.2.3.4", 1111);

    // enqueue two
    mock->inbox.emplace(nullptr, src, std::vector<uint8_t>(buf, buf + sizeof(buf)));
    mock->inbox.emplace(nullptr, src, std::vector<uint8_t>(buf, buf + sizeof(buf)));

    sys->execute();

    // should echo back twice
    EXPECT_EQ(mock->outbox.size(), 2u);
}

//-----------------------------------------------------------------------------
// 8. Empty queue does nothing
//-----------------------------------------------------------------------------
TEST_F(NetSysTest, EmptyQueueNoAction)
{
    sys->init();
    // no inbox
    mock->outbox.clear();

    sys->execute();

    EXPECT_TRUE(mock->outbox.empty());
}
