#pragma once

#include "Networking/network_config.h"
#include "Networking/network_sys_config.h"
#include "Networking/common.h"
#include "Networking/ibackend.h"

#include "Systems/coresystems.h"

#include <deque>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>

namespace pg
{
    enum class ClientState : uint8_t
    {
        Connecting    = 0x00,   // trying to reach the server
        WaitingForId,           // connected, waiting for the server's welcome
        Connected,              // ready
        Disconnected            // gave up (autoReconnect off)
    };

    struct ClientInfo
    {
        SocketHandle tcpSock    = nullptr;
        uint32_t     clientId   = 0;
        uint32_t     token      = 0;
        bool         udpLinked  = false;   // its UDP address is known (after a handshake with the right token)
        IpEndpoint   udpAddr{};
        ClientState  state = ClientState::Connecting;
        uint64_t lastHeartbeatMs = 0;      // steady ms of the last message heard
        uint64_t lastPingSentMs  = 0;      // steady ms of the last ping sent
        float    rttMs           = 0;      // smoothed round trip
    };

    /** What the application gets from the network (NetworkSystem::poll), in arrival order. */
    struct NetIncoming
    {
        enum class Kind { Connected, Disconnected, Message };

        Kind kind = Kind::Message;
        uint32_t clientId = 0;   // server: the client; client: its own id
        NetPayload data;
        bool reliable = true;    // came over TCP
    };

    // The same, as ECS events (when NetworkConfig::emitEvents)
    struct NetPeerConnected { uint32_t clientId; };
    struct NetPeerDisconnected { uint32_t clientId; };
    struct NetMessageReceived { uint32_t clientId; NetPayload data; bool reliable; };

    struct SendDataToServer
    {
        NetPayload data;
        bool inTcp = false; // true = send over TCP, false = send over UDP
    };

    /**
     * A server with its clients, or a client with its server, over a backend (SDL_net).
     *
     * Reliable messages go over TCP, unreliable ones over UDP once the peer's UDP address is linked
     * (a handshake carrying the token the server gave over TCP), over TCP until then. Messages of
     * any size are cut into fragments and put back together.
     *
     * The application sends with send / sendToServer and reads with poll, from any thread: both go
     * through queues, and only execute() touches the sockets. Peers silent for timeoutMs are dropped;
     * the server pings every pingTimer ms, which also gives the round trip.
     */
    struct NetworkSystem : public System<InitSys, Listener<TickEvent>, Listener<SendDataToServer>>
    {
        NetworkSystem(INetworkBackend* backend, const NetworkConfig& cfg) : backend(backend), netCfg(cfg) {}
        virtual ~NetworkSystem() override;

        virtual std::string getSystemName() const override { return "Network System"; }

        virtual void onEvent(const TickEvent&) override {}
        virtual void onEvent(const SendDataToServer& e) override { sendToServer(e.data, e.inTcp); }

        virtual void init() override;
        virtual void execute() override;

        // ---- Application side (any thread) ----

        bool poll(NetIncoming& out);

        /** Server: to one client. */
        void send(uint32_t clientId, const NetPayload& data, bool reliable);

        /** Client: to the server. */
        void sendToServer(const NetPayload& data, bool reliable);

        /** Client: say goodbye and stop (no reconnect). */
        void disconnect();

        bool isServer() const { return netCfg.isServer; }
        float rttMs(uint32_t clientId) const;

        // ---- State ----

        ClientState getCurrentConnectingState() const { return currentClientState; }
        uint32_t myClientId() const { return _myClientId; }

        const ClientInfo& getClientState(uint32_t id) const { return clients.at(idToTcp.at(id)); }
        std::vector<uint32_t> getClientIds() const;
        bool clientExists(uint32_t id) const { return idToTcp.find(id) != idToTcp.end(); }

        NetPacketBuffer& getReassemblyBuffer() { return reassembly; }
        NetPacketTimers& getFragmentTimers() { return fragmentTimers; }

        uint64_t getCurrentTime() const { return steadyMs(); }

        std::string ipPortKey(const IpEndpoint& addr) const { return addr.host + ":" + std::to_string(addr.port); }

        // Low level: one message to one place (network thread only)
        bool sendTCPMessage(uint32_t clientId, uint32_t token, NetMsgType type, const NetPayload& payload, SocketHandle tcpSock = nullptr);
        bool sendUDPMessage(uint32_t clientId, uint32_t token, NetMsgType type, const NetPayload& payload, const IpEndpoint& udpDest);

    protected:
        struct Outgoing
        {
            uint32_t clientId;
            NetPayload data;
            bool reliable;
        };

        void push(NetIncoming incoming);
        void flushOutbox();

        void runServerFrame();
        void runClientFrame();

        void handleServerMessage(SocketHandle from, const IpEndpoint* udpFrom, const PacketHeader& header, NetPayload& payload);
        void handleClientMessage(const PacketHeader& header, NetPayload& payload, bool reliable);

        void disconnectClient(uint32_t id);
        void lostServer();

        uint32_t genToken() const;

        INetworkBackend* backend;
        NetworkConfig    netCfg;

        uint32_t nextPacketNumber = 0;

        NetPacketTimers fragmentTimers;
        NetPacketBuffer reassembly;

        // Server state
        std::unordered_map<SocketHandle, ClientInfo> clients;
        std::unordered_map<uint32_t, SocketHandle>   idToTcp;
        uint32_t nextClientId = 1;
        std::set<uint32_t> leavingClients;   // said goodbye: removed after the frame's loops, not inside them

        // Client state
        uint32_t _myClientId = 0;
        uint32_t _myToken    = 0;
        ClientState currentClientState = ClientState::Connecting;
        bool     connecting = false;
        uint64_t lastAttemptMs = 0;
        uint64_t lastHeardMs = 0;
        bool     udpAcked = false;
        uint64_t lastHandshakeMs = 0;
        bool     leaving = false;

        mutable std::mutex queueMutex;
        std::deque<NetIncoming> inbox;
        std::deque<Outgoing> outbox;
    };
}
