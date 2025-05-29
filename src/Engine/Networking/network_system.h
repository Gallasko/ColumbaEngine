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
        std::unordered_map<TCPsocket,ClientInfo> clients;
        std::unordered_map<uint32_t,TCPsocket>   idToTcp;
        std::unordered_map<std::string,uint32_t> _udpClientMap;
        uint32_t nextClientId = 1;

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
            // build packet
            std::vector<uint8_t> data(sizeof(UdpHeader));
            UdpHeader h{_myClientId, _myToken, 0};

            writeHeader(data.data(), h);

            // send with zero‐length payload
            IPaddress dest{};
            SDLNet_ResolveHost(&dest, netCfg.peerAddress.c_str(), netCfg.udpPeerPort);

            backend->sendUdp(dest, data);
            LOG_INFO("NetSys", "UDP handshake sent");
        }

        void sendToServer(const std::vector<uint8_t>& data, bool overTcp)
        {
            if (overTcp)
                backend->sendTcp(clients[idToTcp[_myClientId]].tcpSock, data);
            else
            {
                std::vector<uint8_t> data(sizeof(UdpHeader));
                UdpHeader h{_myClientId, _myToken, 0};

                writeHeader(data.data(), h);

                // send with zero‐length payload
                IPaddress dest{};
                SDLNet_ResolveHost(&dest, netCfg.peerAddress.c_str(), netCfg.udpPeerPort);

                backend->sendUdp(dest, data);
            }
                // backend->sendUdp(clients[idToTcp[_myClientId]].udpAddr, data);
        }

        // ----- Per-frame logic -----
        void runServerFrame()
        {
            // 1) Accept new TCP clients
            if (auto newSock = backend->acceptTcpClient())
            {
                ClientInfo ci;
                ci.tcpSock  = newSock;
                ci.clientId = nextClientId++;
                ci.token    = genToken();

                clients[newSock] = ci;
                idToTcp[ci.clientId] = newSock;

                // send welcome
                std::vector<uint8_t> w(8);

                SDLNet_Write32(ci.clientId, w.data());
                SDLNet_Write32(ci.token,    w.data() + 4);

                if (backend->sendTcp(newSock, w))
                {
                    LOG_INFO("NetSys", "Sent client Id and Token to the client: " << ci.clientId << " " << ci.token);
                }

                LOG_INFO("NetSys", "New client " << ci.clientId);
            }

            // 2) Process incoming packets
            TCPsocket  sock;
            IPaddress  udpSrc;
            std::vector<uint8_t> pkt;

            while (backend->receive(sock, udpSrc, pkt))
            {
                LOG_INFO("NetSys", "Got Packet");

                if (sock)
                {
                    // handle any future TCP messages…

                    LOG_INFO("NetSys", "Got TCP packet from a client");
                }
                else
                {

                    // UDP packet => header + optional payload
                    if (pkt.size() < sizeof(UdpHeader))
                        continue;

                    UdpHeader h = readHeader(pkt.data());

                    LOG_INFO("NetSys", "Got UDP packet from client " << h.clientId);

                    auto it = idToTcp.find(h.clientId);
                    if (it == idToTcp.end())
                        continue;

                    auto& ci = clients[it->second];
                    if (ci.token != h.token)
                        continue;

                    // first‐time UDP link?
                    if (not ci.udpLinked)
                    {
                        ci.udpLinked = true;
                        ci.udpAddr   = udpSrc;

                        auto key = ipPortKey(udpSrc);
                        _udpClientMap[key] = ci.clientId;

                        LOG_INFO("NetSys", "Linked UDP for client " << ci.clientId);
                    }

                    // echo back header
                    std::vector<uint8_t> out(sizeof(UdpHeader) + h.payloadLen);
                    UdpHeader oh{ci.clientId, ci.token, h.payloadLen};
                    writeHeader(out.data(), oh);

                    // copy payload if any
                    if (h.payloadLen>0)
                        std::copy(pkt.begin() + sizeof(UdpHeader), pkt.end(), out.begin() + sizeof(UdpHeader));

                    backend->sendUdp(ci.udpAddr, out);
                }
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

            TCPsocket sock;
            IPaddress udpSrc;
            std::vector<uint8_t> pkt;

            // backend->receive returns one packet at a time
            while (backend->receive(sock, udpSrc, pkt))
            {
                if (sock != nullptr)
                {
                    // Data on TCP (could be a reconnect drop, server message, etc.)
                    // For now, we only expected the initial welcome, so ignore further TCP data
                }
                else
                {
                    // UDP packet
                    if (pkt.size() < sizeof(UdpHeader))
                        return;

                    UdpHeader h = readHeader(pkt.data());

                    if (h.clientId == _myClientId and h.token == _myToken)
                    {
                        // LOG_INFO("NetSys", "Received UDP echo from server" << int(h.payloadLen) << ". " << pkt.data() + sizeof(UdpHeader));
                        SDL_Log("CLIENT echo: %.*s", int(h.payloadLen), pkt.data() + sizeof(UdpHeader));
                    }
                }
            }
        }
    };

}