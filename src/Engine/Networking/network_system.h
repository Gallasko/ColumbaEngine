#pragma once

#include "Networking/network_config.h"
#include "Networking/network_sys_config.h"
#include "Networking/common.h"
#include "Networking/ibackend.h"

#include "Systems/coresystems.h"

namespace pg
{
    struct ClientInfo
    {
        size_t       tcpSetID   = 0;
        TCPsocket    tcpSock    = nullptr;
        uint32_t     clientId   = 0;
        uint32_t     token      = 0;
        bool         udpLinked  = false;
        IPaddress    udpAddr{};
    };

    struct SendDataToServer
    {
        std::vector<uint8_t> data;
        bool inTcp = false; // true = send over TCP, false = send over UDP
    };

    struct NetworkSystem : public System<InitSys, Listener<TickEvent>, Listener<SendDataToServer>>
    {
        virtual void onEvent(const TickEvent& e) override
        {
            deltaTime += e.tick;
        }

        virtual void onEvent(const SendDataToServer& e) override
        {
            sendToServer(e.data, e.inTcp);
        }

        NetworkSystem(INetworkBackend* backend, const NetworkConfig& cfg) : backend(backend), netCfg(cfg)
        {

        }

        virtual ~NetworkSystem() override
        {
            if (backend)
                delete backend;
        }

        virtual void init() override
        {
            if (netCfg.isServer)
                initServer();
            else
                initClient();
        }

        virtual void execute() override
        {
            if (netCfg.isServer)
                runServerFrame();
            else
                runClientFrame();

            deltaTime = 0.0f;
        }

    private:
        INetworkBackend* backend;
        NetworkConfig    netCfg;

        float deltaTime = 0.0f;

        // Server state
        std::unordered_map<TCPsocket, ClientInfo> clients;
        std::unordered_map<uint32_t, TCPsocket>   idToTcp;
        std::unordered_map<std::string, uint32_t> _udpClientMap;

        std::vector<SDLNet_SocketSet>             sockSets;
        uint32_t nextClientId = 0;

        // Client state
        uint32_t _myClientId = 0;
        uint32_t _myToken    = 0;
        bool     clientConnected  = false;
        float    timeSinceFail = 0.0f;
        bool waitingForServerId = false;

        // Utilities
        uint32_t genToken()
        {
            static std::mt19937 rng{ std::random_device{}() };
            return rng();
        }

        std::string ipPortKey(const IPaddress& addr)
        {
            Uint32 rawIP   = SDLNet_Read32(reinterpret_cast<const Uint8*>(&addr.host));
            Uint16 rawPort = SDLNet_Read16(reinterpret_cast<const Uint8*>(&addr.port));

            Uint8 b0 = (rawIP >> 24) & 0xFF;
            Uint8 b1 = (rawIP >> 16) & 0xFF;
            Uint8 b2 = (rawIP >>  8) & 0xFF;
            Uint8 b3 = (rawIP      ) & 0xFF;

            return std::to_string(b0) + "." + std::to_string(b1) + "." +
                   std::to_string(b2) + "." + std::to_string(b3) + ":" + std::to_string(rawPort);
        }

        // ----- Initialization -----
        void initServer()
        {
            // nothing to do here: backend ctor did the listening
            LOG_INFO("NetSys", "Server ready (TCP:" << netCfg.tcpPort << " UDP:" << netCfg.udpLocalPort << ")");
        }

        void initClient()
        {
            // attempt TCP connect, with simple back-off
            if (not clientConnected)
            {
                if (backend->connectToServer())
                {
                    clientConnected = true;
                    LOG_INFO("NetSys", "TCP connected to " << netCfg.peerAddress << ":" << netCfg.tcpPort);
                }
                else
                {
                    LOG_WARNING("NetSys", "TCP connect failed, retry in 1s");
                    timeSinceFail = 0.0f;
                    return;
                }
            }

            waitingForServerId = true;
        }

