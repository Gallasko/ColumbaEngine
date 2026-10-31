#include "stdafx.h"

#include "network_system.h"

#include <random>

#include "ECS/entitysystem.h"

namespace pg
{
    namespace
    {
        constexpr const char* DOM = "NetSys";

        // How often a client repeats its UDP handshake until the server says it got it
        constexpr uint64_t HANDSHAKE_EVERY_MS = 250;

        // After UDP went silent: how long a client stays on TCP before trying UDP again
        constexpr uint64_t UDP_RETRY_FIRST_MS = 5000;
        constexpr uint64_t UDP_RETRY_MOST_MS = 60000;
        constexpr uint64_t RETRY_CONNECT_MS = 1000;
    }

    NetworkSystem::~NetworkSystem()
    {
        if (backend)
            delete backend;
    }

    uint32_t NetworkSystem::genToken() const
    {
        static std::mt19937 rng{ std::random_device{}() };
        return rng() | 1u;   // never 0: 0 is "no token yet"
    }

    std::vector<uint32_t> NetworkSystem::getClientIds() const
    {
        std::vector<uint32_t> ids;
        ids.reserve(idToTcp.size());

        for (const auto& pair : idToTcp)
            ids.push_back(pair.first);

        return ids;
    }

    float NetworkSystem::rttMs(uint32_t clientId) const
    {
        auto it = idToTcp.find(clientId);
        return it == idToTcp.end() ? 0.0f : clients.at(it->second).rttMs;
    }

    // ---- Queues ----------------------------------------------------------------------------------

    void NetworkSystem::push(NetIncoming incoming)
    {
        if (netCfg.emitEvents and ecsRef)
        {
            switch (incoming.kind)
            {
                case NetIncoming::Kind::Connected: ecsRef->sendEvent(NetPeerConnected{incoming.clientId}); break;
                case NetIncoming::Kind::Disconnected: ecsRef->sendEvent(NetPeerDisconnected{incoming.clientId}); break;
                case NetIncoming::Kind::Message: ecsRef->sendEvent(NetMessageReceived{incoming.clientId, incoming.data, incoming.reliable}); break;
            }
        }

        std::lock_guard<std::mutex> lock(queueMutex);
        inbox.push_back(std::move(incoming));

        // Nobody reads the queue (an app using the events only): keep it from growing for ever
        while (inbox.size() > 4096)
            inbox.pop_front();
    }

    bool NetworkSystem::poll(NetIncoming& out)
    {
        std::lock_guard<std::mutex> lock(queueMutex);

        if (inbox.empty())
            return false;

        out = std::move(inbox.front());
        inbox.pop_front();
        return true;
    }

