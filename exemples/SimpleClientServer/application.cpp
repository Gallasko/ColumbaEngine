#include "application.h"

#include "logger.h"

#include "Networking/common.h"
#include "Networking/backend.h"

#include "Systems/coresystems.h"

using namespace pg;

namespace {
    static const char *const DOM = "App";
}

GameApp::GameApp(const std::string &appName) : appName(appName)
{
    LOG_THIS_MEMBER(DOM);
}

GameApp::~GameApp()
{
    LOG_THIS_MEMBER(DOM);
}

std::thread *initThread;
pg::Window *mainWindow = nullptr;
std::atomic<bool> initialized = {false};
bool init = false;
bool running = true;

void initWindow(const std::string &appName)
{
#ifdef __EMSCRIPTEN__
    mainWindow = new pg::Window(appName, "/save/savedData.sz");
#else
    mainWindow = new pg::Window(appName);
#endif

    LOG_INFO(DOM, "Window init...");

    initialized = true;
}

constexpr int TICK_MS = 16;

// Simple server loop: echo back what you get
void runServer(NetworkBackend& net)
{
    Packet p;

    net.pollIncoming();

    while (net.receivePacket(p))
    {
        LOG_INFO(DOM, "Received packet from client: id = " << p.entityId << ", type = " << p.compType << ", data size = " << p.data.size());

        net.sendPacket(p, /*reliable=*/false);
    }
}

// Simple client loop: send HELLO once, then print echoes
void runClient(NetworkBackend& net)
{
    LOG_INFO(DOM, "Client running, sending HELLO packet...");

    Packet hello{ 1, 0, { 'H','E','L','O' } };

    net.sendPacket(hello, /*reliable=*/true);

    Packet p;
    net.pollIncoming();

    while (net.receivePacket(p))
    {
        std::cout << "[CLIENT] echo: '"
                    << std::string(p.data.begin(), p.data.end())
                    << "'\n";
    }
}

struct ClientInfo
{
    TCPsocket    tcpSock    = nullptr;
    uint32_t     clientId   = 0;
    uint32_t     token      = 0;
    bool         udpLinked  = false;
    IPaddress    udpAddr{};
};


struct NetworkSystem : public System<InitSys, Listener<TickEvent>>
{
    bool isServer;
    bool initialized;

    NetworkConfig netCfg;

    bool connectedToServer = false;
    float timeBeforeReconnect = 0.0f;

    float deltaTime = 0.0f;

    virtual void onEvent(const TickEvent& e) override
    {
        deltaTime += e.tick;
    }

    NetworkSystem(bool isServer) : isServer(isServer), initialized(false)
    {
        if (SDLNet_Init() < 0)
        {
            LOG_ERROR(DOM, "SDLNet_Init failed: " << SDLNet_GetError());
            return;
        }

        LOG_INFO(DOM, "SDLNet initialized");
        initialized = true;
    }

    // virtual void execute() override
    // {
    //     if (not initialized)
    //         return;

    //     if (isServer)
    //         runServer(*netBackend);
    //     else
    //     {
    //         if (deltaTime > 1000.0f)
    //         {
    //             runClient(*netBackend);
    //             deltaTime = 0.0f;
    //         }
    //     }
    // }

    virtual ~NetworkSystem() override
    {
        if (not initialized)
            return;

        LOG_INFO(DOM, "Shutting down network backend...");
        SDLNet_Quit();
    }

    virtual void init() override
    {
        // --- Common network init ---

        netCfg.isServer     = isServer;
        netCfg.peerAddress  = "127.0.0.1";
        netCfg.udpLocalPort = isServer ? 9001 : 0;    // server binds; client gets ephemeral
        netCfg.udpPeerPort  = isServer ? 0    : 9001; // client → server port
        netCfg.tcpEnabled   = true;                   // disable TCP for PoC
        netCfg.tcpPort  = 9000;
        // you can tweak defaultSystemFlags here:
        netCfg.defaultSystemFlags.networked       = true;
        netCfg.defaultSystemFlags.reliableChannel = false;
        netCfg.defaultSystemFlags.updateRateHz    = 30.0f;

        // --- Specific init ---

        if (isServer)
            initServer();
        else
            initClient();
    }

