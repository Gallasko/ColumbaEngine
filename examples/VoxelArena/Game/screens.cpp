#include "stdafx.h"

#include "screens.h"

#include <algorithm>
#include <cmath>

namespace arena::screens
{
    using namespace ui;

    namespace
    {
        const Colour ARENA_LINE = hex(0x5A4718);
        const Colour SLOT_DASH = hex(0x4A4F5C);
        const Colour SUBTITLE = hex(0xDDE0E8);
        const Colour DEFEAT_WASH = hex(0x280A0A, 0.6f);
    }

    std::string grouped(int value)
    {
        std::string digits = std::to_string(std::abs(value));
        std::string out;

        for (size_t i = 0; i < digits.size(); ++i)
        {
            if (i > 0 and (digits.size() - i) % 3 == 0)
                out += ' ';
            out += digits[i];
        }

        return (value < 0 ? "-" : "") + out;
    }

    void drawQuit(Ui& ui, float x, float y, bool armed)
    {
        const std::string label = armed ? "Click again to quit" : "Quit game";
        const float w = ui.measure(label, Font::Display, 20) + 24.0f;
        const bool over = ui.hover(x, y, w, 32);
        ui.box(x, y, w, 32, armed ? hex(0x3A1614) : CARD, 2.0f, armed or over ? WARNING : LINE);
        ui.text(x + w * 0.5f, y + 6, label, Font::Display, 20, armed or over ? WARNING : TEXT_DIM, Align::Centre);
        ui.hit(x, y, w, 32, TITLE_QUIT);
    }

    void drawInviteButton(Ui& ui, float time)
    {
        (void)time;
        ui.anchor(Anchor::TopLeft);
        const bool over = ui.hover(24, 20, 40, 40);
        ui.raised(24, 20, 40, 40, over ? GOLD : CARD, 3.0f, 2.0f, over ? TEXT : GOLD);
        ui.text(44, 22, "+", Font::Display, 34, over ? GOLD_INK : GOLD, Align::Centre);
        const float lw = ui.textShadow(74, 33, "INVITE", Font::Text700, 11, over ? GOLD : TEXT_DIM, 2.0f, Align::Left, 1.5f);
        ui.frame(74 + lw + 8, 30, 18, 18, 2, FAINT);
        ui.text(74 + lw + 17, 31, "I", Font::Display, 15, FAINT, Align::Centre);
        ui.hit(24, 20, 40, 40, TITLE_INVITE);
        ui.hit(64, 20, lw + 40, 40, TITLE_INVITE);
    }