    void NetworkSystem::send(uint32_t clientId, const NetPayload& data, bool reliable)
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        outbox.push_back({clientId, data, reliable});
    }

    void NetworkSystem::sendToServer(const NetPayload& data, bool reliable)
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        outbox.push_back({0, data, reliable});
    }

    void NetworkSystem::disconnect()
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        leaving = true;
    }

    void NetworkSystem::flushOutbox()
    {
        std::deque<Outgoing> pending;
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            pending.swap(outbox);
        }

        for (auto& o : pending)
        {
            if (netCfg.isServer)
            {
                auto it = idToTcp.find(o.clientId);

                if (it == idToTcp.end())
                    continue;

                const ClientInfo& ci = clients[it->second];

                if (not o.reliable and ci.udpLinked)
                    sendUDPMessage(ci.clientId, ci.token, NetMsgType::Custom, o.data, ci.udpAddr);
                else
                    sendTCPMessage(ci.clientId, ci.token, NetMsgType::Custom, o.data, ci.tcpSock);
            }
            else if (currentClientState == ClientState::Connected)
            {
                // Unreliable over UDP once the server has our address, over TCP until then
                if (not o.reliable and udpAcked)
                    sendUDPMessage(_myClientId, _myToken, NetMsgType::Custom, o.data, IpEndpoint{netCfg.peerAddress, netCfg.udpPeerPort});
                else
                    sendTCPMessage(_myClientId, _myToken, NetMsgType::Custom, o.data);
            }
        }
    }

    // ---- Sending ---------------------------------------------------------------------------------

    bool NetworkSystem::sendTCPMessage(uint32_t clientId, uint32_t token, NetMsgType type, const NetPayload& payload, SocketHandle tcpSock)
    {
        const auto frags = fragmentPayload(clientId, token, type, nextPacketNumber++, backend, payload);

        for (const auto& pkt : frags)
        {
            const bool sent = tcpSock ? backend->sendTcp(tcpSock, pkt) : backend->sendTcp(pkt);

            if (not sent)
                return false;
        }

        return true;
    }

    bool NetworkSystem::sendUDPMessage(uint32_t clientId, uint32_t token, NetMsgType type, const NetPayload& payload, const IpEndpoint& udpDest)
    {
        const auto frags = fragmentPayload(clientId, token, type, nextPacketNumber++, backend, payload, MAX_UDP_DATAGRAM);

        for (const auto& pkt : frags)
            if (not backend->sendUdp(udpDest, pkt))
                return false;

        return true;
    }

    // ---- Frame -----------------------------------------------------------------------------------

    void NetworkSystem::init()
    {
        if (not backend or not backend->isReady())
        {
            LOG_ERROR(DOM, "The network backend is not ready (port in use, or SDL_net failed): nothing will connect");
            return;
        }

        if (netCfg.isServer)
            LOG_INFO(DOM, "Server ready (TCP:" << netCfg.tcpPort << " UDP:" << netCfg.udpLocalPort << ")");
    }

    void NetworkSystem::execute()
    {
        if (not backend or not backend->isReady())
            return;

        if (netCfg.isServer)
            runServerFrame();
        else
            runClientFrame();

        const size_t dropped = dropStaleFragments(reassembly, fragmentTimers, steadyMs(), static_cast<uint64_t>(netCfg.defaultSystemFlags.dropPacketTimeout));

        if (dropped > 0)
            LOG_WARNING(DOM, dropped << " message(s) dropped: their fragments never all arrived");
    }

    void NetworkSystem::runServerFrame()
    {
        const uint64_t now = steadyMs();

        // 1) New clients (a few a frame)
        for (int i = 0; i < 4; ++i)
        {
            SocketHandle sock = backend->acceptTcpClient();

            if (not sock)
                break;

            ClientInfo ci;
            ci.tcpSock = sock;
            ci.clientId = nextClientId++;
            ci.token = genToken();
            ci.state = ClientState::Connected;
            ci.lastHeartbeatMs = now;
            ci.lastPingSentMs = now;

            clients[sock] = ci;
            idToTcp[ci.clientId] = sock;

            // The welcome carries the id and token in its header
            if (not sendTCPMessage(ci.clientId, ci.token, NetMsgType::Connect, {}, sock))
            {
                LOG_ERROR(DOM, "Could not welcome client " << ci.clientId);
                disconnectClient(ci.clientId);
                continue;
            }

            LOG_INFO(DOM, "Client " << ci.clientId << " connected");
            push({NetIncoming::Kind::Connected, ci.clientId, {}, true});
        }

        // 2) What the application wants sent
        flushOutbox();

        // 3) TCP from every client
        std::set<uint32_t> gone;

        for (auto& [sock, ci] : clients)
        {
            NetPayload data;
            bool closed = false;
            SocketHandle s = sock;

            while (backend->receiveTcp(s, data, closed))
            {
                ParsedPacket msg;

                if (parseAndReassemble(data, reassembly, fragmentTimers, backend, msg))
                {
                    // A message on this socket must carry this client's id and token
                    if (msg.header.clientId != ci.clientId or msg.header.token != ci.token)
                        continue;

                    ci.lastHeartbeatMs = now;
                    handleServerMessage(sock, nullptr, msg.header, msg.payload);
                }
            }

            if (closed)
                gone.insert(ci.clientId);

            if (closed or leavingClients.count(ci.clientId))
                continue;

            // 4) Ping, timeout
            if (static_cast<float>(now - ci.lastPingSentMs) >= netCfg.defaultSystemFlags.pingTimer)
            {
                sendTCPMessage(ci.clientId, ci.token, NetMsgType::Ping, {}, ci.tcpSock);
                ci.lastPingSentMs = now;
            }

            if (static_cast<float>(now - ci.lastHeartbeatMs) > netCfg.timeoutMs)
            {
                LOG_WARNING(DOM, "Client " << ci.clientId << " timed out");
                gone.insert(ci.clientId);
            }
        }

        // 5) UDP
        IpEndpoint from;
        NetPayload data;

        while (backend->receiveUdp(from, data))
        {
            ParsedPacket msg;

            if (not parseAndReassemble(data, reassembly, fragmentTimers, backend, msg))
                continue;

            auto it = idToTcp.find(msg.header.clientId);

            // Unknown id or wrong token: not one of ours
            if (it == idToTcp.end() or clients[it->second].token != msg.header.token)
                continue;

            ClientInfo& ci = clients[it->second];

            // Once linked, only that address speaks for the client
            if (ci.udpLinked and (ci.udpAddr.host != from.host or ci.udpAddr.port != from.port) and msg.header.type != NetMsgType::Handshake)
                continue;

            ci.lastHeartbeatMs = now;
            handleServerMessage(it->second, &from, msg.header, msg.payload);
        }

        gone.insert(leavingClients.begin(), leavingClients.end());
        leavingClients.clear();

        for (uint32_t id : gone)
            disconnectClient(id);
    }

    void NetworkSystem::handleServerMessage(SocketHandle from, const IpEndpoint* udpFrom, const PacketHeader& header, NetPayload& payload)
    {
        ClientInfo& ci = clients[from];

        switch (header.type)
        {
            case NetMsgType::Handshake:
            {
                if (not udpFrom)
                    break;

                // The client's UDP address, proven by the token it got over TCP. The answer goes over
                // UDP: only the client can tell that this direction works, and until it does, TCP
                ci.udpAddr = *udpFrom;
                ci.udpLinked = false;

                sendUDPMessage(ci.clientId, ci.token, NetMsgType::Handshake, {}, ci.udpAddr);
                break;
            }

            case NetMsgType::UdpConfirm:
                ci.udpLinked = true;
                LOG_INFO(DOM, "Client " << ci.clientId << " UDP linked at " << ipPortKey(ci.udpAddr));
                break;

            case NetMsgType::UdpLost:
                ci.udpLinked = false;
                LOG_WARNING(DOM, "Client " << ci.clientId << " gets no UDP from here: back to TCP");
                break;

            case NetMsgType::Pong:
            {
                if (payload.size() < 8)
                    break;

                const uint64_t sent = backend->readU64BE(payload.data());
                const uint64_t now = steadyMs();

                if (now >= sent)
                {
                    const float sample = static_cast<float>(now - sent);
                    ci.rttMs = ci.rttMs <= 0.0f ? sample : ci.rttMs * 0.8f + sample * 0.2f;
                }
                break;
            }

            case NetMsgType::Disconnect:
                leavingClients.insert(ci.clientId);
                break;

            case NetMsgType::Custom:
                push({NetIncoming::Kind::Message, ci.clientId, std::move(payload), udpFrom == nullptr});
                break;

            default:
                break;
        }
    }

    void NetworkSystem::disconnectClient(uint32_t id)
    {
        auto it = idToTcp.find(id);

        if (it == idToTcp.end())
            return;

        SocketHandle sock = it->second;
        clients.erase(sock);
        idToTcp.erase(it);
        backend->closeTcp(sock);

        LOG_INFO(DOM, "Client " << id << " disconnected");
        push({NetIncoming::Kind::Disconnected, id, {}, true});
    }

    void NetworkSystem::lostServer()
    {
        const bool wasConnected = currentClientState == ClientState::Connected or currentClientState == ClientState::WaitingForId;

        backend->disconnectFromServer();
        udpAcked = false;
        udpRetryAtMs = 0;
        udpRetryDelayMs = 0;
        connecting = false;
        lastAttemptMs = steadyMs();

        currentClientState = (netCfg.autoReconnect and not leaving) ? ClientState::Connecting : ClientState::Disconnected;

        if (wasConnected)
        {
            LOG_WARNING(DOM, "Lost the server");
            push({NetIncoming::Kind::Disconnected, _myClientId, {}, true});
        }
    }

    void NetworkSystem::runClientFrame()
    {
        const uint64_t now = steadyMs();

        if (currentClientState == ClientState::Disconnected)
            return;

        if (leaving)
        {
            if (currentClientState == ClientState::Connected)
                sendTCPMessage(_myClientId, _myToken, NetMsgType::Disconnect, {});

            lostServer();
            currentClientState = ClientState::Disconnected;
            return;
        }

        // Connecting, without blocking the frame
        if (currentClientState == ClientState::Connecting)
        {
            if (not connecting)
            {
                if (lastAttemptMs != 0 and now - lastAttemptMs < RETRY_CONNECT_MS)
                    return;

                backend->startConnect();
                connecting = true;
                lastAttemptMs = now;
            }

            const int status = backend->connectStatus();

            if (status == 0)
                return;

            connecting = false;

            if (status < 0)
            {
                lastAttemptMs = now;

                if (not netCfg.autoReconnect)
                    currentClientState = ClientState::Disconnected;
                return;
            }

            LOG_INFO(DOM, "Connected to " << netCfg.peerAddress << ":" << netCfg.tcpPort);
            currentClientState = ClientState::WaitingForId;
            lastHeardMs = now;
        }

        flushOutbox();

        SocketHandle sock;
        IpEndpoint udpSrc;
        NetPayload buf;

        while (backend->receive(sock, udpSrc, buf))
        {
            ParsedPacket msg;

            if (parseAndReassemble(buf, reassembly, fragmentTimers, backend, msg))
            {
                lastHeardMs = now;

                if (not sock)
                    lastUdpHeardMs = now;

                handleClientMessage(msg.header, msg.payload, sock != nullptr);
            }
        }

        if (not backend->isConnectedToServer())
        {
            lostServer();
            return;
        }

        // UDP gone quiet (read after draining the socket, so a slow frame of ours does not count):
        // TCP carries everything again, for a while
        if (currentClientState == ClientState::Connected and udpAcked and netCfg.udpSilenceMs > 0 and now - lastUdpHeardMs > netCfg.udpSilenceMs)
        {
            LOG_WARNING(DOM, "No UDP from the server for " << netCfg.udpSilenceMs << " ms: back to TCP");

            udpAcked = false;
            udpRetryDelayMs = udpRetryDelayMs == 0 ? UDP_RETRY_FIRST_MS : std::min(udpRetryDelayMs * 2, UDP_RETRY_MOST_MS);
            udpRetryAtMs = now + udpRetryDelayMs;

            sendTCPMessage(_myClientId, _myToken, NetMsgType::UdpLost, {});
        }

        // Keep offering the UDP address until the server's answer arrives
        if (currentClientState == ClientState::Connected and not udpAcked and now >= udpRetryAtMs and now - lastHandshakeMs >= HANDSHAKE_EVERY_MS)
        {
            sendUDPMessage(_myClientId, _myToken, NetMsgType::Handshake, {}, IpEndpoint{netCfg.peerAddress, netCfg.udpPeerPort});
            lastHandshakeMs = now;
        }

        if (static_cast<float>(now - lastHeardMs) > netCfg.timeoutMs)
        {
            LOG_WARNING(DOM, "The server stopped answering");
            lostServer();
        }
    }

    void NetworkSystem::handleClientMessage(const PacketHeader& header, NetPayload& payload, bool reliable)
    {
        switch (header.type)
        {
            case NetMsgType::Connect:
            {
                if (currentClientState != ClientState::WaitingForId)
                    break;

                _myClientId = header.clientId;
                _myToken = header.token;
                currentClientState = ClientState::Connected;
                udpAcked = false;
                lastHandshakeMs = 0;

                LOG_INFO(DOM, "Welcomed by the server as client " << _myClientId);
                push({NetIncoming::Kind::Connected, _myClientId, {}, true});
                break;
            }

            case NetMsgType::Handshake:
                // Over UDP only: this is what proves the server's datagrams reach us
                if (reliable or udpAcked or currentClientState != ClientState::Connected or header.token != _myToken)
                    break;

                udpAcked = true;
                lastUdpHeardMs = steadyMs();
                sendTCPMessage(_myClientId, _myToken, NetMsgType::UdpConfirm, {});
                break;

            case NetMsgType::Ping:
            {
                uint8_t buf[8];
                backend->writeU64BE(buf, header.timestamp);
                sendTCPMessage(_myClientId, _myToken, NetMsgType::Pong, NetPayload(buf, buf + 8));
                break;
            }

            case NetMsgType::Disconnect:
                lostServer();
                break;

            case NetMsgType::Custom:
                if (currentClientState == ClientState::Connected and header.token == _myToken)
                    push({NetIncoming::Kind::Message, _myClientId, std::move(payload), reliable});
                break;

            default:
                break;
        }
    }
}