    virtual void execute() override
    {
        if (isServer)
            runServerFrame(deltaTime);
        else
            runClientFrame(deltaTime);

        deltaTime = 0.0f; // reset after processing
    }

private:
    // --- shared utilities ---
    uint32_t genToken()
    {
        static std::mt19937 rng{ std::random_device{}() };
        return rng();
    }

    // Convert an SDL_net IPaddress to "x.x.x.x:port"
    inline std::string ipPortKey(const IPaddress& addr) {
        // Read host and port from network‐order fields
        Uint32 rawIP   = SDLNet_Read32(reinterpret_cast<const Uint8*>(&addr.host));
        Uint16 rawPort = SDLNet_Read16(reinterpret_cast<const Uint8*>(&addr.port));

        // Break into bytes
        Uint8 b0 = (rawIP >> 24) & 0xFF;
        Uint8 b1 = (rawIP >> 16) & 0xFF;
        Uint8 b2 = (rawIP >>  8) & 0xFF;
        Uint8 b3 = (rawIP      ) & 0xFF;

        // Port is already in host‐order after SDLNet_Read16
        return std::to_string(b0) + "."
            + std::to_string(b1) + "."
            + std::to_string(b2) + "."
            + std::to_string(b3) + ":"
            + std::to_string(rawPort);
    }

    // --- SERVER MEMBERS ---
    TCPsocket               _tcpListener = nullptr;
    UDPsocket               _udpSock      = nullptr;
    UDPpacket*              _udpPacket    = nullptr;
    std::unordered_map<TCPsocket,ClientInfo> _clients;
    std::unordered_map<uint32_t,TCPsocket>   _idToTcp;
    uint32_t                _nextClientId = 1;
    std::unordered_map<std::string,uint32_t> _udpClientMap;

    // --- CLIENT MEMBERS ---
    TCPsocket               _tcpSock      = nullptr;
    UDPsocket               _udpsockClient= nullptr;
    UDPpacket*              _udpPacketC   = nullptr;
    uint32_t                _myClientId   = 0;
    uint32_t                _myToken      = 0;
    IPaddress               _serverUdpAddr{};

    // ---- INITIALIZATION ----
    void initServer()
    {
        LOG_INFO(DOM, "Initializing server...");

        // 1) TCP listener on cfg.tcpPort
        IPaddress tcpAddr{};
        // nullptr host means “bind all interfaces”
        if (SDLNet_ResolveHost(&tcpAddr, nullptr, netCfg.tcpPort) < 0)
        {
            LOG_ERROR(DOM, "SDLNet_ResolveHost (TCP) failed: " << SDLNet_GetError());
            return;
        }

        _tcpListener = SDLNet_TCP_Open(&tcpAddr);
        if (not _tcpListener)
        {
            LOG_ERROR(DOM, "SDLNet_TCP_Open failed: " << SDLNet_GetError());
            return;
        }

        // 2) UDP socket on cfg.udpLocalPort
        _udpSock = SDLNet_UDP_Open(netCfg.udpLocalPort);
        if (not _udpSock)
        {
            LOG_ERROR(DOM, "SDLNet_UDP_Open failed: " << SDLNet_GetError());
            return;
        }

        // 3) Allocate inbound packet buffer
        _udpPacket = SDLNet_AllocPacket(MAX_PAYLOAD + sizeof(UdpHeader));
        if (not _udpPacket)
        {
            LOG_ERROR(DOM, "SDLNet_AllocPacket failed");
            return;
        }

        LOG_INFO(DOM, "Server initialized: TCP port=" << netCfg.tcpPort << ", UDP port=" << netCfg.udpLocalPort);
    }