    void drawInvite(Ui& ui, const InviteView& v)
    {
        ui.section("invite");
        ui.anchor(Anchor::TopLeft);
        ui.rectPx(0, 0, ui.width(), ui.height(), withAlpha(INK, 0.6f));
        ui.hit(0, 0, ui.baseX(ui.width()), ui.baseY(ui.height()), INVITE_CLOSE);   // a click outside closes it

        ui.anchor(Anchor::Centre);
        const float x = 330, y = 150, w = 620, h = 420;
        ui.raised(x, y, w, h, PANEL, 4.0f);
        ui.hit(x, y, w, h, -1);   // the panel itself takes the click (nothing happens)

        ui.text(x + 28, y + 22, v.hosting or v.joined ? "Invite friends" : "Multiplayer", Font::Display, 40, TEXT);
        {
            const float cw = 22.0f + 8.0f + ui.measure("Close", Font::Text600, 13) + 18.0f;
            const bool over = ui.hover(x + w - 28 - cw, y + 26, cw, 30);
            ui.box(x + w - 28 - cw, y + 26, cw, 30, CARD, 2.0f, over ? GOLD : LINE);
            const float kw = ui.measure("ESC", Font::Display, 13) + 8.0f;
            ui.box(x + w - 28 - cw + 6, y + 31, kw, 20, TEXT_DIM, 0.0f);
            ui.text(x + w - 28 - cw + 6 + kw * 0.5f, y + 32, "ESC", Font::Display, 13, INK, Align::Centre);
            ui.text(x + w - 28 - cw + 12 + kw, y + 33, "Close", Font::Text600, 13, TEXT);
            ui.hit(x + w - 28 - cw, y + 26, cw, 30, INVITE_CLOSE);
        }

        const std::string port = ":" + std::to_string(v.port);
        float ry = y + 82.0f;

        // A row: its label, the address in big, a Copy button
        auto row = [&](const std::string& label, const std::string& address, const std::string& note, const Colour& noteColour, int copyAction) {
            ui.box(x + 28, ry, w - 56, 64, WELL);
            ui.text(x + 44, ry + 10, label, Font::Text700, 11, MUTED, Align::Left, 1.5f);
            ui.text(x + 44, ry + 26, ui.fit(address, Font::Display, 28, w - 56 - 140), Font::Display, 28, address.empty() ? FAINT : TEXT);
            if (not note.empty())
                ui.text(x + 44 + ui.measure(label, Font::Text700, 11, 1.5f) + 10, ry + 11, ui.fit(note, Font::Text600, 10, 360), Font::Text600, 10, noteColour);

            if (copyAction != 0)
            {
                const float bx = x + w - 28 - 16 - 92;
                const bool over = ui.hover(bx, ry + 16, 92, 32);
                ui.raised(bx, ry + 16, 92, 32, GOLD, 3.0f, 2.0f, over ? TEXT : INK);
                ui.text(bx + 46, ry + 20, "Copy", Font::Display, 22, GOLD_INK, Align::Centre);
                ui.hit(bx, ry + 16, 92, 32, copyAction);
            }
            ry += 74.0f;
        };

        if (not v.hosting and not v.joined)
        {
            // Alone: host this game (it becomes the host right here), or join a friend's
            ui.paragraph(x + 28, ry, w - 56, "Play with friends: host a game here and they join you, or join the game a friend hosts. "
                         "Everyone plays with the host's mods.", Font::Text500, 14, TEXT_DIM, 20.0f);
            ry += 64.0f;

            auto choice = [&](float by, const std::string& key, const std::string& name, const std::string& line, int action) {
                const auto choiceScope = ui.scope(static_cast<uint64_t>(action));
                const bool over = ui.hover(x + 28, by, w - 56, 84);
                ui.raised(x + 28, by, w - 56, 84, GOLD, 3.0f, 2.0f, over ? TEXT : INK);
                ui.keycap(x + 46, by + 26, 30, key, INK, GOLD, 22, 0.0f);
                ui.text(x + 92, by + 14, name, Font::Display, 34, GOLD_INK);
                ui.text(x + 92, by + 54, line, Font::Text600, 12, hex(0x5A4718));
                ui.hit(x + 28, by, w - 56, 84, action);
            };

            choice(ry, "H", "Host a game", "your friends join you, on port " + std::to_string(v.port) + " and " + std::to_string(v.port + 1), INVITE_HOST);
            ry += 98.0f;
            choice(ry, "J", "Join a game", "a friend's, with the address their invite shows", INVITE_JOIN);
            ry += 98.0f;
        }
        else if (v.joined)
        {
            row("THE GAME YOU JOINED", v.lanAddress + port, "friends join it the same way", MUTED, INVITE_COPY_LAN);
        }
        else
        {
            switch (v.router)
            {
                case LobbyView::Router::Open:
                    row("ONLINE", (v.externalAddress.empty() ? "?" : v.externalAddress) + ":" + std::to_string(v.externalPort ? v.externalPort : v.port),
                        v.externalPort and v.externalPort != v.port ? "your router opened " + std::to_string(v.externalPort) + " (to this game's " + std::to_string(v.port) + ")"
                                                                  : std::string("your router opened the ports"),
                        GOOD, v.externalAddress.empty() ? 0 : INVITE_COPY_ONLINE);
                    break;
                case LobbyView::Router::Searching:
                    row("ONLINE", "", "looking for your router\xE2\x80\xA6", MUTED, 0);
                    break;
                default:
                    row("ONLINE", "", "forward TCP " + std::to_string(v.port) + " and UDP " + std::to_string(v.port + 1) + " to " + v.lanAddress + " on your router", WARNING, 0);
                    break;
            }
            row("SAME NETWORK", v.lanAddress + port, "", MUTED, INVITE_COPY_LAN);
        }

        if (v.hosting or v.joined)
        {
            ui.text(x + 28, ry + 4, "They start the game with", Font::Text600, 12, MUTED);
            const bool over = ui.hover(x + 28, ry + 22, w - 56, 34);
            ui.box(x + 28, ry + 22, w - 56, 34, CARD, 2.0f, over ? GOLD : LINE);
            ui.text(x + 42, ry + 30, "VoxelArena --join <address>", Font::Mono, 13, TEXT);
            ui.text(x + w - 42, ry + 31, "copy the command", Font::Text600, 11, over ? GOLD : FAINT, Align::Right);
            ui.hit(x + 28, ry + 22, w - 56, 34, INVITE_COPY_JOIN);
        }

        if (not v.message.empty())
            ui.text(x + 28, y + h - 34, ui.fit(v.message, Font::Text600, 12, w - 56), Font::Text600, 12, v.message.rfind("Copied", 0) == 0 ? GOOD : WARNING);
        else if (v.hosting)
            ui.text(x + 28, y + h - 34, std::to_string(v.players) + (v.players == 1 ? " player" : " players") + " in \xC2\xB7 everyone needs the same mods", Font::Text600, 12, FAINT);
    }

