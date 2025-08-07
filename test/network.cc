// // __tests__/NetworkSystem_ipPortKey_test.cpp

// #include <gtest/gtest.h>

// #include "mocknetworking.h"

// #include "Networking/common.h"
// #include "Networking/network_system.h"

// using namespace pg;

// namespace pg
// {
//     inline std::ostream& operator<<(std::ostream& os, const NetworkConfig& cfg)
//     {
//         os << "{ isServer=" << std::boolalpha << cfg.isServer << " }";
//         return os;
//     }
// }

// class net_server_test : public ::testing::TestWithParam<NetworkConfig>
// {
// protected:
//     MockBackend*      backendPtr;
//     std::unique_ptr<NetworkSystem> netSys;

//     void SetUp() override
//     {
//         backendPtr = new MockBackend();

//         // Suppose your NetworkSystem constructor is:
//         //    NetworkSystem(INetworkBackend* backend, const NetworkConfig& cfg);
//         NetworkConfig cfg = GetParam();
//         // … fill cfg as needed for tests …

//         netSys.reset(new NetworkSystem(backendPtr, cfg));
//     }

//     void TearDown() override
//     {
//         // NetworkSystem’s destructor will delete backendPtr,
//         // so we don’t delete backendPtr here.
//         netSys.reset();
//     }
// };

// static std::vector<NetworkConfig> AllConfigsForServerSystems() {
//     std::vector<NetworkConfig> params;

//     NetworkConfig c1;
//     c1.isServer      = true;
//     c1.defaultSystemFlags.socketSetSize = 16;
//     params.push_back(c1);

//     NetworkConfig c2;
//     c2.isServer      = true;
//     c2.defaultSystemFlags.socketSetSize = 64;
//     params.push_back(c2);

//     NetworkConfig c3;
//     c3.isServer      = true;
//     c3.defaultSystemFlags.socketSetSize = 1;
//     params.push_back(c3);

//     return params;
// }

// class net_client_test : public ::testing::TestWithParam<NetworkConfig>
// {
// protected:
//     MockBackend*      backendPtr;
//     std::unique_ptr<NetworkSystem> netSys;

//     void SetUp() override
//     {
//         backendPtr = new MockBackend();

//         // Suppose your NetworkSystem constructor is:
//         //    NetworkSystem(INetworkBackend* backend, const NetworkConfig& cfg);
//         NetworkConfig cfg = GetParam();
//         // … fill cfg as needed for tests …

//         netSys.reset(new NetworkSystem(backendPtr, cfg));
//     }

//     void TearDown() override
//     {
//         // NetworkSystem’s destructor will delete backendPtr,
//         // so we don’t delete backendPtr here.
//         netSys.reset();
//     }
// };

// static std::vector<NetworkConfig> AllConfigsForClientSystems() {
//     std::vector<NetworkConfig> params;

//     NetworkConfig c1;
//     c1.isServer      = false;
//     c1.defaultSystemFlags.socketSetSize = 16;
//     params.push_back(c1);

//     NetworkConfig c2;
//     c2.isServer      = false;
//     c2.defaultSystemFlags.socketSetSize = 64;
//     params.push_back(c2);

//     NetworkConfig c3;
//     c3.isServer      = false;
//     c3.defaultSystemFlags.socketSetSize = 1;
//     params.push_back(c3);

//     return params;
// }

// class net_system_test : public ::testing::TestWithParam<NetworkConfig>
// {
// protected:
//     MockBackend*      backendPtr;
//     std::unique_ptr<NetworkSystem> netSys;

//     void SetUp() override
//     {
//         backendPtr = new MockBackend();

//         // Suppose your NetworkSystem constructor is:
//         //    NetworkSystem(INetworkBackend* backend, const NetworkConfig& cfg);
//         NetworkConfig cfg = GetParam();
//         // … fill cfg as needed for tests …

//         netSys.reset(new NetworkSystem(backendPtr, cfg));
//     }

//     void TearDown() override
//     {
//         // NetworkSystem’s destructor will delete backendPtr,
//         // so we don’t delete backendPtr here.
//         netSys.reset();
//     }
// };

// static std::vector<NetworkConfig> AllConfigsForNetSystems() {
//     std::vector<NetworkConfig> params;

