#include "stdafx.h"

#include "network_system.h"

namespace pg
{
    NetworkSystem::~NetworkSystem()
    {
        if (backend)
            delete backend;
    }

    uint64_t NetworkSystem::getCurrentTime() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    uint32_t NetworkSystem::genToken() const
    {
        static std::mt19937 rng{ std::random_device{}() };
        return rng();
    }

    std::string NetworkSystem::ipPortKey(const IPaddress& addr) const
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

    void NetworkSystem::cleanReassemblyBuffer()
    {
        auto now = getCurrentTime();

        for (auto& [key, slots] : reassembly)
        {
            uint64_t firstSeen = fragmentTimers[std::get<2>(key)];

            if (now - firstSeen > netCfg.defaultSystemFlags.dropPacketTimeout)
            {
                // 5s timeout
                reassembly.erase(key);
                fragmentTimers.erase(std::get<2>(key));
                LOG_ERROR("NetSys", "Reassembly buffer cleaned for key: " << std::get<2>(key));
            }
        }
    }

    // ----- Initialization -----
    void NetworkSystem::initServer()
    {
        // nothing to do here: backend ctor did the listening
        LOG_INFO("NetSys", "Server ready (TCP:" << netCfg.tcpPort << " UDP:" << netCfg.udpLocalPort << ")");
    }

    void NetworkSystem::initClient()
    {
        // attempt TCP connect, with simple back-off
        if (currentClientState == ClientState::Connecting)
        {
            if (backend->connectToServer())
            {
                currentClientState = ClientState::WaitingForId;
                LOG_INFO("NetSys", "TCP connected to " << netCfg.peerAddress << ":" << netCfg.tcpPort);
            }
            else
            {
                LOG_WARNING("NetSys", "TCP connect failed, retry in 1s");
                timeSinceFail = 0.0f;
                return;
            }
        }
    }

    void NetworkSystem::sendUdpHandshake()
    {
        // send with zero‐length payload
        IPaddress dest{};
        SDLNet_ResolveHost(&dest, netCfg.peerAddress.c_str(), netCfg.udpPeerPort);

        sendUDPMessage(_myClientId, _myToken, NetMsgType::Handshake, {0}, dest);
        LOG_INFO("NetSys", "UDP handshake sent");
    }

    void NetworkSystem::sendToServer(const std::vector<uint8_t>& data, bool overTcp)
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

    bool NetworkSystem::sendTCPMessage(uint32_t clientId, uint32_t token, NetMsgType type,
        const NetPayload& payload, TCPsocket tcpSock)
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

