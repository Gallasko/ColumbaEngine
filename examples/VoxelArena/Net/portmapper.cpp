// MinGW's ws2tcpip.h declares inet_pton/inet_ntop for Vista and later only, and the system headers
// pick an older default: this has to come before the first include.
#if defined(_WIN32) && !defined(_WIN32_WINNT)
    #define _WIN32_WINNT 0x0600
#endif

#include "portmapper.h"

#include "logger.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>

#ifdef VOXELARENA_UPNP
    #include "miniupnpc.h"
    #include "upnpcommands.h"
    #include "upnperrors.h"
#endif

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

namespace arena::net
{
    namespace
    {
        constexpr const char* DOM = "Port Mapper";

        // Leases are renewed well before they run out; a router that only takes permanent ones gets 0
        constexpr int LEASE_SECONDS = 3600;
        constexpr auto RENEW_EVERY = std::chrono::minutes(20);
    }

    PortMapper::~PortMapper()
    {
        stop();
    }

    std::vector<uint16_t> PortMapper::parsePorts(const std::string& text)
    {
        std::vector<uint16_t> out;
        size_t at = 0;

        while (at < text.size() and out.size() < 256)
        {
            size_t comma = text.find(',', at);
            if (comma == std::string::npos)
                comma = text.size();
            const std::string part = text.substr(at, comma - at);
            at = comma + 1;

            const size_t dash = part.find('-');
            const long from = std::strtol(part.c_str(), nullptr, 10);
            const long to = dash == std::string::npos ? from : std::strtol(part.c_str() + dash + 1, nullptr, 10);

            // A range: every other port (each one taken goes with the next, for UDP)
            for (long p = from; p <= to and p > 0 and p < 65535 and out.size() < 256; p += 2)
                if (std::find(out.begin(), out.end(), static_cast<uint16_t>(p)) == out.end())
                    out.push_back(static_cast<uint16_t>(p));
        }

        return out;
    }

    void PortMapper::start(uint16_t tcp, uint16_t udp, const std::string& desc, const std::vector<uint16_t>& tries)
    {
        stop();

        tcpPort = tcp;
        udpPort = udp;
        description = desc;
        stopping = false;

        // The game's own port first, then the others (a router may only open some)
        candidates = tries;
        if (candidates.empty())
        {
            candidates.push_back(tcp);
            for (uint16_t p : parsePorts(DEFAULT_RANGE))
                if (p != tcp)
                    candidates.push_back(p);
        }

#ifdef VOXELARENA_UPNP
        set(State::Searching);
        worker = std::thread([this]() { run(); });
#else
        set(State::Failed, "this build has no UPnP");
#endif
    }

