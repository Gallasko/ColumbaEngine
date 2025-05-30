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

        virtual ~NetworkSystem() override;

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

        size_t nextPacketNumber = 0;

        NetPacketBuffer reassembly;

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
        uint32_t genToken() const;

        std::string ipPortKey(const IPaddress& addr) const;

        // ----- Initialization -----
        void initServer();

        void initClient();

        void sendUdpHandshake();

        void sendToServer(const std::vector<uint8_t>& data, bool overTcp);

        bool sendTCPMessage(uint32_t clientId, uint32_t token, NetMsgType type,
            const NetPayload& payload, TCPsocket tcpSock = nullptr);

        bool sendUDPMessage(uint32_t clientId, uint32_t token, NetMsgType type,
            const NetPayload& payload, const IPaddress& udpDest);

        void readData();

        // ----- Per-frame logic -----
        void runServerFrame();

        void runClientFrame();
    };

}