//     // Add both server and client configurations to test all cases
//     auto serverConfigs = AllConfigsForServerSystems();
//     params.insert(params.end(), serverConfigs.begin(), serverConfigs.end());

//     auto clientConfigs = AllConfigsForClientSystems();
//     params.insert(params.end(), clientConfigs.begin(), clientConfigs.end());

//     return params;
// }

// TEST(net_common, u64_roundtrip)
// {
//     MockBackend backend;

//     uint8_t buffer[8];

//     uint64_t original = 0x1122334455667788ULL;
//     backend.writeU64BE(buffer, original);
//     uint64_t readback = backend.readU64BE(buffer);
//     EXPECT_EQ(readback, original);
// }

// TEST(net_common, u16_u32_roundtrip)
// {
//     MockBackend backend;
//     uint8_t buf16[2], buf32[4];

//     uint16_t v16 = 0xABCD;
//     backend.writeU16BE(buf16, v16);
//     EXPECT_EQ(backend.readU16BE(buf16), v16);

//     uint32_t v32 = 0xDEADBEEF;
//     backend.writeU32BE(buf32, v32);
//     EXPECT_EQ(backend.readU32BE(buf32), v32);
// }

// TEST(net_common, small_payload_no_fragments)
// {
//     MockBackend backend;

//     // If payload size + header <= MAX_PAYLOAD, we get exactly one fragment.
//     NetPayload payload = {0x01, 0x02, 0x03, 0x04};
//     uint32_t clientId = 42, token = 0xCAFEBABE, packetNumber = 7;
//     NetMsgType type = NetMsgType::Custom;

//     auto frags = fragmentPayload(clientId, token, type, packetNumber, &backend, payload);
//     ASSERT_EQ(frags.size(), 1u);

//     NetPacketBuffer buffer;
//     std::map<uint32_t, uint64_t> timers;
//     ParsedPacket outPkt;

//     // Feed the single fragment into parseAndReassemble
//     bool complete = parseAndReassemble(frags[0], buffer, timers, &backend, outPkt);
//     EXPECT_TRUE(complete);
//     EXPECT_EQ(outPkt.header.clientId, clientId);
//     EXPECT_EQ(outPkt.header.token, token);
//     EXPECT_EQ(outPkt.header.packetNumber, packetNumber);
//     EXPECT_EQ(outPkt.header.type, type);
//     EXPECT_EQ(outPkt.payload, payload);
// }

// TEST(net_common, large_payload_multiple_fragments_random_order)
// {
//     MockBackend backend;

//     // Construct a payload slightly larger than chunkMax so that it splits.
//     // chunkMax = MAX_PAYLOAD - HEADER_SIZE.
//     size_t chunkMax = MAX_PAYLOAD - HEADER_SIZE;
//     // Make payload length = chunkMax * 2 + 10 → 3 fragments total.
//     NetPayload payload(chunkMax * 2 + 10);
//     // Fill with some pattern.
//     for (size_t i = 0; i < payload.size(); i++)
//     {
//         payload[i] = static_cast<uint8_t>(i & 0xFF);
//     }

//     uint32_t clientId = 99, token = 0xFEEDFACE, packetNumber = 1234;
//     NetMsgType type = NetMsgType::Custom;

//     auto frags = fragmentPayload(clientId, token, type, packetNumber, &backend, payload);
//     ASSERT_EQ(frags.size(), 3u);

//     // Shuffle them so reassembly must handle out‐of‐order.
//     std::vector<NetPayload> rawFrags;
//     for (auto& f : frags)
//         rawFrags.push_back(f);
//     std::random_shuffle(rawFrags.begin(), rawFrags.end());

//     NetPacketBuffer buffer;
//     std::map<uint32_t, uint64_t> timers;
//     ParsedPacket reassembled;

//     bool complete = false;
//     for (auto& raw : rawFrags)
//     {
//         complete = parseAndReassemble(raw, buffer, timers, &backend, reassembled);
//     }

//     EXPECT_TRUE(complete);
//     EXPECT_EQ(reassembled.header.clientId, clientId);
//     EXPECT_EQ(reassembled.header.token, token);
//     EXPECT_EQ(reassembled.header.packetNumber, packetNumber);
//     EXPECT_EQ(reassembled.header.type, type);
//     EXPECT_EQ(reassembled.payload.size(), payload.size());
//     EXPECT_EQ(reassembled.payload, payload);
// }

