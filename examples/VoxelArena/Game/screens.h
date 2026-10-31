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
        bool quitArmed = false;         // Quit clicked once: the next click quits
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
        bool canLeave = false;          // a Leave (or Stop hosting) back to playing alone
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
        bool quitArmed = false;
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

    /** Joining a friend's game (the title's J): the host's address and the name the others see. */
    struct JoinView
    {
        std::string address, name, message;
        int focus = 0;                  // 0 the address, 1 the name
        float time = 0.0f;
    };

    /** The weapon choice before the first wave (the armory): one card per weapon, its skills. */
    struct ArmoryView
    {
        struct Weapon
        {
            std::string name, role, text, icon;
            std::vector<std::string> skills;
            bool chosen = false;
        };

        std::vector<Weapon> weapons;
        bool picked = false;            // this player took one: waiting for the others
        int ready = 0, players = 1;
    };

    namespace screens
    {
        // Click actions
        constexpr int TITLE_ARENA = 1, TITLE_TRAINING = 2, TITLE_SPELLBOOK = 3, TITLE_OPTIONS = 4;
        constexpr int END_AGAIN = 11, END_TRAINING = 12;
        constexpr int LOBBY_READY = 5;   // the host's start is TITLE_ARENA
        constexpr int TITLE_INVITE = 6, TITLE_JOIN = 7, LOBBY_LEAVE = 8, TITLE_QUIT = 9;

        /** Quit game, at the bottom of the title and the lobby (a second click quits). */
        void drawQuit(Ui& ui, float x, float y, bool armed);
        constexpr int INVITE_JOIN = 26;
        constexpr int ARMORY_PICK = 600;   // + the weapon's index
        void drawArmory(Ui& ui, const ArmoryView& v);

        constexpr int JOIN_ADDRESS = 31, JOIN_NAME = 32, JOIN_PASTE = 33, JOIN_GO = 34, JOIN_CLOSE = 35;

        void drawJoin(Ui& ui, const JoinView& v);
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
