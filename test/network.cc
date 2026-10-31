#include "stdafx.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <random>
#include <thread>
#include <unistd.h>

#include "Networking/backend_sdl.h"
#include "Networking/common.h"
#include "Networking/network_system.h"

#include "mocknetworking.h"

namespace pg
{
    namespace test
    {
        namespace
        {
            NetworkConfig clientConfig(uint16_t tcp, uint16_t udp)
            {
                NetworkConfig c;
                c.isServer = false;
                c.peerAddress = "127.0.0.1";
                c.tcpPort = tcp;
                c.udpPeerPort = udp;
                c.udpLocalPort = 0;
                c.emitEvents = false;
                return c;
            }

            NetworkConfig serverConfig(uint16_t tcp, uint16_t udp)
            {
                NetworkConfig c;
                c.isServer = true;
                c.tcpPort = tcp;
                c.udpLocalPort = udp;
                c.emitEvents = false;
                return c;
            }

            // Ports of their own per test run, away from the usual ones
            uint16_t basePort()
            {
                return static_cast<uint16_t>(41000 + (getpid() % 4000) * 2);
            }

            NetPayload bytes(size_t n, uint8_t seed)
            {
                NetPayload p(n);
                for (size_t i = 0; i < n; ++i)
                    p[i] = static_cast<uint8_t>(seed + i * 31);
                return p;
            }

            /** A server whose UDP may vanish on the way, as a router on the internet can make it. */
            struct LossyBackend : public SdlNetworkBackend
            {
                using SdlNetworkBackend::SdlNetworkBackend;

                bool dropUdp = false;

                bool sendUdp(const IpEndpoint& dest, const NetPayload& data) override
                {
                    return dropUdp ? true : SdlNetworkBackend::sendUdp(dest, data);
                }
            };

            /** Steps both systems until pred is true or the time is up. */
            template <typename Pred>
            bool runUntil(NetworkSystem& a, NetworkSystem& b, Pred pred, int ms = 3000)
            {
                const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);

