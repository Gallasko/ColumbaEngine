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


    struct NetworkSystem : public System<InitSys, Listener<TickEvent>>
    {
        virtual void onEvent(const TickEvent& e) override
        {
            _deltaTime += e.tick;
        }

        NetworkSystem(INetworkBackend* backend, const NetworkConfig& cfg)
            : _backend(backend), _cfg(cfg)
        {}

        virtual ~NetworkSystem() override
         {
            if (_backend)
                delete _backend;
        }


        virtual void init() override {
            if (_cfg.isServer) initServer();
            else               initClient();
        }

        virtual void execute() override {
            if (_cfg.isServer) runServerFrame();
            else               runClientFrame();
            _deltaTime = 0.0f;
        }

    private:
        INetworkBackend*                _backend;
        NetworkConfig                   _cfg;
        float                           _deltaTime = 0.0f;

        // Server state
        std::unordered_map<TCPsocket,ClientInfo> _clients;
        std::unordered_map<uint32_t,TCPsocket>   _idToTcp;
        std::unordered_map<std::string,uint32_t> _udpClientMap;
        uint32_t                              _nextClientId = 1;

        // Client state
        uint32_t _myClientId = 0;
        uint32_t _myToken    = 0;
        bool     _connected  = false;
        float    _timeSinceFail = 0.0f;

        // Utilities
        uint32_t genToken() {
            static std::mt19937 rng{ std::random_device{}() };
            return rng();
        }
        std::string ipPortKey(const IPaddress& addr) {
            Uint32 rawIP   = SDLNet_Read32(reinterpret_cast<const Uint8*>(&addr.host));
            Uint16 rawPort = SDLNet_Read16(reinterpret_cast<const Uint8*>(&addr.port));
            Uint8 b0 = (rawIP >> 24) & 0xFF;
            Uint8 b1 = (rawIP >> 16) & 0xFF;
            Uint8 b2 = (rawIP >>  8) & 0xFF;
            Uint8 b3 = (rawIP      ) & 0xFF;
            return std::to_string(b0)+"."+std::to_string(b1)+"."+
                    std::to_string(b2)+"."+std::to_string(b3)+":"+
                    std::to_string(rawPort);
        }

        // ----- Initialization -----
        void initServer() {
            // nothing to do here: backend ctor did the listening
            LOG_INFO("NetSys", "Server ready (TCP:" << _cfg.tcpPort
                        << " UDP:" << _cfg.udpLocalPort << ")");
        }

        void initClient() {
            // attempt TCP connect, with simple back-off
            if (!_connected) {
                if (_backend->connectToServer()) {
                    _connected = true;
                    LOG_INFO("NetSys", "TCP connected to "
                                << _cfg.peerAddress << ":" << _cfg.tcpPort);
                } else {
                    LOG_WARNING("NetSys", "TCP connect failed, retry in 1s");
                    _timeSinceFail = 0.0f;
                    return;
                }
            }

            // receive welcome <id,token> over TCP
            TCPsocket sock;
            IPaddress udpSrc;               // ignored for TCP
            std::vector<uint8_t> buf;
            if (_backend->receive(sock, udpSrc, buf) && sock != nullptr) {
                // We got data on our TCP socket
                if (buf.size() >= 8) {
                    _myClientId = SDLNet_Read32(buf.data());
                    _myToken    = SDLNet_Read32(buf.data()+4);
                    LOG_INFO("NetSys", "Received id=" << _myClientId
                            << " token=" << _myToken);

                    // Send the UDP handshake now that we have id+token
                    sendUdpHandshake();
                }
            }
        }

        void sendUdpHandshake() {
            // build packet
            std::vector<uint8_t> data(sizeof(UdpHeader));
            UdpHeader h{_myClientId, _myToken, 0};
            writeHeader(data.data(), h);
            // send with zero‐length payload
            IPaddress dest{};
            SDLNet_ResolveHost(&dest,
                _cfg.peerAddress.c_str(),
                _cfg.udpPeerPort);
            _backend->sendUdp(dest, data);
            LOG_INFO("NetSys", "UDP handshake sent");
        }

        // ----- Per-frame logic -----
        void runServerFrame() {
            // 1) Accept new TCP clients
            if (auto newSock = _backend->acceptTcpClient()) {
                ClientInfo ci;
                ci.tcpSock  = newSock;
                ci.clientId = _nextClientId++;
                ci.token    = genToken();
                _clients[newSock] = ci;
                _idToTcp[ci.clientId] = newSock;
                // send welcome
                std::vector<uint8_t> w(8);
                SDLNet_Write32(ci.clientId, w.data());
                SDLNet_Write32(ci.token,    w.data()+4);
                _backend->sendTcp(newSock, w);
                LOG_INFO("NetSys", "New client " << ci.clientId);
            }

            // 2) Process incoming packets
            TCPsocket  sock;
            IPaddress  udpSrc;
            std::vector<uint8_t> pkt;
            while (_backend->receive(sock, udpSrc, pkt)) {
                if (sock) {
                    // handle any future TCP messages…
                } else {
                    // UDP packet => header + optional payload
                    if (pkt.size() < sizeof(UdpHeader)) continue;
                    UdpHeader h = readHeader(pkt.data());
                    auto it = _idToTcp.find(h.clientId);
                    if (it==_idToTcp.end()) continue;
                    auto& ci = _clients[it->second];
                    if (ci.token != h.token) continue;
                    // first‐time UDP link?
                    if (!ci.udpLinked) {
                        ci.udpLinked = true;
                        ci.udpAddr   = udpSrc;
                        auto key = ipPortKey(udpSrc);
                        _udpClientMap[key] = ci.clientId;
                        LOG_INFO("NetSys","Linked UDP for client "<<ci.clientId);
                    }
                    // echo back header
                    std::vector<uint8_t> out(sizeof(UdpHeader)+h.payloadLen);
                    UdpHeader oh{ci.clientId,ci.token,h.payloadLen};
                    writeHeader(out.data(), oh);
                    // copy payload if any
                    if (h.payloadLen>0)
                        std::copy(pkt.begin()+sizeof(UdpHeader),
                                    pkt.end(),
                                    out.begin()+sizeof(UdpHeader));
                    _backend->sendUdp(ci.udpAddr, out);
                }
            }
        }

        void runClientFrame() {
            if (!_connected) {
                _timeSinceFail += _deltaTime;
                if (_timeSinceFail > 1000.0f) initClient();
                return;
            }

            TCPsocket sock;
            IPaddress udpSrc;
            std::vector<uint8_t> pkt;

            // _backend->receive returns one packet at a time
            if (_backend->receive(sock, udpSrc, pkt)) {
                if (sock != nullptr) {
                    // Data on TCP (could be a reconnect drop, server message, etc.)
                    // For now, we only expected the initial welcome, so ignore further TCP data
                } else {
                    // UDP packet
                    if (pkt.size() < sizeof(UdpHeader)) return;
                    UdpHeader h = readHeader(pkt.data());
                    if (h.clientId == _myClientId && h.token == _myToken) {
                        SDL_Log("CLIENT echo: %.*s",
                                int(h.payloadLen),
                                pkt.data() + sizeof(UdpHeader));
                    }
                }
            }
        }
    };

}