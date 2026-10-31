#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace arena::net
{
    /**
     * Opens the host's ports on the home router (UPnP IGD), so friends can join over the internet
     * without forwarding them by hand: TCP for the reliable messages and the next port for UDP.
     *
     * Works on a thread (finding the router takes a couple of seconds), renews the mapping while the
     * game runs and closes it on the way out. A router without UPnP, or with it turned off, leaves
     * the ports closed: status() says so, and the lobby tells the host what to forward.
     */
    class PortMapper
    {
    public:
        enum class State { Off, Searching, Open, NoRouter, Failed };

        struct Status
        {
            State state = State::Off;
            std::string externalIp;   // the router's address on the internet, when it said
            uint16_t externalPort = 0;   // the port others join with (TCP; UDP the next one): may differ from the game's
            std::string lanIp;        // this machine's address on the router's network
            std::string error;
        };

        PortMapper() = default;
        ~PortMapper();

        PortMapper(const PortMapper&) = delete;
        PortMapper& operator=(const PortMapper&) = delete;

        /**
         * candidates: the outside ports to ask the router for, in order (TCP; UDP gets the next one), each
         * pair led to the game's own tcpPort and udpPort. Some routers only open some (a Freebox: above
         * 45000). Empty: the game's own port, then DEFAULT_RANGE.
         */
        void start(uint16_t tcpPort, uint16_t udpPort, const std::string& description, const std::vector<uint16_t>& candidates = {});

        /** "7777,45000-45200": ports and ranges, in order (a range by twos: each takes a pair). */
        static std::vector<uint16_t> parsePorts(const std::string& text);
        static constexpr const char* DEFAULT_RANGE = "45000-45200";
        void stop();

        Status status() const;

        /** This machine's address on its network (the one the default route goes out by), or "" when unknown. */
        static std::string localAddress();

    private:
        void run();
        void set(State state, const std::string& error = "");

        uint16_t tcpPort = 0, udpPort = 0;
        std::vector<uint16_t> candidates;
        std::string description;

        mutable std::mutex mutex;
        std::condition_variable wake;
        bool stopping = false;
        Status current;
        std::thread worker;
    };
}