                while (std::chrono::steady_clock::now() < end)
                {
                    a.execute();
                    b.execute();

                    if (pred())
                        return true;

                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

                return false;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------

        TEST(network_fragments, messages_of_any_size_come_back_whole_in_any_order)
        {
            MockBackend backend;

            for (size_t size : {size_t(0), size_t(1), size_t(4069), size_t(4070), size_t(20000)})
            {
                const NetPayload payload = bytes(size, static_cast<uint8_t>(size));
                auto frags = fragmentPayload(3, 77, NetMsgType::Custom, 9, &backend, payload);

                EXPECT_GE(frags.size(), 1u);
                std::shuffle(frags.begin(), frags.end(), std::mt19937(static_cast<uint32_t>(size)));

                NetPacketBuffer buffer;
                NetPacketTimers timers;
                ParsedPacket out;
                int completed = 0;

                for (const auto& f : frags)
                    completed += parseAndReassemble(f, buffer, timers, &backend, out);

                EXPECT_EQ(completed, 1) << size;
                EXPECT_EQ(out.payload, payload) << size;
                EXPECT_EQ(out.header.clientId, 3u);
                EXPECT_EQ(out.header.token, 77u);
                EXPECT_TRUE(buffer.empty());
                EXPECT_TRUE(timers.empty());
            }
        }

        TEST(network_fragments, broken_fragments_take_no_room_and_stale_ones_are_dropped)
        {
            MockBackend backend;
            NetPacketBuffer buffer;
            NetPacketTimers timers;
            ParsedPacket out;

            // Too short, or a fragment index past the count
            EXPECT_FALSE(parseAndReassemble(NetPayload(5), buffer, timers, &backend, out));
            auto frags = fragmentPayload(1, 2, NetMsgType::Custom, 3, &backend, bytes(9000, 1));
            auto bad = frags[0];
            backend.writeU16BE(bad.data() + 23, 50);
            EXPECT_FALSE(parseAndReassemble(bad, buffer, timers, &backend, out));
            EXPECT_TRUE(buffer.empty());

            // One fragment of three arrives, the rest never: dropped after the timeout, safely
            EXPECT_FALSE(parseAndReassemble(frags[0], buffer, timers, &backend, out));
            auto other = fragmentPayload(1, 2, NetMsgType::Custom, 4, &backend, bytes(9000, 2));
            EXPECT_FALSE(parseAndReassemble(other[1], buffer, timers, &backend, out));
            EXPECT_EQ(buffer.size(), 2u);

            EXPECT_EQ(dropStaleFragments(buffer, timers, steadyMs(), 60000), 0u);
            EXPECT_EQ(dropStaleFragments(buffer, timers, steadyMs() + 120000, 60000), 2u);
            EXPECT_TRUE(buffer.empty());
            EXPECT_TRUE(timers.empty());
        }

        TEST(network_sockets, a_client_joins_talks_both_ways_and_leaves)
        {
            const uint16_t tcp = basePort(), udp = static_cast<uint16_t>(basePort() + 1);

            NetworkSystem server(new SdlNetworkBackend(serverConfig(tcp, udp)), serverConfig(tcp, udp));
            NetworkSystem client(new SdlNetworkBackend(clientConfig(tcp, udp)), clientConfig(tcp, udp));
            server.init();
            client.init();

            std::vector<NetIncoming> atServer, atClient;
            auto drain = [&]() {
                NetIncoming in;
                while (server.poll(in))
                    atServer.push_back(in);
                while (client.poll(in))
                    atClient.push_back(in);
            };

            // Joined: both sides told
            ASSERT_TRUE(runUntil(server, client, [&] {
                drain();
                return not atServer.empty() and not atClient.empty();
            })) << "no connection on localhost:" << tcp;

            EXPECT_EQ(atServer[0].kind, NetIncoming::Kind::Connected);
            EXPECT_EQ(atClient[0].kind, NetIncoming::Kind::Connected);
            const uint32_t id = atServer[0].clientId;
            EXPECT_EQ(client.myClientId(), id);
            atServer.clear();
            atClient.clear();

            // Many reliable messages back to back (TCP coalesces them: the framing must cut them apart), and a big one
            for (int i = 0; i < 50; ++i)
                client.sendToServer(bytes(10 + i, static_cast<uint8_t>(i)), true);
            client.sendToServer(bytes(200000, 7), true);
            server.send(id, bytes(3000, 9), true);

            ASSERT_TRUE(runUntil(server, client, [&] {
                drain();
                return atServer.size() >= 51 and atClient.size() >= 1;
            }));

            for (int i = 0; i < 50; ++i)
            {
                EXPECT_EQ(atServer[i].data, bytes(10 + i, static_cast<uint8_t>(i))) << i;
                EXPECT_TRUE(atServer[i].reliable);
            }
            EXPECT_EQ(atServer[50].data, bytes(200000, 7));
            EXPECT_EQ(atClient[0].data, bytes(3000, 9));
            atServer.clear();
            atClient.clear();

            // Unreliable: over UDP once linked (the client offers its address, the server confirms)
            ASSERT_TRUE(runUntil(server, client, [&] {
                client.sendToServer(bytes(100, 3), false);
                server.send(id, bytes(100, 4), false);
                drain();

                const bool up = std::any_of(atServer.begin(), atServer.end(), [](const NetIncoming& m) { return not m.reliable; });
                const bool down = std::any_of(atClient.begin(), atClient.end(), [](const NetIncoming& m) { return not m.reliable; });
                return up and down;
            }));

            // The ping gives a round trip
            ASSERT_TRUE(runUntil(server, client, [&] { return server.rttMs(id) > 0.0f; }, 2500));

            // Leaving: the server is told
            atServer.clear();
            client.disconnect();

            ASSERT_TRUE(runUntil(server, client, [&] {
                drain();
                return std::any_of(atServer.begin(), atServer.end(), [](const NetIncoming& m) { return m.kind == NetIncoming::Kind::Disconnected; });
            }));

            EXPECT_EQ(client.getCurrentConnectingState(), ClientState::Disconnected);
            EXPECT_FALSE(server.clientExists(id));
        }

        TEST(network_fragments, a_udp_message_goes_in_datagrams_that_cross_the_internet_whole)
        {
            MockBackend backend;
            const NetPayload big = bytes(5000, 5);

            const auto frags = fragmentPayload(1, 2, NetMsgType::Custom, 3, &backend, big, MAX_UDP_DATAGRAM);
            EXPECT_GT(frags.size(), 1u);

            NetPacketBuffer buffer;
            NetPacketTimers timers;
            ParsedPacket out;
            bool whole = false;

            for (const auto& f : frags)
            {
                EXPECT_LE(f.size(), MAX_UDP_DATAGRAM);
                whole = parseAndReassemble(f, buffer, timers, &backend, out);
            }

            ASSERT_TRUE(whole);
            EXPECT_EQ(out.payload, big);
        }

        TEST(network_sockets, when_the_servers_udp_never_arrives_its_messages_come_over_tcp)
        {
            const uint16_t tcp = static_cast<uint16_t>(basePort() + 20), udp = static_cast<uint16_t>(tcp + 1);

            auto* lossy = new LossyBackend(serverConfig(tcp, udp));
            lossy->dropUdp = true;

            NetworkSystem server(lossy, serverConfig(tcp, udp));
            NetworkSystem client(new SdlNetworkBackend(clientConfig(tcp, udp)), clientConfig(tcp, udp));
            server.init();
            client.init();

            uint32_t id = 0;
            NetIncoming in;

            ASSERT_TRUE(runUntil(server, client, [&] {
                while (server.poll(in))
                    if (in.kind == NetIncoming::Kind::Connected)
                        id = in.clientId;
                return id != 0;
            }));

            // Time for handshakes: the client's reach the server, and no answer comes back
            runUntil(server, client, [] { return false; }, 700);

            while (client.poll(in)) {}

            int arrived = 0;

            ASSERT_TRUE(runUntil(server, client, [&] {
                server.send(id, bytes(100, 4), false);

                while (client.poll(in))
                    if (in.kind == NetIncoming::Kind::Message)
                    {
                        EXPECT_TRUE(in.reliable);
                        ++arrived;
                    }

                return arrived >= 5;
            }));
        }

        TEST(network_sockets, when_udp_goes_silent_tcp_takes_over_and_udp_comes_back_later)
        {
            const uint16_t tcp = static_cast<uint16_t>(basePort() + 40), udp = static_cast<uint16_t>(tcp + 1);

            NetworkConfig clientCfg = clientConfig(tcp, udp);
            clientCfg.udpSilenceMs = 200;

            auto* lossy = new LossyBackend(serverConfig(tcp, udp));

            NetworkSystem server(lossy, serverConfig(tcp, udp));
            NetworkSystem client(new SdlNetworkBackend(clientCfg), clientCfg);
            server.init();
            client.init();

            uint32_t id = 0;
            NetIncoming in;

            ASSERT_TRUE(runUntil(server, client, [&] {
                while (server.poll(in))
                    if (in.kind == NetIncoming::Kind::Connected)
                        id = in.clientId;
                return id != 0;
            }));

            // The server talks all the time; this waits for a message that came the given way
            auto arrivesBy = [&](bool reliable, int ms) {
                return runUntil(server, client, [&] {
                    server.send(id, bytes(5000, 4), false);   // more than a datagram: it has to come back whole

                    bool got = false;

                    while (client.poll(in))
                        got = got or (in.kind == NetIncoming::Kind::Message and in.reliable == reliable and in.data == bytes(5000, 4));

                    return got;
                }, ms);
            };

            ASSERT_TRUE(arrivesBy(false, 3000)) << "never linked over UDP";

            lossy->dropUdp = true;
            EXPECT_TRUE(arrivesBy(true, 2000)) << "no fallback to TCP";

            lossy->dropUdp = false;
            EXPECT_TRUE(arrivesBy(false, 8000)) << "UDP never tried again";
        }

        TEST(network_sockets, a_client_with_nobody_to_talk_to_does_not_block)
        {
            const uint16_t tcp = static_cast<uint16_t>(basePort() + 2);
            NetworkConfig cfg = clientConfig(tcp, static_cast<uint16_t>(tcp + 1));
            cfg.autoReconnect = false;

            NetworkSystem client(new SdlNetworkBackend(cfg), cfg);
            client.init();

            // Every frame returns at once, whatever the connect is doing
            const auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < 200 and client.getCurrentConnectingState() != ClientState::Disconnected; ++i)
            {
                const auto frame = std::chrono::steady_clock::now();
                client.execute();
                const double took = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - frame).count();
                EXPECT_LT(took, 50.0);
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }

            EXPECT_EQ(client.getCurrentConnectingState(), ClientState::Disconnected);   // refused, and told to give up
            (void)start;
        }
    }
}