    void initClient()
    {
        IPaddress tcpAddr{};
        SDLNet_ResolveHost(&tcpAddr, netCfg.peerAddress.c_str(), netCfg.tcpPort);

        // retry loop
        if (not _tcpSock)
        {
            _tcpSock = SDLNet_TCP_Open(&tcpAddr);
            if (not _tcpSock)
            {
                LOG_WARNING(DOM, "TCP connect failed, retrying: " << SDLNet_GetError());
                connectedToServer = false;
                return;
            }
        }

        connectedToServer = true;

        // receive <id,token>
        uint8_t buf[8];
        SDLNet_TCP_Recv(_tcpSock, buf, 8);
        _myClientId = SDLNet_Read32(buf);
        _myToken    = SDLNet_Read32(buf+4);

        // setup UDP
        _udpsockClient = SDLNet_UDP_Open(netCfg.udpLocalPort);

        SDLNet_ResolveHost(&_serverUdpAddr, netCfg.peerAddress.c_str(), netCfg.udpPeerPort);

        _udpPacketC = SDLNet_AllocPacket(MAX_PAYLOAD + sizeof(UdpHeader));

        // send UDP handshake
        const char* hello = "HELLO";

        UdpHeader h{_myClientId, _myToken, static_cast<uint16_t>(5)};

        writeHeader(_udpPacketC->data, h);
        memcpy(_udpPacketC->data + sizeof(UdpHeader), hello, 5);

        _udpPacketC->address = _serverUdpAddr;
        _udpPacketC->len     = sizeof(UdpHeader) + 5;

        SDLNet_UDP_Send(_udpsockClient, -1, _udpPacketC);
    }

    // ---- PER-FRAME LOGIC ----
    void runServerFrame(float dt)
    {
        // A) Accept new TCP clients
        TCPsocket newC = SDLNet_TCP_Accept(_tcpListener);
        if (newC)
        {
            ClientInfo ci;
            ci.tcpSock  = newC;
            ci.clientId = _nextClientId++;
            ci.token    = genToken();
            _clients[newC] = ci;
            _idToTcp[ci.clientId] = newC;

            // send <id,token>
            uint8_t w[8];
            SDLNet_Write32(ci.clientId, w);
            SDLNet_Write32(ci.token,    w+4);
            SDLNet_TCP_Send(newC, w, 8);
        }

        // B) Handle UDP packets
        int recv = SDLNet_UDP_Recv(_udpSock, _udpPacket);
        if (recv > 0)
        {
            UdpHeader h = readHeader(_udpPacket->data);
            auto itId = _idToTcp.find(h.clientId);

            if (itId == _idToTcp.end())
                return;

            auto& ci = _clients[itId->second];

            if (ci.token != h.token)
                return;

            // first time link
            if (not ci.udpLinked)
            {
                ci.udpAddr   = _udpPacket->address;
                ci.udpLinked = true;

                std::string key = ipPortKey(ci.udpAddr);

                _udpClientMap[key] = ci.clientId;

                LOG_INFO(DOM, "UDP linked client " << ci.clientId << " @ " << key);
            }

            // echo back
            uint8_t out[MAX_PAYLOAD + sizeof(UdpHeader)];
            UdpHeader outH{ci.clientId, ci.token, h.payloadLen};
            writeHeader(out, outH);
            memcpy(out + sizeof(UdpHeader), _udpPacket->data + sizeof(UdpHeader), h.payloadLen);
            UDPpacket opkt;
            opkt.address = ci.udpAddr;
            opkt.data    = out;
            opkt.len     = sizeof(UdpHeader) + h.payloadLen;
            SDLNet_UDP_Send(_udpSock, -1, &opkt);
        }
    }