    void drawControls(Ui& ui, const ControlsView& v)
    {
        ui.section("controls");
        ui.anchor(Anchor::TopLeft);
        ui.rectPx(0, 0, ui.width(), ui.height(), withAlpha(INK, 0.4f));
        ui.hit(0, 0, ui.baseX(ui.width()), ui.baseY(ui.height()), -1);

        ui.anchor(Anchor::Centre);
        ui.raised(190, 56, 900, 608, PANEL, 4.0f);

        const float tw = ui.text(218, 76, "Controls", Font::Display, 44, TEXT);
        ui.text(218 + tw + 14, 96, "click a key, then press the new one (a key or a mouse button); Esc cancels", Font::Text500, 12, MUTED);

        for (size_t i = 0; i < v.rows.size(); ++i)
        {
            const auto itemScope = ui.scope(i);
            const auto& r = v.rows[i];
            const bool right = static_cast<int>(i) >= v.split;
            const size_t k = right ? i - static_cast<size_t>(v.split) : i;
            const float x = right ? 652.0f : 218.0f, y = 140.0f + static_cast<float>(k) * 48.0f;

            ui.text(x, y + 10, r.label, Font::Text600, 15, r.waiting ? GOLD : TEXT_DIM);

            const float bx = x + 270.0f, bw = 130.0f;
            const bool over = ui.hover(bx, y, bw, 36);
            ui.box(bx, y, bw, 36, r.waiting ? hex(0x2A2414) : WELL, 2.0f, r.waiting ? GOLD : (over ? TEXT_DIM : LINE));
            ui.text(bx + bw * 0.5f, y + 6, r.waiting ? "press a key" : ui.fit(r.key, Font::Display, 22, bw - 16), Font::Display, 22, r.waiting ? GOLD : TEXT, Align::Centre);
            if (not r.also.empty() and not r.waiting)
                ui.text(bx - 10, y + 12, "or " + r.also, Font::Text600, 11, FAINT, Align::Right);
            ui.hit(bx, y, bw, 36, CONTROLS_ROW + static_cast<int>(i));
        }

        {
            const float w = ui.measure("Reset to defaults", Font::Display, 22) + 28.0f;
            const bool over = ui.hover(218, 600, w, 42);
            ui.box(218, 600, w, 42, CARD, 2.0f, over ? GOLD : LINE);
            ui.text(218 + w * 0.5f, 608, "Reset to defaults", Font::Display, 22, TEXT_DIM, Align::Centre);
            ui.hit(218, 600, w, 42, CONTROLS_RESET);
        }

        if (not v.message.empty())
            ui.text(470, 613, ui.fit(v.message, Font::Text600, 12, 420), Font::Text600, 12, GOOD);

        const bool over = ui.hover(912, 600, 150, 42);
        ui.raised(912, 600, 150, 42, GOLD, 3.0f, 2.0f, over ? TEXT : INK);
        const float kw = ui.measure("ESC", Font::Display, 15) + 10.0f;
        ui.box(924, 610, kw, 22, INK, 0.0f);
        ui.text(924 + kw * 0.5f, 612, "ESC", Font::Display, 15, GOLD, Align::Centre);
        ui.text(924 + kw + 10, 607, "Back", Font::Display, 24, GOLD_INK);
        ui.hit(912, 600, 150, 42, CONTROLS_BACK);
    }

    void drawArmory(Ui& ui, const ArmoryView& v)
    {
        ui.section("armory");
        ui.anchor(Anchor::TopLeft);
        ui.rectPx(0, 0, ui.width(), ui.height(), withAlpha(INK, 0.58f));

        ui.anchor(Anchor::Centre);
        ui.raised(40, 60, 1200, 600, PANEL, 4.0f);

        ui.text(66, 84, "CHOOSE YOUR WEAPON", Font::Display, 40, TEXT, Align::Left, 1.0f);
        ui.text(66, 128, "It is yours for the whole run: the shop sells its skills, not other weapons.", Font::Text500, 13, TEXT_DIM);

        if (v.players > 1)
            ui.text(1214, 98, std::to_string(v.ready) + " / " + std::to_string(v.players) + " chose", Font::Display, 26, v.picked ? GOOD : TEXT_DIM, Align::Right);

        const size_t n = std::max<size_t>(1, v.weapons.size());
        const float gap = 12.0f;
        const float cw = std::floor((1148.0f - gap * static_cast<float>(n - 1)) / static_cast<float>(n));

        for (size_t i = 0; i < v.weapons.size(); ++i)
        {
            const auto itemScope = ui.scope(i);
            const auto& w = v.weapons[i];
            const float x = 66.0f + static_cast<float>(i) * (cw + gap), y = 160.0f, h = 470.0f;
            const bool over = ui.hover(x, y, cw, h);

            Colour role = KIND_WEAPON;
            if (w.role == "ranged") role = KIND_STAT;
            if (w.role == "support") role = GOOD;

            ui.raised(x, y, cw, h, CARD, 3.0f, 2.0f, w.chosen ? GOLD : (over ? TEXT_DIM : INK));

            float cy = y + 14.0f;
            ui.keycap(x + 14, cy, 22, std::to_string(i + 1), GOLD, GOLD_INK, 18, 0.0f);
            ui.rect(x + 46, cy + 7, 8, 8, role);
            ui.text(x + 60, cy + 4, w.role.empty() ? "WEAPON" : w.role, Font::Text700, 11, role, Align::Left, 1.5f);
            cy += 36.0f;

            ui.box(x + 14, cy, 60, 60, WELL, 2.0f, role);
            ui.icon(w.icon, x + 23, cy + 9, 6.0f, role);
            cy += 72.0f;

            ui.text(x + 14, cy, ui.fit(w.name, Font::Display, 30, cw - 28), Font::Display, 30, TEXT);
            cy += 40.0f;

            cy += ui.paragraph(x + 14, cy, cw - 28, w.text, Font::Text500, 13, TEXT_DIM, 18.0f, 6) + 12.0f;

            ui.text(x + 14, cy, "ITS SKILLS IN THE SHOP", Font::Text700, 10, MUTED, Align::Left, 1.5f);
            cy += 18.0f;
            if (w.skills.empty())
                ui.text(x + 14, cy, "none", Font::Text600, 12, FAINT);
            for (size_t k = 0; k < w.skills.size() and k < 4; ++k)
            {
                ui.rect(x + 14, cy + 5, 6, 6, KIND_SKILL);
                ui.text(x + 26, cy, w.skills[k], Font::Text600, 12, TEXT);
                cy += 18.0f;
            }

            // Take it (or taken)
            const float by = y + h - 14 - 44;
            if (w.chosen)
            {
                ui.box(x + 14, by, cw - 28, 44, hex(0x16261C), 2.0f, INK);
                ui.icon("check", x + 26, by + 15, 2.0f, GOOD);
                ui.text(x + 46, by + 9, "TAKEN", Font::Display, 24, GOOD);
            }
            else
            {
                ui.raised(x + 14, by, cw - 28, 44, GOLD, 3.0f, 2.0f, over ? TEXT : INK);
                ui.text(x + cw * 0.5f, by + 9, "Take it", Font::Display, 26, GOLD_INK, Align::Centre);
            }

            ui.hit(x, y, cw, h, ARMORY_PICK + static_cast<int>(i));
        }

        ui.text(66, 640, v.picked and v.players > 1 ? "Waiting for the others to choose\xE2\x80\xA6 (you can still change your mind)"
                                                  : "1-" + std::to_string(n) + " or a click to take one", Font::Text500, 12, FAINT);
    }

