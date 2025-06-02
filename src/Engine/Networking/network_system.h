#pragma once

#include "Networking/network_config.h"
#include "Networking/network_sys_config.h"
#include "Networking/common.h"
#include "Networking/ibackend.h"

#include "Systems/coresystems.h"

namespace pg
{
    enum class ClientState : uint8_t
    {
        Connecting    = 0x00,
        WaitingForId,
        Connected
    };

    struct ClientInfo
    {
        SocketHandle tcpSock    = nullptr;
        uint32_t     clientId   = 0;
        uint32_t     token      = 0;
        bool         udpLinked  = false;
        IpEndpoint  udpAddr{};

        ClientState state = ClientState::Connecting;

        uint64_t lastHeartbeatMs = 0;   // last time we heard anything
        uint64_t lastPingSentMs  = 0;   // for RTT
        float    rttMs           = 0;   // smoothed RTT
    };

    struct SendDataToServer
    {
        NetPayload data;
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
                runServerFrame(deltaTime);
            else
                runClientFrame(deltaTime);

            cleanReassemblyBuffer();

            deltaTime = 0.0f;
        }

        const ClientInfo& getClientState(uint32_t id) const
        {
            return clients.at(idToTcp.at(id));
        }

        std::vector<uint32_t> getClientIds() const
        {
            std::vector<uint32_t> ids;

            ids.reserve(idToTcp.size());

            for (const auto& pair : idToTcp)
            {
                ids.push_back(pair.first);
            }

            return ids;
        }

        // Expose the reassembly‐buffer map:
        std::map<std::tuple<uint32_t, uint32_t, uint32_t, NetMsgType>,
            std::vector<std::vector<uint8_t>>>& getReassemblyBuffer()
        {
            return reassembly;
        }

        // Expose fragment‐timestamp map:
        std::map<uint32_t, uint64_t>& getFragmentTimers()
        {
            return fragmentTimers;
        }

        // Check if a client exists by ID:
        bool clientExists(uint32_t id) const
        {
            return idToTcp.find(id) != idToTcp.end();
        }

    public:
        uint64_t getCurrentTime() const;

        std::string ipPortKey(const IpEndpoint& addr) const;

    protected:
        INetworkBackend* backend;
        NetworkConfig    netCfg;

        float deltaTime = 0.0f;

        size_t nextPacketNumber = 0;

        // Reassembly‐buffer timestamping:
        std::map<uint32_t, uint64_t> fragmentTimers; // packetNumber → first‐seen timestamp
        NetPacketBuffer reassembly;

        // Server state
        std::unordered_map<SocketHandle, ClientInfo> clients;
        std::unordered_map<uint32_t, SocketHandle>   idToTcp;
        std::unordered_map<std::string, uint32_t> _udpClientMap;
        uint32_t nextClientId = 0;

        // Client state
        uint32_t _myClientId = 0;
        uint32_t _myToken    = 0;
        float    timeSinceFail = 0.0f;
        ClientState currentClientState = ClientState::Connecting;

        // Utilities
        uint32_t genToken() const;

        void cleanReassemblyBuffer();

        // ----- Initialization -----
        void initServer();

        void initClient();

        void sendUdpHandshake();

        void sendToServer(const NetPayload& data, bool overTcp);

        bool sendTCPMessage(uint32_t clientId, uint32_t token, NetMsgType type,
            const NetPayload& payload, SocketHandle tcpSock = nullptr);

        bool sendUDPMessage(uint32_t clientId, uint32_t token, NetMsgType type,
            const NetPayload& payload, const IpEndpoint& udpDest);

        void readData();

        // ----- Per-frame logic -----
        void runServerFrame(float deltaTime);

        void runClientFrame(float deltaTime);

        void handleMessage(const PacketHeader& header, const NetPayload& payload);
        void handleServerMessage(const PacketHeader& header, const NetPayload& payload);
        void handleClientMessage(const PacketHeader& header, const NetPayload& payload);
    };

}