        void sendUdpHandshake()
        {
            // send with zero‐length payload
            IPaddress dest{};
            SDLNet_ResolveHost(&dest, netCfg.peerAddress.c_str(), netCfg.udpPeerPort);

            sendUDPMessage(_myClientId, _myToken, NetMsgType::Handshake, {0}, dest);
            LOG_INFO("NetSys", "UDP handshake sent");
        }

        void sendToServer(const std::vector<uint8_t>& data, bool overTcp)
        {
            if (overTcp)
            {
                sendTCPMessage(_myClientId, _myToken, NetMsgType::Custom, data);
            }
            else
            {
                // send with zero‐length payload
                IPaddress dest{};
                SDLNet_ResolveHost(&dest, netCfg.peerAddress.c_str(), netCfg.udpPeerPort);

                sendUDPMessage(_myClientId, _myToken, NetMsgType::Custom, data, dest);
            }
        }

        size_t nextPacketNumber = 0;

        bool sendTCPMessage(uint32_t clientId, uint32_t token, NetMsgType type,
            const NetPayload& payload, TCPsocket tcpSock = nullptr)
        {
            auto frags = fragmentPayload(clientId, token, type,
                            nextPacketNumber++, payload);

            bool sent = true;

            for (auto& pkt : frags)
            {
                if (not tcpSock)
                    sent = backend->sendTcp(pkt);
                else
                    sent = backend->sendTcp(tcpSock, pkt);

                if (not sent)
                    break;
            }

            return sent;
        }

        bool sendUDPMessage(uint32_t clientId, uint32_t token, NetMsgType type,
            const NetPayload& payload, const IPaddress& udpDest)
        {
            auto frags = fragmentPayload(clientId, token, type,
                            nextPacketNumber++, payload);

            bool sent = true;

            for (auto& pkt : frags)
            {
                sent = backend->sendUdp(udpDest, pkt);

                if (not sent)
                    break;
            }

            return sent;
        }

        NetPacketBuffer reassembly;

        void readData()
        {
            TCPsocket sock;
            IPaddress udpSrc; // ignored for TCP
            std::vector<uint8_t> buf;

            while (backend->receive(sock, udpSrc, buf))
            {
                ParsedPacket msg;
                if (parseAndReassemble(buf, reassembly, msg))
                {
                    // msg.header.clientId, msg.header.token,
                    // msg.header.packetNumber, msg.header.timestamp,
                    // msg.header.type, msg.payload

                    LOG_INFO("NetSys", "Received packet from client: "
                        << msg.header.clientId << " type: " << int(msg.header.type)
                        << " payload size: " << msg.payload.size());

                    // handleMessage(msg.header, msg.payload);
                }
            }
        }

