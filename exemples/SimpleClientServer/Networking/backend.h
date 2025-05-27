#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include "inetwork_transport.h"
#include "network_config.h"
#include "tcp_socket_transport.h"
#include "udp_socket_transport.h"

//-----------------------------------------------------------------------------
// NetworkBackend: manages UDP/TCP, packet routing
//-----------------------------------------------------------------------------

class NetworkBackend {
public:
    explicit NetworkBackend(const NetworkConfig& cfg)
      : config(cfg)
    {}

    ~NetworkBackend() = default;

    // Init transports (server: bind; client: connect)
    void initialize() {
        openTransports();
    }

    // Poll sockets & queue incoming packets
    void pollIncoming() {
        Packet p;
        while (udpTransport->receive(p))    incomingQueue.push_back(p);
        if (tcpTransport) {
            while (tcpTransport->receive(p)) incomingQueue.push_back(p);
        }
    }

    // Send a packet (reliable → TCP, else UDP)
    bool sendPacket(const Packet& p, bool reliable) {
        if (reliable && tcpTransport) return tcpTransport->sendReliable(p);
        if (!reliable && udpTransport) return udpTransport->sendUnreliable(p);
        return false;
    }

    // Pop the next incoming packet (returns false if none left)
    bool receivePacket(Packet& out) {
        if (incomingQueue.empty()) return false;
        out = incomingQueue.front();
        incomingQueue.erase(incomingQueue.begin());
        return true;
    }

private:
    const NetworkConfig&             config;
    std::unique_ptr<INetworkTransport> udpTransport;
    std::unique_ptr<INetworkTransport> tcpTransport;
    std::vector<Packet>               incomingQueue;

    void openTransports() {
        // UDP: server binds (port ≠ 0), client uses ephemeral (port = 0)
        udpTransport.reset(
          new UDPSocketTransport(
            config.udpLocalPort,
            config.peerAddress,
            config.udpPeerPort
          )
        );

        // TCP: optional fallback channel
        if (config.tcpEnabled) {
            tcpTransport.reset(
              new TCPSocketTransport(
                config.peerAddress,
                config.tcpPort,
                config.isServer
              )
            );
        }
    }
};