    void drawJoin(Ui& ui, const JoinView& v)
    {
        ui.section("join");
        ui.anchor(Anchor::TopLeft);
        ui.rectPx(0, 0, ui.width(), ui.height(), withAlpha(INK, 0.6f));
        ui.hit(0, 0, ui.baseX(ui.width()), ui.baseY(ui.height()), JOIN_CLOSE);

        ui.anchor(Anchor::Centre);
        const float x = 330, y = 140, w = 620, h = 440;
        ui.raised(x, y, w, h, PANEL, 4.0f);
        ui.hit(x, y, w, h, -1);

        ui.text(x + 28, y + 22, "Join a game", Font::Display, 40, TEXT);
        {
            const float cw = 22.0f + 8.0f + ui.measure("Close", Font::Text600, 13) + 18.0f;
            const bool over = ui.hover(x + w - 28 - cw, y + 26, cw, 30);
            ui.box(x + w - 28 - cw, y + 26, cw, 30, CARD, 2.0f, over ? GOLD : LINE);
            const float kw = ui.measure("ESC", Font::Display, 13) + 8.0f;
            ui.box(x + w - 28 - cw + 6, y + 31, kw, 20, TEXT_DIM, 0.0f);
            ui.text(x + w - 28 - cw + 6 + kw * 0.5f, y + 32, "ESC", Font::Display, 13, INK, Align::Centre);
            ui.text(x + w - 28 - cw + 12 + kw, y + 33, "Close", Font::Text600, 13, TEXT);
            ui.hit(x + w - 28 - cw, y + 26, cw, 30, JOIN_CLOSE);
        }

        const bool caret = static_cast<int>(v.time * 2.0f) % 2 == 0;

        // A field: its label, the box (gold when it has the keyboard), what is typed
        auto field = [&](float fy, const std::string& label, const std::string& value, const std::string& placeholder, bool focused, int action, float fw) {
            ui.text(x + 28, fy, label, Font::Text700, 11, MUTED, Align::Left, 1.5f);
            const bool over = ui.hover(x + 28, fy + 18, fw, 44);
            ui.box(x + 28, fy + 18, fw, 44, WELL, 2.0f, focused ? GOLD : (over ? TEXT_DIM : LINE));
            const std::string shown = value.empty() and not focused ? placeholder : value + (focused and caret ? "_" : "");
            ui.text(x + 44, fy + 26, ui.fit(shown, Font::Display, 26, fw - 32), Font::Display, 26, value.empty() and not focused ? FAINT : TEXT);
            ui.hit(x + 28, fy + 18, fw, 44, action);
        };

        field(y + 86, "THE HOST'S ADDRESS", v.address, "81.2.3.4:7777", v.focus == 0, JOIN_ADDRESS, w - 56 - 110);
        {
            const float bx = x + w - 28 - 100;
            const bool over = ui.hover(bx, y + 104, 100, 44);
            ui.box(bx, y + 104, 100, 44, CARD, 2.0f, over ? GOLD : LINE);
            ui.text(bx + 50, y + 111, "Paste", Font::Display, 24, TEXT, Align::Centre);
            ui.hit(bx, y + 104, 100, 44, JOIN_PASTE);
        }
        ui.text(x + 28, y + 156, "As their invite shows it (the + at the top left of their game). The port is 7777 unless they said otherwise.",
                Font::Text500, 11, FAINT);

        field(y + 186, "YOUR NAME", v.name, "player", v.focus == 1, JOIN_NAME, 300);
        ui.text(x + 340, y + 226, "Tab: the other field", Font::Text500, 11, FAINT);

        const float by = y + 286;
        const bool over = ui.hover(x + 28, by, w - 56, 64);
        ui.raised(x + 28, by, w - 56, 64, GOLD, 3.0f, 2.0f, over ? TEXT : INK);
        const float kw = ui.measure("ENTER", Font::Display, 18) + 12.0f;
        ui.box(x + 46, by + 19, kw, 26, INK, 0.0f);
        ui.text(x + 46 + kw * 0.5f, by + 22, "ENTER", Font::Display, 18, GOLD, Align::Centre);
        ui.text(x + 46 + kw + 14, by + 12, "Join", Font::Display, 32, GOLD_INK);
        ui.text(x + w - 44, by + 25, "everyone needs the same mods", Font::Text600, 12, hex(0x5A4718), Align::Right);
        ui.hit(x + 28, by, w - 56, 64, JOIN_GO);

        if (not v.message.empty())
            ui.text(x + 28, y + h - 36, ui.fit(v.message, Font::Text600, 12, w - 56), Font::Text600, 12, WARNING);
    }