    void PortMapper::stop()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopping = true;
        }

        wake.notify_all();

        if (worker.joinable())
            worker.join();
    }

    PortMapper::Status PortMapper::status() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return current;
    }

    void PortMapper::set(State state, const std::string& error)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            current.state = state;
            current.error = error;
        }

        // The lobby shows this; the log keeps it for a host asking why nobody could join
        if (state == State::Open)
            LOG_INFO(DOM, "Router opened TCP " << current.externalPort << " and UDP " << current.externalPort + (udpPort - tcpPort) << " to this machine's " << tcpPort << " and " << udpPort << " (UPnP)");
        else if (not error.empty())
            LOG_INFO(DOM, "Router did not open the ports: " << error);
    }

    void PortMapper::run()
    {
#ifdef VOXELARENA_UPNP
        int error = 0;
        UPNPDev* devices = upnpDiscover(1000, nullptr, nullptr, UPNP_LOCAL_PORT_ANY, 0, 2, &error);

        UPNPUrls urls{};
        IGDdatas data{};
        char lan[64] = {}, wan[64] = {};
        const int found = devices ? UPNP_GetValidIGD(devices, &urls, &data, lan, sizeof(lan), wan, sizeof(wan)) : 0;
        freeUPNPDevlist(devices);

        if (found == 0)
        {
            set(State::NoRouter, "no UPnP router answered");
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            current.lanIp = lan;
        }

        // A router behind another one (a reserved address outside): opening ports here is not enough
        if (found != 1)
        {
            FreeUPNPUrls(&urls);
            set(State::Failed, found == 2 ? "the router is behind another one (its outside address is private)" : "the router is not connected");
            return;
        }

        char external[40] = {};
        if (UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, external) == UPNPCOMMAND_SUCCESS)
        {
            std::lock_guard<std::mutex> lock(mutex);
            current.externalIp = external;
        }

        const std::string inTcp = std::to_string(tcpPort), inUdp = std::to_string(udpPort);
        const int udpOffset = static_cast<int>(udpPort) - static_cast<int>(tcpPort);

        // Outside port -> the game's port on this machine, for a while (renewed); 725: only permanent ones
        auto add = [&](const std::string& outside, const std::string& inside, const char* proto) {
            const std::string lease = std::to_string(LEASE_SECONDS);
            int r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, outside.c_str(), inside.c_str(), lan, description.c_str(), proto, nullptr, lease.c_str());
            if (r == 725)
                r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, outside.c_str(), inside.c_str(), lan, description.c_str(), proto, nullptr, "0");
            return r;
        };

        // The first pair of outside ports the router takes: the TCP one, and the next for UDP
        std::string outTcp, outUdp;
        int lastError = 0;
        int tried = 0;

        for (uint16_t candidate : candidates)
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (stopping)
                    break;
            }

            const int udpOut = static_cast<int>(candidate) + udpOffset;
            if (udpOut <= 0 or udpOut > 65535)
                continue;

            ++tried;
            const std::string t = std::to_string(candidate), u = std::to_string(udpOut);
            const int rt = add(t, inTcp, "TCP");

            if (rt != UPNPCOMMAND_SUCCESS)
            {
                lastError = rt;
                continue;
            }

            const int ru = add(u, inUdp, "UDP");

            if (ru != UPNPCOMMAND_SUCCESS)
            {
                // Half open is no use: the TCP one closed again, on to the next pair
                UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, t.c_str(), "TCP", nullptr);
                lastError = ru;
                continue;
            }

            outTcp = t;
            outUdp = u;
            break;
        }

        if (outTcp.empty())
        {
            FreeUPNPUrls(&urls);
            set(State::Failed, "the router took none of the " + std::to_string(tried) + " ports tried" +
                                   (lastError ? std::string(" (") + strupnperror(lastError) + ")" : std::string()));
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            current.externalPort = static_cast<uint16_t>(std::stoi(outTcp));
        }
        set(State::Open);

        // Kept open while the game runs: renewed before the lease runs out
        while (true)
        {
            {
                std::unique_lock<std::mutex> lock(mutex);
                if (wake.wait_for(lock, RENEW_EVERY, [this]() { return stopping; }))
                    break;
            }

            add(outTcp, inTcp, "TCP");
            add(outUdp, inUdp, "UDP");
        }

        // On the way out: the ports closed again
        UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, outTcp.c_str(), "TCP", nullptr);
        UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, outUdp.c_str(), "UDP", nullptr);

        FreeUPNPUrls(&urls);
#endif
    }

    std::string PortMapper::localAddress()
    {
        // A UDP socket "connected" to a public address sends nothing, but picks the interface the
        // default route goes out by: its address is the one others on the network reach this machine at
#ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
            return "";
        SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == INVALID_SOCKET)
            return "";
#else
        int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s < 0)
            return "";
#endif

        sockaddr_in remote{};
        remote.sin_family = AF_INET;
        remote.sin_port = htons(53);
        inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);

        std::string out;

        if (connect(s, reinterpret_cast<sockaddr*>(&remote), sizeof(remote)) == 0)
        {
            sockaddr_in local{};
            socklen_t length = sizeof(local);

            if (getsockname(s, reinterpret_cast<sockaddr*>(&local), &length) == 0)
            {
                char text[INET_ADDRSTRLEN] = {};
                if (inet_ntop(AF_INET, &local.sin_addr, text, sizeof(text)))
                    out = text;
            }
        }

#ifdef _WIN32
        closesocket(s);
        WSACleanup();
#else
        close(s);
#endif

        return out;
    }
}
