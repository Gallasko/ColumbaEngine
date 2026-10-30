#include "portmapper.h"

#include <chrono>
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
        // Leases are renewed well before they run out; a router that only takes permanent ones gets 0
        constexpr int LEASE_SECONDS = 3600;
        constexpr auto RENEW_EVERY = std::chrono::minutes(20);
    }

    PortMapper::~PortMapper()
    {
        stop();
    }

    void PortMapper::start(uint16_t tcp, uint16_t udp, const std::string& desc)
    {
        stop();

        tcpPort = tcp;
        udpPort = udp;
        description = desc;
        stopping = false;

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
        std::lock_guard<std::mutex> lock(mutex);
        current.state = state;
        current.error = error;
    }

    void PortMapper::run()
    {
#ifdef VOXELARENA_UPNP
        int error = 0;
        UPNPDev* devices = upnpDiscover(2000, nullptr, nullptr, UPNP_LOCAL_PORT_ANY, 0, 2, &error);

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

        const std::string tcp = std::to_string(tcpPort), udp = std::to_string(udpPort);

        auto add = [&](const std::string& port, const char* proto) {
            const std::string lease = std::to_string(LEASE_SECONDS);
            int r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, port.c_str(), port.c_str(), lan, description.c_str(), proto, nullptr, lease.c_str());

            // 725: OnlyPermanentLeasesSupported
            if (r == 725)
                r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, port.c_str(), port.c_str(), lan, description.c_str(), proto, nullptr, "0");

            return r;
        };

        bool open = false;

        while (true)
        {
            const int rt = add(tcp, "TCP");
            const int ru = add(udp, "UDP");

            if (rt == UPNPCOMMAND_SUCCESS and ru == UPNPCOMMAND_SUCCESS)
            {
                open = true;
                set(State::Open);
            }
            else if (not open)
            {
                const int r = rt != UPNPCOMMAND_SUCCESS ? rt : ru;
                set(State::Failed, std::string("the router said no: ") + strupnperror(r));
                break;
            }

            std::unique_lock<std::mutex> lock(mutex);
            if (wake.wait_for(lock, RENEW_EVERY, [this]() { return stopping; }))
                break;
        }

        // On the way out: the ports closed again
        if (open)
        {
            UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, tcp.c_str(), "TCP", nullptr);
            UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, udp.c_str(), "UDP", nullptr);
        }

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