    void drawTitle(Ui& ui, const TitleView& v)
    {
        ui.section("title");

        // A dark band on the left, ending in two hard steps
        ui.anchor(Anchor::Left);
        ui.rectPx(0, 0, ui.X(620), ui.height(), withAlpha(INK, 0.82f));
        ui.rectPx(ui.X(620), 0, ui.X(628), ui.height(), withAlpha(INK, 0.6f));
        ui.rectPx(ui.X(628), 0, ui.X(636), ui.height(), withAlpha(INK, 0.35f));

        ui.anchor(Anchor::Left);

        // The logo: a row of blocks in the game's colours, then the name
        const Colour blocks[] = {HEALTH, STAMINA, MANA, GOLD, SHARDS, FIRE, ICE, VOLT};
        for (int k = 0; k < 8; ++k)
            ui.box(72.0f + static_cast<float>(k) * 23.0f, 72, 18, 18, blocks[k]);

        ui.textShadow(72, 96, "VOXEL", Font::Display, 112, TEXT, 5.0f, Align::Left, 2.0f);
        ui.textShadow(72, 186, "ARENA", Font::Display, 112, GOLD, 5.0f, Align::Left, 2.0f);

        // The arena (or waiting for the host)
        if (v.waitingForHost)
        {
            ui.rect(72, 318, 472, 78, CARD);
            ui.dashed(72, 318, 472, 78, 2, 6, SLOT_DASH);
            for (int k = 0; k < 3; ++k)
            {
                const bool lit = static_cast<int>(v.time * 3.0f) % 3 == k;
                ui.rect(92.0f + static_cast<float>(k) * 16.0f, 336, 10, 10, lit ? GOLD : LINE);
            }
            ui.text(146, 326, "Waiting for the host\xE2\x80\xA6", Font::Display, 30, TEXT);
            ui.text(92, 364, std::to_string(v.players) + " of " + std::to_string(v.maxPlayers) + " players in \xC2\xB7 only the host can start", Font::Text500, 13, MUTED);
        }
        else
        {
            const bool over = ui.hover(72, 318, 472, 78);
            ui.raised(72, 318, 472, 78, GOLD, 3.0f, 2.0f, over ? TEXT : INK);
            const float kw = ui.measure("ENTER", Font::Display, 18) + 12.0f;
            ui.box(90, 332, kw, 26, INK, 0.0f);
            ui.text(90 + kw * 0.5f, 335, "ENTER", Font::Display, 18, GOLD, Align::Centre);
            ui.text(90 + kw + 14, 326, "The Arena", Font::Display, 34, GOLD_INK);
            ui.text(90, 368, std::to_string(v.waves) + " timed waves, a shop between them, bosses midway and at the end", Font::Text500, 13, ARENA_LINE);
            ui.hit(72, 318, 472, 78, TITLE_ARENA);
        }

        struct Item { std::string key, name, hint; int action; };
        const Item items[] = {
            {"T", "Training ground", "dummies, enemies that come back", TITLE_TRAINING},
            {"M", "Multiplayer", "host a game, or join a friend's", TITLE_MULTIPLAYER},
            {"B", "Spellbook", "build your spells", TITLE_SPELLBOOK},
            {"O", "Options", "mouse, view, sound", TITLE_OPTIONS},
        };

        for (int k = 0; k < 4; ++k)
        {
            const auto itemScope = ui.scope(static_cast<uint64_t>(k));
            const float y = 404.0f + static_cast<float>(k) * 52.0f;
            const bool over = ui.hover(72, y, 472, 46);
            ui.box(72, y, 472, 46, CARD, 2.0f, over ? GOLD : LINE);
            ui.keycap(84, y + 10, 26, items[k].key, GOLD, GOLD_INK, 20, 0.0f);
            ui.text(124, y + 8, items[k].name, Font::Display, 28, TEXT);
            ui.text(530, y + 16, items[k].hint, Font::Text500, 12, MUTED, Align::Right);
            ui.hit(72, y, 472, 46, items[k].action);
        }

        ui.text(72, 616, "BEST SCORE", Font::Text700, 11, MUTED, Align::Left, 1.5f);
        const float bw = ui.text(72, 630, grouped(v.best), Font::Display, 40, TEXT);
        ui.text(72 + bw + 12, 648, v.best > 0 ? "your best run so far" : "no run yet", Font::Text500, 12, MUTED);

        drawQuit(ui, 72, 672, v.quitArmed);
        ui.text(248, 682, "F1 overlay \xC2\xB7 ` console \xC2\xB7 Esc options", Font::Text500, 11, FAINT);
    }