// TEST_P(net_client_test, init_client_retries_on_failure)
// {
//     // First call → false, second call → true
//     int callCount = 0;
//     backendPtr->connectToServerFn = [&callCount]()
//     {
//         return (++callCount >= 2);
//     };

//     // At t=0, runClientFrame: connectToServer() is not called yet because no timeout
//     netSys->execute();
//     // still in ClientState::Connecting
//     EXPECT_EQ(netSys->getCurrentConnectingState(), ClientState::Connecting);

//     // Advance enough time so that initClient() tries to call connectToServer():
//     netSys->onEvent(TickEvent{1100.f}); // Simulate a tick event with deltaTime = 1100ms

//     netSys->execute();
//     // First call to connectToServer() -> returned false
//     EXPECT_EQ(netSys->getCurrentConnectingState(), ClientState::Connecting);

//     netSys->onEvent(TickEvent{1100.f}); // Simulate a tick event with deltaTime = 1100ms

//     // Next frame, it should try again, now connectToServer() returns true
//     netSys->execute();
//     EXPECT_EQ(netSys->getCurrentConnectingState(), ClientState::WaitingForId);
// }

// TEST_P(net_system_test, send_TCPMessage_fails_on_second_fragment)
// {
//     // Suppose you know fragmentPayload(...) will break your payload into 3 pieces.
//     int fragmentCalls = 0;
//     backendPtr->sendTcpSockFn = [&](SocketHandle, const NetPayload&) {
//         fragmentCalls++;
//         // fail on the second invocation
//         return (fragmentCalls != 2);
//     };

//     // Now call sendTCPMessage(...) with a large payload in NetworkSystem:
//     bool result = netSys->sendTCPMessage(
//         /*clientId=*/1,
//         /*token=*/0xdeadbeef,
//         /*type=*/NetMsgType::Custom,
//         /*payload=*/std::vector<uint8_t>(5000, 'x'),
//         /*socket=*/reinterpret_cast<SocketHandle>(1234)
//     );

//     EXPECT_FALSE(result);
//     // 2 calls to sendTcp -> it stopped on the second fragment
//     EXPECT_EQ(fragmentCalls, 2);
//     // peek at what was actually “sent”:
//     EXPECT_EQ(backendPtr->sentTcpPackets.size(), 2u);
//     // You can inspect backendPtr->sentTcpPackets[0].second, etc.
// }

// TEST_P(net_server_test, handles_incoming_ping)
// {
//     //
//     // 1) Arrange acceptTcpClient() -> fakeSock exactly once
//     //
//     SocketHandle fakeSock = reinterpret_cast<SocketHandle>(0x1234);
//     int acceptCount = 0;
//     backendPtr->acceptTcpClientFn = [&]() -> SocketHandle
//     {
//         return (++acceptCount == 1) ? fakeSock : nullptr;
//     };

//     // 2) Stub sendTcp so that the “Connect” message always succeeds
//     backendPtr->sendTcpSockFn = [](SocketHandle, const NetPayload&) {
//         return true;
//     };

//     netSys->init();

//     // 3) First execute(): server “accepts” fakeSock, assigns clientId=0, token=<some random>,
//     //    and calls sendTCPMessage(0, token, NetMsgType::Connect, {0}, fakeSock).
//     netSys->execute();

//     // 4) Now the server has created one client entry with clientId=0.
//     //    Retrieve that token from our test‐only getter:
//     uint32_t assignedClientId = 0;
//     uint32_t assignedToken    = 0x01234567;
//     ASSERT_NE(assignedToken, 0u) << "Server must have generated a non‐zero token.";

//     //
//     // 5) Build a valid Ping whose header matches (clientId=0, token=assignedToken).
//     //    We must also set header.timestamp to some value (e.g. 12345).  The payload
//     //    for Ping is zero‐length in your code (you do sendPing {0} over TCP?), but
//     //    the “timestamp” lives in header.timestamp, not in the payload.
//     //
//     PacketHeader pingHdr;
//     pingHdr.clientId  = assignedClientId;
//     pingHdr.token     = assignedToken;
//     pingHdr.type      = NetMsgType::Ping;
//     pingHdr.timestamp = 12345;  // any fake timestamp

