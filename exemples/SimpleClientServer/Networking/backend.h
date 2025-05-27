// NetworkingBackend.h
#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>
#include "inetwork_transport.h"
#include "network_sys_config.h"
#include "network_config.h"
#include "ECS/entitysystem.h"
#include "ECS/system.h"

using namespace pg;

// Forward-declare transports
class UDPSocketTransport;
class TCPSocketTransport;

//-----------------------------------------------------------------------------
// NetworkBackend: manages UDP/TCP, packet routing
//-----------------------------------------------------------------------------
class NetworkBackend
{
public:
    explicit NetworkBackend(const NetworkConfig& config) : config(config) {}
    ~NetworkBackend() = default;

    // Init transports (server bind or client connect)
    void initialize()
    {
        openTransports();
    }

    // Poll sockets & queue incoming packets
    void pollIncoming()
    {
        Packet p;
        while (udpTransport->receive(p)) incomingQueue.push_back(p);
        while (tcpTransport && tcpTransport->receive(p)) incomingQueue.push_back(p);
    }

    // Send a packet (reliable -> TCP, else UDP)
    bool sendPacket(const Packet& p, bool reliable)
    {
        return reliable
            ? tcpTransport ? tcpTransport->sendReliable(p) : false
            : udpTransport->sendUnreliable(p);
    }

    // Pop next incoming packet
    bool receivePacket(Packet& out) {
        if (incomingQueue.empty()) return false;
        out = incomingQueue.front();
        incomingQueue.erase(incomingQueue.begin());
        return true;
    }

private:
    const NetworkConfig& config;
    std::unique_ptr<INetworkTransport> udpTransport;
    std::unique_ptr<INetworkTransport> tcpTransport;
    std::vector<Packet> incomingQueue;

    void openTransports()
    {
        // UDP: server binds, client connects
        udpTransport.reset(new UDPSocketTransport(config.udpLocalPort, config.peerAddress, config.udpPeerPort));

        // TCP: optional fallback channel
        if (config.tcpEnabled)
        {
            tcpTransport.reset(new TCPSocketTransport(config.peerAddress, config.tcpPort, config.isServer));
        }
    }
};

//-----------------------------------------------------------------------------
// NetworkSystem: serializes NetState<States> via backend
//-----------------------------------------------------------------------------

template<typename... States>
class NetworkSystem : public System<Own<NetTag>, Own<States>...>
{
public:
    NetworkSystem(NetworkBackend& backend, const NetworkConfig& cfg)
        : backend(backend), config(cfg) {}

    void update(float dt) override {
        // Outgoing
        for (auto& e : entities) {
            uint32_t id = e.entity.id();
            (sendState<States>(id, e.get<States>()), ...);
        }
        // Incoming
        backend.pollIncoming();
        Packet pkt;
        while (backend.receivePacket(pkt)) dispatchIncoming(pkt);
    }

private:
    NetworkBackend& backend;
    const NetworkConfig& config;

    template<typename S>
    void sendState(uint32_t entityId, const NetState<S>& state) {
        Packet p{ entityId,
                  ComponentRegistry::typeId<NetState<S>>(),
                  state.serialize() };
        bool reliable = config.systemFlags.reliableChannel;
        backend.sendPacket(p, reliable);
    }

    void dispatchIncoming(const Packet& pkt) {
        Entity e = registry.getEntity(pkt.entityId);
        if (!e) return;
        auto wrapper = ComponentRegistry::createByTypeId(pkt.compType);
        wrapper->deserialize(pkt.data);
        config.invokeReceived(wrapper, e);
    }
};