    void drawLobby(Ui& ui, const LobbyView& v)
    {
        ui.section("lobby");

        ui.anchor(Anchor::Left);
        ui.rectPx(0, 0, ui.X(620), ui.height(), withAlpha(INK, 0.82f));
        ui.rectPx(ui.X(620), 0, ui.X(628), ui.height(), withAlpha(INK, 0.6f));
        ui.rectPx(ui.X(628), 0, ui.X(636), ui.height(), withAlpha(INK, 0.35f));

        drawInviteButton(ui, v.time);
        ui.anchor(Anchor::Left);

        const Colour blocks[] = {HEALTH, STAMINA, MANA, GOLD, SHARDS, FIRE, ICE, VOLT};
        for (int k = 0; k < 8; ++k)
            ui.box(72.0f + static_cast<float>(k) * 23.0f, 70, 18, 18, blocks[k]);

        const float vw = ui.textShadow(72, 92, "VOXEL", Font::Display, 56, TEXT, 4.0f, Align::Left, 2.0f);
        ui.textShadow(72 + vw + 16, 92, "ARENA", Font::Display, 56, GOLD, 4.0f, Align::Left, 2.0f);

        // ---- Who is in
        const float lw = ui.text(72, 164, "LOBBY", Font::Text700, 11, MUTED, Align::Left, 1.5f);
        if (v.status.empty())
            ui.text(72 + lw + 10, 163, std::to_string(v.players.size()) + " / " + std::to_string(v.maxPlayers) + " players", Font::Text600, 12, FAINT);

        if (v.canLeave)
        {
            const std::string label = v.isHost ? "Stop hosting" : "Leave";
            const float bw = ui.measure(label, Font::Text600, 12) + 20.0f;
            const bool over = ui.hover(544 - bw, 156, bw, 24);
            ui.box(544 - bw, 156, bw, 24, CARD, 2.0f, over ? WARNING : LINE);
            ui.text(544 - bw * 0.5f, 161, label, Font::Text600, 12, over ? WARNING : TEXT_DIM, Align::Centre);
            ui.hit(544 - bw, 156, bw, 24, LOBBY_LEAVE);
        }

        if (not v.status.empty())
        {
            // Not in yet (or not any more)
            ui.box(72, 184, 472, 64, CARD, 2.0f, v.statusBad ? WARNING : LINE);
            if (not v.statusBad)
                for (int k = 0; k < 3; ++k)
                    ui.rect(92.0f + static_cast<float>(k) * 16.0f, 211, 10, 10, static_cast<int>(v.time * 3.0f) % 3 == k ? GOLD : LINE);
            ui.text(v.statusBad ? 92.0f : 146.0f, 200, ui.fit(v.status, Font::Display, 26, v.statusBad ? 432.0f : 380.0f), Font::Display, 26, v.statusBad ? WARNING : TEXT);
        }
        else
        {
            for (size_t i = 0; i < v.players.size() and i < 8; ++i)
            {
                const auto itemScope = ui.scope(i);
                const auto& p = v.players[i];
                const float y = 184.0f + static_cast<float>(i) * 34.0f;
                ui.box(72, y, 472, 30, CARD, 2.0f, p.you ? TEXT_DIM : INK);

                float x = 86.0f + ui.text(86, y + 7, ui.fit(p.name, Font::Text700, 14, 200), Font::Text700, 14, TEXT) + 10.0f;
                auto tag = [&](const std::string& t, const Colour& fill, const Colour& ink) {
                    const float tw = ui.measure(t, Font::Display, 15) + 10.0f;
                    ui.rect(x, y + 8, tw, 17, fill);
                    ui.text(x + 5, y + 8, t, Font::Display, 15, ink);
                    x += tw + 6.0f;
                };
                if (p.host)
                    tag("HOST", GOLD, GOLD_INK);
                if (p.you)
                    tag("YOU", TEXT, INK);

                float right = 530.0f;
                if (not p.host)
                    right -= ui.text(right, y + 9, std::to_string(p.pingMs) + " ms", Font::Text600, 11, p.pingMs > 150 ? WARNING : MUTED, Align::Right) + 14.0f;

                if (p.host)
                    ui.text(right, y + 9, "starts the run", Font::Text600, 11, FAINT, Align::Right);
                else if (p.ready)
                {
                    const float rw = ui.text(right, y + 6, "READY", Font::Display, 20, GOOD, Align::Right);
                    ui.icon("check", right - rw - 20, y + 9, 2.0f, GOOD);
                }
                else
                    ui.text(right, y + 9, "not ready", Font::Text600, 11, MUTED, Align::Right);
            }
        }

        // ---- Start (the host) or ready (the others)
        int ready = 0, others = 0;
        for (const auto& p : v.players)
            if (not p.host)
            {
                ++others;
                ready += p.ready ? 1 : 0;
            }

        const float by = 472.0f;
        if (v.isHost)
        {
            const bool over = ui.hover(72, by, 472, 58);
            ui.raised(72, by, 472, 58, GOLD, 3.0f, 2.0f, over ? TEXT : INK);
            const float kw = ui.measure("ENTER", Font::Display, 18) + 12.0f;
            ui.box(90, by + 16, kw, 26, INK, 0.0f);
            ui.text(90 + kw * 0.5f, by + 19, "ENTER", Font::Display, 18, GOLD, Align::Centre);
            ui.text(90 + kw + 14, by + 8, "Start the arena", Font::Display, 30, GOLD_INK);
            ui.text(530, by + 21, others == 0 ? "alone so far" : std::to_string(ready) + " of " + std::to_string(others) + " ready", Font::Text600, 12, hex(0x5A4718), Align::Right);
            ui.hit(72, by, 472, 58, TITLE_ARENA);
        }
        else if (v.status.empty())
        {
            const bool over = ui.hover(72, by, 472, 58);
            if (v.youReady)
                ui.box(72, by, 472, 58, CARD, 2.0f, over ? GOLD : GOOD);
            else
                ui.raised(72, by, 472, 58, GOLD, 3.0f, 2.0f, over ? TEXT : INK);

            const float kw = ui.measure("ENTER", Font::Display, 18) + 12.0f;
            ui.box(90, by + 16, kw, 26, v.youReady ? TEXT_DIM : INK, 0.0f);
            ui.text(90 + kw * 0.5f, by + 19, "ENTER", Font::Display, 18, v.youReady ? INK : GOLD, Align::Centre);
            ui.text(90 + kw + 14, by + 8, v.youReady ? "Ready!" : "I'm ready", Font::Display, 30, v.youReady ? GOOD : GOLD_INK);
            ui.text(530, by + 21, v.dedicated ? "starts when everyone is ready" : (v.youReady ? "waiting for the host\xE2\x80\xA6" : "the host starts the run"), Font::Text600, 12,
                    v.youReady ? MUTED : hex(0x5A4718), Align::Right);
            ui.hit(72, by, 472, 58, LOBBY_READY);
        }

        // ---- The training ground, the spellbook, the options
        struct Item { std::string key, name; int action; };
        const Item items[] = {{"T", "Training", TITLE_TRAINING}, {"B", "Spellbook", TITLE_SPELLBOOK}, {"O", "Options", TITLE_OPTIONS}};
        for (int k = 0; k < 3; ++k)
        {
            const float x = 72.0f + static_cast<float>(k) * 160.0f;
            const bool over = ui.hover(x, 542, 152, 40);
            ui.box(x, 542, 152, 40, CARD, 2.0f, over ? GOLD : LINE);
            ui.keycap(x + 9, 550, 24, items[k].key, GOLD, GOLD_INK, 18, 0.0f);
            ui.text(x + 42, 549, items[k].name, Font::Display, 24, TEXT);
            ui.hit(x, 542, 152, 40, items[k].action);
        }

        // ---- How friends join
        ui.text(72, 598, v.isHost ? "FRIENDS JOIN WITH" : "JOINED", Font::Text700, 11, MUTED, Align::Left, 1.5f);
        const std::string port = ":" + std::to_string(v.port);

        if (v.isHost)
        {
            ui.text(72, 616, "LAN", Font::Text700, 11, FAINT, Align::Left, 1.0f);
            ui.text(130, 612, v.lanAddress.empty() ? "?" : v.lanAddress + port, Font::Display, 22, TEXT);

            ui.text(72, 642, "ONLINE", Font::Text700, 11, FAINT, Align::Left, 1.0f);
            const std::string forward = "forward TCP " + std::to_string(v.port) + " and UDP " + std::to_string(v.port + 1) + (v.lanAddress.empty() ? "" : " to " + v.lanAddress);

            switch (v.router)
            {
                case LobbyView::Router::Searching:
                    ui.text(130, 641, "looking for your router\xE2\x80\xA6", Font::Text600, 12, MUTED);
                    break;
                case LobbyView::Router::Open:
                {
                    const float aw = ui.text(130, 638, (v.externalAddress.empty() ? "your public address" : v.externalAddress) + ":" + std::to_string(v.externalPort ? v.externalPort : v.port), Font::Display, 22, TEXT);
                    ui.text(130 + aw + 10, 643, "ports opened on your router", Font::Text600, 11, GOOD);
                    break;
                }
                case LobbyView::Router::Closed:
                    ui.text(130, 641, ui.fit(forward + " on your router", Font::Text600, 12, 414), Font::Text600, 12, WARNING);
                    ui.text(130, 657, ui.fit(v.routerError, Font::Text500, 10, 414), Font::Text500, 10, FAINT);
                    break;
                case LobbyView::Router::Off:
                    ui.text(130, 641, ui.fit("UPnP off: " + forward, Font::Text600, 12, 414), Font::Text600, 12, MUTED);
                    break;
            }
        }
        else
        {
            ui.text(72, 612, v.joined + port, Font::Display, 22, TEXT);
        }

        ui.anchor(Anchor::Left);
        drawQuit(ui, 72, 676, v.quitArmed);

        // The latest of what happened
        float ly = 676.0f;
        for (auto it = v.log.rbegin(); it != v.log.rend() and ly < 704.0f; ++it, ly += 14.0f)
            ui.text(230, ly, ui.fit(*it, Font::Text500, 11, 314), Font::Text500, 11, FAINT);
    }