//     // Since handleServerMessage extracts timestamp from header, no need for payload bytes.
//     NetPayload pingPacket(sizeof(PacketHeader));
//     std::memcpy(pingPacket.data(), &pingHdr, sizeof(PacketHeader));

//     // 6) Queue exactly one TcpReceiveEvent so that receiveTcp(fakeSock, …) sees our Ping:
//     backendPtr->tcpRecvQueue.emplace_back(
//         TcpReceiveEvent(fakeSock, std::move(pingPacket), /*socketClosed=*/false)
//     );

//     // 7) Second execute(): server’s runServerFrame(dt) does:
//     //       while (receiveTcp(fakeSock, data, tcpClosed)) {
//     //           parseAndReassemble(...) → msg.header == pingHdr
//     //           handleServerMessage(header, payload)  // sees a valid Ping
//     //           → sendTCPMessage(0, assignedToken, Pong, {}, fakeSock)
//     //       }
//     //
//     netSys->execute();

//     // 8) Now exactly one sendTcp(...) call must have happened (the server’s Pong).
//     ASSERT_EQ(backendPtr->sentTcpPackets.size(), 1u)
//         << "Server should have replied with exactly one Pong.";

//     auto& sentPair = backendPtr->sentTcpPackets.back();
//     EXPECT_EQ(sentPair.first, fakeSock) << "Pong must be sent back to the same TCP socket.";

//     PacketHeader respHdr;
//     std::memcpy(&respHdr, sentPair.second.data(), sizeof(PacketHeader));
//     EXPECT_EQ(respHdr.type, NetMsgType::Pong);
//     EXPECT_EQ(respHdr.clientId, assignedClientId);
//     EXPECT_EQ(respHdr.token, assignedToken);
// }

// TEST_P(net_client_test, client_ping_pong)
// {
//     //
//     // 1) arrange connectToServer() → true so that the client enters WaitingForId
//     //
//     backendPtr->connectToServerFn = []() {
//         return true;  // immediate TCP connect success
//     };

//     //
//     // 2) arrange receiveTcpNoSockFn() so that the first time it’s called
//     //    we capture “Connect” header { clientId=0, token=0xBEEF, type=Connect }.
//     //
//     bool connectPacketInjected = false;
//     backendPtr->receiveTcpNoSockFn = [&](NetPayload& out, bool& socketClosed) {
//         if (!connectPacketInjected) {
//             PacketHeader connectHdr;
//             connectHdr.clientId  = 0;
//             connectHdr.token     = 0xBEEF;
//             connectHdr.type      = NetMsgType::Connect;
//             connectHdr.timestamp = 0;     // irrelevant for Connect

//             out.resize(sizeof(PacketHeader));
//             std::memcpy(out.data(), &connectHdr, sizeof(PacketHeader));
//             socketClosed = false;
//             // connectPacketInjected = true;
//             return true;
//         }
//         return false;
//     };

//     netSys->init();
//     EXPECT_EQ(netSys->getCurrentConnectingState(), ClientState::WaitingForId);

//     //
//     // 3) First netSys->execute():  initClient() → connectToServer() → true,
//     //    so currentClientState := WaitingForId.  No “Connect” has been processed yet.
//     //
//     netSys->execute();

//     //
//     // 4) Second netSys->execute(): now receiveTcpNoSock() returns “Connect” header,
//     //    so handleClientMessage(...) sets _myClientId=0, _myToken=0xBEEF, state=Connected,
//     //    and also calls sendUdpHandshake().
//     //
//     netSys->execute();
//     EXPECT_EQ(netSys->getCurrentConnectingState(), ClientState::Connected);
//     // EXPECT_EQ(netSys->getMyClientId(), 0u);
//     // EXPECT_EQ(netSys->getMyToken(),    0xBEEF);

//     // You can also assert that one sendUdp(...) happened, but it isn’t necessary for Ping/Pong.
//     // ASSERT_EQ(backendPtr->sentUdpPackets.size(), 1u);