        // ----- Per-frame logic -----
        void runServerFrame()
        {
            // 1) Accept new TCP clients
            if (auto newSock = backend->acceptTcpClient())
            {
                size_t setId = std::floor(nextClientId / netCfg.defaultSystemFlags.socketSetSize);

                if (setId >= sockSets.size())
                {
                    auto newSet = SDLNet_AllocSocketSet(netCfg.defaultSystemFlags.socketSetSize);

                    if (newSet)
                    {
                        sockSets.push_back(newSet);
                        LOG_INFO("NetSys", "Created socket set " << sockSets.size() - 1);
                    }
                    else
                    {
                        LOG_WARNING("NetSys", "Failed to create socket set");
                        return;
                    }
                }

                ClientInfo ci;
                ci.tcpSetID = setId;
                ci.tcpSock  = newSock;
                ci.clientId = nextClientId++;
                ci.token    = genToken();

                clients[newSock] = ci;
                idToTcp[ci.clientId] = newSock;

                LOG_INFO("NetSys", "Adding new socket to socket set: " << setId);

                SDLNet_TCP_AddSocket(sockSets[setId], newSock);

                // send welcome

                if (sendTCPMessage(ci.clientId, ci.token, NetMsgType::Connect, {0}))
                {
                    LOG_INFO("NetSys", "Sent client Id and Token to the client: " << ci.clientId << " " << ci.token);
                }

                LOG_INFO("NetSys", "New client " << ci.clientId);
            }

            // 2) Process incoming packets
            // Process Tcp data
            for (const auto& sockSet : sockSets)
            {
                SDLNet_CheckSockets(sockSet, 0);
            }

            for (auto& client : clients)
            {
                auto& ci = client.second;
                auto& set = sockSets.at(ci.tcpSetID);

                std::vector<uint8_t> data;
                bool tcpClosed = false; // Todo need to use this

                while (backend->receiveTcp(ci.tcpSock, set, data, tcpClosed))
                {
                    LOG_INFO("NetSys", "Received TCP request from client: " << ci.clientId);

                    // ParsedPacket pkt;

                    ParsedPacket msg;
                    if (parseAndReassemble(data, reassembly, msg))
                    {
                        LOG_INFO("NetSys", "Parsed packet from client:" << ci.clientId << " that should match header: " << msg.header.clientId);

                        std::string str(msg.payload.begin(), msg.payload.end());

                        LOG_INFO("NetSys", "Received over tcp: " << str);
                    }

                    // if (parsePacket(data, pkt))
                    // {
                    //     LOG_INFO("NetSys", "Parsed packet from client: " << ci.clientId);
                    //     // Todo here check if the pkt token is correct

                    //     std::string str(pkt.payload.begin(), pkt.payload.end());

                    //     LOG_INFO("NetSys", "Received over tcp: " << str);
                    // }
                    // else
                    // {
                    //     LOG_WARNING("NetSys", "Failed to parse packet");
                    // }
                }
            }

            // Process Udp data
            IPaddress ip;
            std::vector<uint8_t> data;

            while (backend->receiveUdp(ip, data))
            {
                ParsedPacket msg;
                if (parseAndReassemble(data, reassembly, msg))
                {
                    LOG_INFO("NetSys", "Parsed packet from client:"  << msg.header.clientId);

                    std::string str(msg.payload.begin(), msg.payload.end());

                    LOG_INFO("NetSys", "Received over udp: " << str);
                }
                // LOG_INFO("NetSys", "Received UDP request from ip: " << ipPortKey(ip));
            }
        }

        void runClientFrame()
        {
            if (not clientConnected)
            {
                timeSinceFail += deltaTime;

                if (timeSinceFail > 1000.0f)
                    initClient();

                return;
            }

            if (waitingForServerId)
            {
                // LOG_INFO("NetSys", "Waiting for server id");

                // receive welcome <id, token> over TCP
                TCPsocket sock;
                IPaddress udpSrc; // ignored for TCP
                std::vector<uint8_t> buf;

                while (backend->receive(sock, udpSrc, buf))
                {
                    LOG_INFO("NetSys", "Received packet for server");
                    if (sock != nullptr and buf.size() >= 8)
                    {
                        _myClientId = SDLNet_Read32(buf.data());
                        _myToken    = SDLNet_Read32(buf.data() + 4);

                        waitingForServerId = false;

                        LOG_INFO("NetSys", "Received id=" << _myClientId << " token=" << _myToken);

                        // Send the UDP handshake now that we have id + token
                        sendUdpHandshake();
                    }
                }
            }

            if (waitingForServerId)
                return;

            // TCPsocket sock;
            // IPaddress udpSrc;
            // std::vector<uint8_t> pkt;

            // backend->receive returns one packet at a time

            readData();
            // while (backend->receive(sock, udpSrc, pkt))
            // {
            //     if (sock != nullptr)
            //     {
            //         // Data on TCP (could be a reconnect drop, server message, etc.)
            //         // For now, we only expected the initial welcome, so ignore further TCP data
            //     }
            //     else
            //     {
            //         // UDP packet
            //         if (pkt.size() < sizeof(UdpHeader))
            //             return;

            //         UdpHeader h = readHeader(pkt.data());

            //         if (h.clientId == _myClientId and h.token == _myToken)
            //         {
            //             // LOG_INFO("NetSys", "Received UDP echo from server" << int(h.payloadLen) << ". " << pkt.data() + sizeof(UdpHeader));
            //             SDL_Log("CLIENT echo: %.*s", int(h.payloadLen), pkt.data() + sizeof(UdpHeader));
            //         }
            //     }
            // }

            clientConnected = backend->isConnectedToServer();
        }
    };

}