    void drawEnd(Ui& ui, const EndView& v)
    {
        ui.section("end");
        ui.anchor(Anchor::TopLeft);
        ui.rectPx(0, 0, ui.width(), ui.height(), v.won ? withAlpha(INK, 0.5f) : DEFEAT_WASH);

        const Colour accent = v.won ? GOLD : HEALTH_NUMBER;

        ui.anchor(Anchor::Centre);
        ui.raised(320, 150, 640, 420, PANEL, 4.0f);
        ui.rect(322, 152, 636, 6, accent);

        const std::string title = not v.won ? "DEFEATED" : (v.youChampion ? "CHAMPION" : "VICTORY");
        ui.textShadow(354, 176, title, Font::Display, 84, accent, 4.0f, Align::Left, 2.0f);

        std::string subtitle = v.won ? "All " + std::to_string(v.waves) + " waves, and " + v.boss + " falls."
                                     : "At wave " + std::to_string(v.wave) + " of " + std::to_string(v.waves) + ".";
        if (v.won and not v.champion.empty())
            subtitle = "All " + std::to_string(v.waves) + " waves, then " + (v.youChampion ? std::string("you won") : v.champion + " won") + " the final duel.";
        ui.text(354, 262, ui.fit(subtitle, Font::Text600, 16, 572), Font::Text600, 16, SUBTITLE);

        // The waves: reached, the one it ended on, the rest
        for (int k = 0; k < v.waves; ++k)
        {
            const float x = 354.0f + static_cast<float>(k) * (572.0f + 3.0f) / static_cast<float>(v.waves);
            const float w = std::floor(575.0f / static_cast<float>(v.waves)) - 3.0f;
            Colour c = withAlpha(INK, 0.6f);
            if (k < v.wave - 1 or (v.won and k < v.waves))
                c = TEXT;
            else if (k == v.wave - 1)
                c = HEALTH;
            ui.rect(std::round(x), 294, w, 10, c);
        }

        // The score, the best, the run in numbers
        ui.box(354, 322, 572, 96, WELL);
        ui.text(372, 336, "SCORE", Font::Text700, 11, MUTED, Align::Left, 1.5f);
        const float sw = ui.text(372, 350, grouped(v.score), Font::Display, 56, TEXT);

        if (v.score >= v.best and v.score > 0)
        {
            const float tw = ui.measure("BEST!", Font::Display, 18) + 12.0f;
            ui.rect(372 + sw + 12, 368, tw, 22, GOLD);
            ui.text(372 + sw + 18, 369, "BEST!", Font::Display, 18, GOLD_INK);
        }
        else
        {
            ui.text(372 + sw + 12, 372, "(best " + grouped(v.best) + ")", Font::Text600, 13, MUTED);
        }

        const int seconds = static_cast<int>(v.time);
        const std::string time = std::to_string(seconds / 60) + ":" + (seconds % 60 < 10 ? "0" : "") + std::to_string(seconds % 60);
        const std::pair<std::string, std::string> stats[] = {{"KILLS", std::to_string(v.kills)}, {"LEVEL", std::to_string(v.level)}, {"TIME", time}};

        for (int k = 0; k < 3; ++k)
        {
            const float x = 926.0f - 18.0f - static_cast<float>(3 - k) * 86.0f;
            ui.text(x, 340, stats[k].first, Font::Text700, 11, MUTED, Align::Left, 1.5f);
            ui.text(x, 358, stats[k].second, Font::Display, 34, TEXT);
        }

        // Again, or the training ground
        const float aw = 22.0f + ui.measure("ENTER", Font::Display, 16) + 12.0f + ui.measure("Play again", Font::Display, 28) + 24.0f;
        bool over = ui.hover(354, 492, aw, 48);
        ui.raised(354, 492, aw, 48, GOLD, 3.0f, 2.0f, over ? TEXT : INK);
        const float kw = ui.measure("ENTER", Font::Display, 16) + 12.0f;
        ui.box(366, 505, kw, 22, INK, 0.0f);
        ui.text(366 + kw * 0.5f, 507, "ENTER", Font::Display, 16, GOLD, Align::Centre);
        ui.text(366 + kw + 12, 500, "Play again", Font::Display, 28, GOLD_INK);
        ui.hit(354, 492, aw, 48, END_AGAIN);

        if (v.solo)
        {
            const float tx = 354.0f + aw + 12.0f;
            const float tw = 12.0f + 26.0f + 12.0f + ui.measure("Training ground", Font::Display, 28) + 16.0f;
            over = ui.hover(tx, 492, tw, 48);
            ui.box(tx, 492, tw, 48, CARD, 2.0f, over ? GOLD : LINE);
            ui.keycap(tx + 12, 503, 26, "T", GOLD, GOLD_INK, 20, 0.0f);
            ui.text(tx + 50, 500, "Training ground", Font::Display, 28, TEXT);
            ui.hit(tx, 492, tw, 48, END_TRAINING);
        }
    }
}