//     //
//     // 5) Now queue a Ping whose header = {clientId=0, token=0xBEEF, type=Ping, timestamp=44444}
//     //
//     PacketHeader pingHdr;
//     pingHdr.clientId  = 0;
//     pingHdr.token     = 0xBEEF;
//     pingHdr.type      = NetMsgType::Ping;
//     pingHdr.timestamp = 44444;

//     NetPayload pingPacket(sizeof(PacketHeader));
//     std::memcpy(pingPacket.data(), &pingHdr, sizeof(PacketHeader));

//     // Because client code uses receive(SocketHandle&, IpEndpoint&, NetPayload&),
//     // we should insert a TcpReceiveEvent with sock=nullptr (the “no‐socket” path)
//     backendPtr->tcpRecvQueue.emplace_back(
//         TcpReceiveEvent(/*sock=*/nullptr, /*payload=*/pingPacket, /*socketClosed=*/false)
//     );

//     //
//     // 6) Third netSys->execute(): runClientFrame() will do:
//     //      while (receive(sock, srcUdp, buf)) {  // the Ping we just queued
//     //          parseAndReassemble(...) → msg.header = pingHdr
//     //          handleClientMessage(...) sees Ping, tokens match → sendTCPMessage(0,0xBEEF,Pong, …)
//     //      }
//     //
//     netSys->execute();

//     // 7) Now exactly one sendTcp(...) (no‐sock overload) should have happened:
//     ASSERT_EQ(backendPtr->sentTcpNoSockPackets.size(), 1u)
//         << "Client should have sent exactly one Pong.";

//     const NetPayload& sentBack = backendPtr->sentTcpNoSockPackets.back();
//     PacketHeader respHdr;
//     std::memcpy(&respHdr, sentBack.data(), sizeof(PacketHeader));
//     EXPECT_EQ(respHdr.type, NetMsgType::Pong);
//     EXPECT_EQ(respHdr.clientId, 0u);
//     EXPECT_EQ(respHdr.token,    0xBEEF);
//     EXPECT_EQ(respHdr.timestamp, 44444);
// }

// TEST_P(net_system_test, handles_incoming_udp)
// {
//     IpEndpoint fakeSrc("127.0.0.1", 7777);
//     PacketHeader hdr;
//     hdr.clientId = 5;
//     hdr.token    = 0xCAFEBABE;
//     hdr.type     = NetMsgType::Custom;

//     std::vector<uint8_t> payload{1, 2, 3, 4, 5};

//     NetPayload whole;
//     whole.resize(sizeof(PacketHeader) + payload.size());
//     std::memcpy(whole.data(), &hdr, sizeof(PacketHeader));
//     std::memcpy(whole.data() + sizeof(PacketHeader),
//                 payload.data(),
//                 payload.size());

//     backendPtr->udpRecvQueue.push_back(UdpReceiveEvent(fakeSrc, whole));

//     netSys->execute();

//     // Todo
//     // Assuming handleServerMessage(...) for CustomData invokes handleMessage(...)
//     // and handleMessage(...) queue it or calls a callback.  You can inspect
//     // internal state or side‐effects here.
// }

// INSTANTIATE_TEST_SUITE_P(
//     ServConfig,                     // this is just a user‐chosen prefix
//     net_server_test,                // the fixture name
//     ::testing::ValuesIn(AllConfigsForServerSystems())
//     // name_for_cfg                    // optional lambda for nicer names
// );

// INSTANTIATE_TEST_SUITE_P(
//     ClientConfig,                   // this is just a user‐chosen prefix
//     net_client_test,                // the fixture name
//     ::testing::ValuesIn(AllConfigsForClientSystems())
//     // name_for_cfg                    // optional lambda for nicer names
// );

// auto name_for_both_cfg = [](const ::testing::TestParamInfo<NetworkConfig>& info)
// {
//     const NetworkConfig& cfg = info.param;
//     std::ostringstream os;
//     os << "Both_" << (cfg.isServer ? "Server" : "Client") <<
//         "_SSS" << cfg.defaultSystemFlags.socketSetSize;

//     return os.str();
// };

// INSTANTIATE_TEST_SUITE_P(
//     VariousConfigs,                 // this is just a user‐chosen prefix
//     net_system_test,                // the fixture name
//     ::testing::ValuesIn(AllConfigsForNetSystems())
//     // name_for_both_cfg               // optional lambda for nicer names
// );