    bool NetworkSystem::sendUDPMessage(uint32_t clientId, uint32_t token, NetMsgType type,
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

    void NetworkSystem::readData()
    {
        TCPsocket sock;
        IPaddress udpSrc; // ignored for TCP
        std::vector<uint8_t> buf;

        while (backend->receive(sock, udpSrc, buf))
        {
            ParsedPacket msg;
            if (parseAndReassemble(buf, reassembly, fragmentTimers, msg))
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
    void NetworkSystem::runServerFrame(float dt)
    {
        // 1) Accept new TCP clients
        if (auto newSock = backend->acceptTcpClient())
        {
            size_t setId = 0;
            bool addedToSet = false;

            for (;setId < sockSets.size(); setId++)
            {
                auto res = SDLNet_TCP_AddSocket(sockSets[setId], newSock);

                if (res != -1)
                {
                    addedToSet = true;
                    break;
                }
            }

            if (not addedToSet)
            {
                auto newSet = SDLNet_AllocSocketSet(netCfg.defaultSystemFlags.socketSetSize);

                if (newSet)
                {
                    sockSets.push_back(newSet);
                    LOG_INFO("NetSys", "Created socket set " << sockSets.size() - 1);

                    auto res = SDLNet_TCP_AddSocket(newSet, newSock);

                    if (res == -1)
                    {
                        LOG_ERROR("NetSys", "Failed to add this new socket in the new set");
                        return;
                    }
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

            // send welcome

            if (sendTCPMessage(ci.clientId, ci.token, NetMsgType::Connect, {0}, newSock))
            {
                LOG_INFO("NetSys", "Sent client Id and Token to the client: " << ci.clientId << " " << ci.token);
            }
            else
            {
                LOG_ERROR("NetSys", "Couldn't send client id and token to the client");
            }

            LOG_INFO("NetSys", "New client " << ci.clientId);
        }

        // 2) Process incoming packets
        // Process Tcp data
        for (const auto& sockSet : sockSets)
        {
            SDLNet_CheckSockets(sockSet, 0);
        }

        auto now = getCurrentTime();

        for (auto& client : clients)
        {
            auto& ci = client.second;
            auto& set = sockSets.at(ci.tcpSetID);

            ci.lastHeartbeatMs += dt;
            ci.lastPingSentMs += dt;

            if (ci.lastPingSentMs >= netCfg.defaultSystemFlags.pingTimer)
            {
                if (sendTCPMessage(ci.clientId, ci.token, NetMsgType::Ping, {0}, ci.tcpSock))
                {

                    LOG_INFO("NetSys", "Sent ping to client: " << ci.clientId << " at time: " << now);
                    ci.lastPingSentMs = 0;
                }
            }

            std::vector<uint8_t> data;
            bool tcpClosed = false; // Todo need to use this

            while (backend->receiveTcp(ci.tcpSock, set, data, tcpClosed))
            {
                LOG_INFO("NetSys", "Received TCP request from client: " << ci.clientId);

                ci.lastHeartbeatMs = 0.0f;

                // ParsedPacket pkt;

                ParsedPacket msg;
                if (parseAndReassemble(data, reassembly, fragmentTimers, msg))
                {
                    LOG_INFO("NetSys", "Parsed packet from client:" << ci.clientId << " that should match header: " << msg.header.clientId);

                    handleServerMessage(msg.header, msg.payload);
                }
            }
        }

        // Process Udp data
        IPaddress ip;
        std::vector<uint8_t> data;

        while (backend->receiveUdp(ip, data))
        {
            ParsedPacket msg;
            if (parseAndReassemble(data, reassembly, fragmentTimers, msg))
            {
                LOG_INFO("NetSys", "Parsed packet from client:"  << msg.header.clientId);

                handleServerMessage(msg.header, msg.payload);
            }
            // LOG_INFO("NetSys", "Received UDP request from ip: " << ipPortKey(ip));
        }
    }

    void NetworkSystem::runClientFrame(float dt)
    {
        if (currentClientState == ClientState::Connecting)
        {
            timeSinceFail += dt;

            if (timeSinceFail > 1000.0f)
                initClient();

            return;
        }

        TCPsocket sock;
        IPaddress udpSrc; // ignored for TCP
        std::vector<uint8_t> buf;

        while (backend->receive(sock, udpSrc, buf))
        {
            ParsedPacket msg;
            if (parseAndReassemble(buf, reassembly, fragmentTimers, msg))
            {
                handleClientMessage(msg.header, msg.payload);
            }
        }
    }

    void NetworkSystem::handleMessage(const PacketHeader&, const NetPayload&)
    {
    }

    void NetworkSystem::handleServerMessage(const PacketHeader& header, const NetPayload& payload)
    {

        if (header.type == NetMsgType::Pong)
        {
            auto now = getCurrentTime();

            LOG_INFO("NetSys", "Received Pong from client: " << header.clientId << " at time: " << now);

            auto it = idToTcp.find(header.clientId);

            if (it == idToTcp.end())
            {
                LOG_ERROR("NetSys", "Received Pong from unknown client: " << header.clientId);
                return;
            }

            auto& state = clients[it->second];

            // payload is original timestamp
            uint64_t sentTs = readU64BE(payload.data());

            float sampleRtt = float(now - sentTs);

            // simple smoothing
            // state.rttMs = state.rttMs == 0 ? sampleRtt : (state.rttMs * 0.8f + sampleRtt * 0.2f);
            state.rttMs = sampleRtt;
            LOG_INFO("NetSys", "Received Pong from client: " << header.clientId << " rtt: " << state.rttMs);
        }

        handleMessage(header, payload);
    }

    void NetworkSystem::handleClientMessage(const PacketHeader& header, const NetPayload& payload)
    {
        LOG_INFO("NetSys", "Received packet from client: "
            << header.clientId << " type: " << int(header.type)
            << " payload size: " << payload.size());

        if (currentClientState == ClientState::WaitingForId)
        {
            _myClientId = header.clientId;
            _myToken = header.token;

            currentClientState = ClientState::Connected;

            LOG_INFO("NetSys", "Connected to server");

            sendUdpHandshake();
        }

        if (header.type == NetMsgType::Ping)
        {
            // send pong back

            // Todo make helpers that convert str, uint16, uint32, uint64 -> vector<uint8_t>
            uint8_t buf[8];
            writeU64BE(buf, header.timestamp);

            if (sendTCPMessage(_myClientId, _myToken, NetMsgType::Pong, std::vector<uint8_t>(buf, buf + 8)))
            {
                LOG_INFO("NetSys", "Sent Pong to server");
            }
            else
            {
                LOG_ERROR("NetSys", "Failed to send Pong to server");
            }
        }

        handleMessage(header, payload);
    }

} // namespace pg