    void runClientFrame(float dt)
    {
        if (not connectedToServer)
        {
            if (timeBeforeReconnect > 5000.0f)
            {
                LOG_WARNING(DOM, "Reconnecting to server...");
                initClient();
                timeBeforeReconnect = 0.0f;
            }
            else
            {
                timeBeforeReconnect += dt;
                return;
            }
        }

        int rec = SDLNet_UDP_Recv(_udpsockClient, _udpPacketC);

        if (rec > 0)
        {
            UdpHeader h = readHeader(_udpPacketC->data);
            // should match ours

            if (h.clientId != _myClientId or h.token != _myToken)
                return;

            std::string msg(
                reinterpret_cast<char*>(_udpPacketC->data + sizeof(UdpHeader)),
                h.payloadLen
            );
            SDL_Log("CLIENT echo: %s", msg.c_str());
        }
        // you can also trigger sends here…
    }
};


void initGame(int argc, char** argv)
{
    printf("Initializing engine ...\n");

#ifdef __EMSCRIPTEN__
        EM_ASM(
            console.error("Syncing... !");
            FS.mkdir('/save');
            console.error("Syncing... !");
            FS.mount(IDBFS, {autoPersist: true}, '/save');
            console.error("Syncing... !");
            FS.syncfs(true, function (err) {
                console.error("Synced !");
                if (err) {
                    console.error("Initial sync error:", err);
                }
            });
            console.error("Syncing... !");
        );
#endif

    mainWindow->initEngine();

    // ——— 1) Parse mode ———
    bool isServer = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a == "--mode=server") isServer = true;
        if (a == "--mode=client") isServer = false;
    }

    mainWindow->ecs.createSystem<NetworkSystem>(isServer);

    // SDL_Quit();

    printf("Engine initialized ...\n");

    mainWindow->ecs.dumbTaskflow();

    mainWindow->render();

    mainWindow->resize(820, 640);

    mainWindow->ecs.start();

    printf("Engine initialized\n");
}

// New function for syncing manually when needed
void syncFilesystem() {
#ifdef __EMSCRIPTEN__
    EM_ASM(
        FS.syncfs(false, function (err) {
            if (err) {
                console.error("Sync error:", err);
            } else {
                console.log("Filesystem synced.");
            }
        });
    );
#endif
}

void mainloop(int argc, char** argv, void *arg)
{
    if (not initialized.load())
        return;

    if (not init) {
        if (initThread) {
            printf("Joining thread...\n");

            initThread->join();

            delete initThread;

            printf("Thread joined...\n");
        }

        init = true;

        mainWindow->init(820, 640, false, static_cast<SDL_Window *>(arg));

        printf("Window init done !\n");

        initGame(argc, argv);
    }

    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        mainWindow->processEvents(event);
    }

    mainWindow->render();

#ifdef __EMSCRIPTEN__
    // Sync file system at a specific point instead of every frame
    if (event.type == SDL_QUIT)
    {
        syncFilesystem();
    }
#endif

    if (mainWindow->requestQuit()) {
        LOG_ERROR("Window", "RequestQuit");
        std::terminate();
    }
}

int GameApp::exec(int argc, char** argv)
{
#ifdef __EMSCRIPTEN__
    printf("Start init thread...\n");
    initThread = new std::thread(initWindow, appName);
    printf("Detach init thread...\n");

    SDL_Window *pWindow =
    SDL_CreateWindow("Hello Triangle Minimal",
                        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                        820, 640,
                        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);

    emscripten_set_main_loop_arg(mainloop, argc, argv, pWindow, 0, 1);

#else
    LOG_THIS_MEMBER(DOM);

    initWindow(appName);

    mainWindow->init(820, 640, false);

    LOG_INFO(DOM, "Window init done !");

    initGame(argc, argv);

    while (running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            mainWindow->processEvents(event);
        }

        mainWindow->render();

        if (mainWindow->requestQuit())
            break;
    }

    delete mainWindow;
#endif

    return 0;
}
