#pragma once

#include <string>
#include <vector>

#include "ui.h"

namespace arena
{
    /** The title screen (UI_SPEC.md section 8): the game's name, what to play, the best score. */
    struct TitleView
    {
        int waves = 20;
        int best = 0;
        bool waitingForHost = false;    // a client: only the host starts the run
        int players = 1, maxPlayers = 8;
        float time = 0.0f;
    };

    /** The end of a run (section 9). */
    struct EndView
    {
        bool won = false;
        int wave = 1, waves = 20;
        int score = 0, best = 0, kills = 0, level = 1;
        float time = 0.0f;              // s the run lasted
        bool solo = true;               // the training ground is offered alone only
        std::string boss = "the Stone Warden";
        std::string champion;           // the final duel's winner ("" when there was none)
        bool youChampion = false;
    };

    /** The lobby (multiplayer's title): who is in, ready or not, how friends join, start or ready. */
    struct LobbyView
    {
        struct Row
        {
            std::string name;
            bool host = false, you = false, ready = false;
            int pingMs = 0;
        };

        bool isHost = false;            // this game hosts it
        bool dedicated = false;         // a dedicated server: no host, it starts when everyone is ready
        bool youReady = false;
        std::vector<Row> players;
        int maxPlayers = 8;

        std::string status;             // a client not in yet: "Connecting...", "Refused: ..."
        bool statusBad = false;

        // How to join: the host's addresses and its router; a client, where it joined
        std::string lanAddress, externalAddress, joined;
        int port = 7777;
        enum class Router { Off, Searching, Open, Closed } router = Router::Off;
        std::string routerError;

        std::vector<std::string> log;   // the latest joins, leaves and refusals
        int waves = 20;
        float time = 0.0f;
    };

    /** Inviting friends (the + of the title and the lobby): the address to share, or hosting first. */
    struct InviteView
    {
        bool hosting = false;           // this game hosts: its addresses
        bool joined = false;            // this game joined one: the address it joined
        int players = 1;
        std::string lanAddress, externalAddress, routerError, message;
        int port = 7777;
        LobbyView::Router router = LobbyView::Router::Off;
    };

    namespace screens
    {
        // Click actions
        constexpr int TITLE_ARENA = 1, TITLE_TRAINING = 2, TITLE_SPELLBOOK = 3, TITLE_OPTIONS = 4;
        constexpr int END_AGAIN = 11, END_TRAINING = 12;
        constexpr int LOBBY_READY = 5;   // the host's start is TITLE_ARENA
        constexpr int TITLE_INVITE = 6;
        constexpr int INVITE_HOST = 21, INVITE_COPY_ONLINE = 22, INVITE_COPY_LAN = 23, INVITE_COPY_JOIN = 24, INVITE_CLOSE = 25;

        /** The + at the top left of the title and the lobby. */
        void drawInviteButton(Ui& ui, float time);
        void drawInvite(Ui& ui, const InviteView& v);

        void drawLobby(Ui& ui, const LobbyView& v);

        void drawTitle(Ui& ui, const TitleView& v);
        void drawEnd(Ui& ui, const EndView& v);

        /** 7073 as "7 073". */
        std::string grouped(int value);
    }
}
