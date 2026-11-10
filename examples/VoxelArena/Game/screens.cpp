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
        const bool alone = not v.hosting and not v.joined;
        const float x = 330, y = alone ? 125.0f : 150.0f, w = 620, h = alone ? 470.0f : 420.0f;
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

            choice(ry, "H", "Host a game", v.portEdit.empty() ? std::string("your friends join you, the game looks for a port on your router")
                                                              : "your friends join you, on port " + v.portEdit + " and the next one", INVITE_HOST);
            ry += 98.0f;
            choice(ry, "J", "Join a game", "a friend's, with the address their invite shows", INVITE_JOIN);
            ry += 98.0f;

            // The port: digits typed land here, there is no other field
            const bool caret = static_cast<int>(v.time * 2.0f) % 2 == 0;
            ui.text(x + 28, ry + 16, "PORT", Font::Text700, 11, MUTED, Align::Left, 1.5f);
            ui.box(x + 80, ry, 130, 44, WELL, 2.0f, GOLD);
            ui.text(x + 94, ry + 8, (v.portEdit.empty() ? std::string("auto") : v.portEdit) + (caret ? "_" : ""), Font::Display, 26, v.portEdit.empty() ? FAINT : TEXT);
            ui.paragraph(x + 224, ry + 2, w - 224 - 28, "Type the port your router lets in, it uses that one and the next. Empty: the game looks for one.",
                         Font::Text500, 12, TEXT_DIM, 17.0f);
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
                    if (v.manualPort)
                        row("ONLINE", (v.externalAddress.empty() ? std::string("your public address") : v.externalAddress) + port,
                            "works once your router forwards TCP " + std::to_string(v.port) + " and UDP " + std::to_string(v.port + 1) + " to " + v.lanAddress, WARNING,
                            v.externalAddress.empty() ? 0 : INVITE_COPY_ONLINE);
                    else
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

        ui.text(218, 76, "Controls", Font::Display, 44, TEXT);

        // The tabs: the keyboard's keys, the controller's buttons
        {
            float tx = 1062.0f;
            for (const bool pad : {true, false})
            {
                const auto s = ui.scope(pad ? 2 : 1);
                const char* label = pad ? "Controller" : "Keyboard";
                const float w = ui.measure(label, Font::Text700, 13) + 28.0f;
                tx -= w;
                const bool on = v.pad == pad, over = ui.hover(tx, 80, w, 32);
                ui.box(tx, 80, w, 32, on ? hex(0x2A2414) : CARD, 2.0f, on ? GOLD : (over ? TEXT_DIM : LINE));
                ui.text(tx + w * 0.5f, 88, label, Font::Text700, 13, on ? GOLD : TEXT_DIM, Align::Centre);
                ui.hit(tx, 80, w, 32, pad ? CONTROLS_TAB_PAD : CONTROLS_TAB_KEYS);
                tx -= 8.0f;
            }
        }
        ui.text(218, 122, v.pad ? "pick an action, then press the new button on the controller; Start cancels"
                                : "click a key, then press the new one (a key or a mouse button); Esc cancels",
                Font::Text500, 12, MUTED);

        for (size_t i = 0; i < v.rows.size(); ++i)
        {
            const auto itemScope = ui.scope(i);
            const auto& r = v.rows[i];
            const bool right = static_cast<int>(i) >= v.split;
            const size_t k = right ? i - static_cast<size_t>(v.split) : i;
            const float x = right ? 652.0f : 218.0f, y = 148.0f + static_cast<float>(k) * 48.0f;

            ui.text(x, y + 10, r.label, Font::Text600, 15, r.waiting ? GOLD : TEXT_DIM);

            const float bx = x + 270.0f, bw = 130.0f;
            const bool over = ui.hover(bx, y, bw, 36);
            ui.box(bx, y, bw, 36, r.waiting ? hex(0x2A2414) : WELL, 2.0f, r.waiting ? GOLD : (over ? TEXT_DIM : LINE));
            ui.text(bx + bw * 0.5f, y + 6, r.waiting ? (v.pad ? "press a button" : "press a key") : ui.fit(r.key, Font::Display, 22, bw - 16), Font::Display, 22,
                    r.waiting ? GOLD : (v.pad and not v.padEnabled ? FAINT : TEXT), Align::Centre);
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

        // The controller on or off (its tab)
        float messageX = 470.0f;
        if (v.pad)
        {
            const float x = 218.0f + ui.measure("Reset to defaults", Font::Display, 22) + 28.0f + 12.0f;
            const std::string label = v.padEnabled ? "Controller: on" : "Controller: off";
            const float w = ui.measure(label, Font::Display, 22) + 28.0f;
            const bool over = ui.hover(x, 600, w, 42);
            ui.box(x, 600, w, 42, v.padEnabled ? hex(0x1E2A1E) : CARD, 2.0f, over ? GOLD : (v.padEnabled ? GOOD : LINE));
            ui.text(x + w * 0.5f, 608, label, Font::Display, 22, v.padEnabled ? GOOD : TEXT_DIM, Align::Centre);
            ui.hit(x, 600, w, 42, CONTROLS_PAD_TOGGLE);
            messageX = x + w + 16.0f;
        }

        if (not v.message.empty())
            ui.text(messageX, 613, ui.fit(v.message, Font::Text600, 12, 900.0f - messageX), Font::Text600, 12, GOOD);

        const bool over = ui.hover(912, 600, 150, 42);
        ui.raised(912, 600, 150, 42, GOLD, 3.0f, 2.0f, over ? TEXT : INK);
        const float kw = ui.measure("ESC", Font::Display, 15) + 10.0f;
        ui.box(924, 610, kw, 22, INK, 0.0f);
        ui.text(924 + kw * 0.5f, 612, "ESC", Font::Display, 15, GOLD, Align::Centre);
        ui.text(924 + kw + 10, 607, "Back", Font::Display, 24, GOLD_INK);
        ui.hit(912, 600, 150, 42, CONTROLS_BACK);
    }

    namespace
    {
        void drawFirstSpell(Ui& ui, const ArmoryView& v)
        {
            ui.text(66, 84, "CHOOSE YOUR FIRST SPELL", Font::Display, 40, TEXT, Align::Left, 1.0f);
            ui.text(66, 128, "This spell stays on key 1 for the whole run. Level ups make it stronger.", Font::Text500, 13, TEXT_DIM);

            // The weapon taken, to change it
            {
                const std::string label = "Weapon: " + v.weapon + "  \xC2\xB7  change";
                const float w = ui.measure(label, Font::Text700, 13) + 28.0f;
                const float x = 1214.0f - w, y = 124.0f;
                const bool over = ui.hover(x, y, w, 30);
                ui.box(x, y, w, 30, CARD, 2.0f, over ? GOLD : LINE);
                ui.text(x + 14, y + 7, label, Font::Text700, 13, over ? GOLD : TEXT_DIM);
                ui.hit(x, y, w, 30, ARMORY_BACK);
            }

            if (v.players > 1)
                ui.text(1214, 84, std::to_string(v.ready) + " / " + std::to_string(v.players) + " ready", Font::Display, 26, v.picked ? GOOD : TEXT_DIM, Align::Right);

            const size_t n = std::max<size_t>(1, v.spells.size());
            const float gap = 24.0f, cw = 360.0f;
            const float startX = 640.0f - (static_cast<float>(n) * cw + static_cast<float>(n - 1) * gap) * 0.5f;

            for (size_t i = 0; i < v.spells.size(); ++i)
            {
                const auto itemScope = ui.scope(i);
                const auto& sp = v.spells[i];
                const float x = std::round(startX + static_cast<float>(i) * (cw + gap)), y = 170.0f, h = 450.0f;
                const bool over = ui.hover(x, y, cw, h);
                const Colour colour = elementColour(sp.element);

                ui.raised(x, y, cw, h, CARD, 3.0f, 2.0f, sp.chosen ? GOLD : (over ? TEXT_DIM : colour));

                float cy = y + 14.0f;
                ui.keycap(x + 14, cy, 22, std::to_string(i + 1), GOLD, GOLD_INK, 18, 0.0f);
                ui.rect(x + 46, cy + 7, 8, 8, colour);
                ui.text(x + 60, cy + 4, sp.element, Font::Text700, 11, colour, Align::Left, 1.5f);
                cy += 36.0f;

                ui.box(x + 14, cy, 72, 72, WELL, 2.0f, colour);
                ui.icon(sp.element, x + 25, cy + 11, 7.0f, colour);
                cy += 86.0f;

                ui.text(x + 14, cy, sp.name, Font::Display, 34, TEXT);
                cy += 46.0f;
                ui.paragraph(x + 14, cy, cw - 28, sp.text, Font::Text500, 14, TEXT_DIM, 19.6f, 8);

                const float by = y + h - 14 - 44;
                if (sp.chosen)
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

                ui.hit(x, y, cw, h, ARMORY_SPELL + static_cast<int>(i));
            }

            ui.text(66, 640, v.picked and v.players > 1 ? "Waiting for the other players\xE2\x80\xA6 You can still change your choice."
                                                      : "Press 1-" + std::to_string(n) + " or click to choose. Backspace: back to weapons.", Font::Text500, 12, FAINT);
        }
    }

    void drawDifficulty(Ui& ui, const DifficultyView& v)
    {
        ui.section("difficulty");
        ui.anchor(Anchor::TopLeft);
        ui.rectPx(0, 0, ui.width(), ui.height(), withAlpha(INK, 0.58f));

        ui.anchor(Anchor::Centre);
        ui.raised(40, 60, 1200, 600, PANEL, 4.0f);

        ui.text(66, 84, "CHOOSE THE DIFFICULTY", Font::Display, 40, TEXT, Align::Left, 1.0f);
        ui.text(66, 128, v.choosing ? "Win a difficulty to open the next one." : v.chooserName + " is choosing the difficulty.", Font::Text500, 13, v.choosing ? TEXT_DIM : GOLD);

        // Easy to hard: green, gold, red, purple
        const Colour ramp[] = {GOOD, GOLD, FIRE_TEXT, HEALTH_NUMBER, BOSS};
        const size_t n = std::max<size_t>(1, v.levels.size());
        auto accentOf = [&](size_t i) {
            const float t = n > 1 ? static_cast<float>(i) / static_cast<float>(n - 1) * 4.0f : 0.0f;
            const int a = std::min(3, static_cast<int>(t));
            const float f = t - static_cast<float>(a);
            return glm::mix(ramp[a], ramp[a + 1], f);
        };

        const float gap = 12.0f;
        const float cw = std::floor((1148.0f - gap * static_cast<float>(n - 1)) / static_cast<float>(n));

        for (size_t i = 0; i < v.levels.size(); ++i)
        {
            const auto itemScope = ui.scope(i);
            const auto& l = v.levels[i];
            const bool open = l.locked.empty();
            const float x = 66.0f + static_cast<float>(i) * (cw + gap), y = 160.0f, h = 400.0f;
            const bool over = v.choosing and open and ui.hover(x, y, cw, h);
            const bool lit = over or (v.choosing and open and static_cast<int>(i) == v.cursor);
            const Colour accent = open ? accentOf(i) : FAINT;

            ui.raised(x, y, cw, h, open ? CARD : CARD_DIM, 3.0f, 2.0f, lit ? GOLD : INK);
            ui.rect(x + 2, y + 2, cw - 4, 6, accent);

            float cy = y + 22.0f;
            if (v.choosing and open)
                ui.keycap(x + 14, cy, 22, std::to_string(i + 1), GOLD, GOLD_INK, 18, 0.0f);
            else if (not open)
                ui.icon("lock", x + 16, cy + 4, 2.0f, FAINT);
            cy += 34.0f;

            ui.text(x + 14, cy, ui.fit(l.name, Font::Display, 32, cw - 28), Font::Display, 32, open ? accent : FAINT);
            cy += 44.0f;
            cy += ui.paragraph(x + 14, cy, cw - 28, l.waves, Font::Text600, 13, open ? TEXT : FAINT, 18.0f, 2) + 12.0f;

            for (const auto& line : l.lines)
                cy += ui.paragraph(x + 14, cy, cw - 28, line, Font::Text500, 12, open ? TEXT_DIM : FAINT, 16.0f, 2) + 6.0f;

            // Play it, or how to open it
            const float by = y + h - 14 - 44;
            if (not open)
            {
                ui.box(x + 14, by, cw - 28, 44, WELL, 2.0f, LINE);
                ui.text(x + cw * 0.5f, by + 14, ui.fit(l.locked, Font::Text600, 12, cw - 40), Font::Text600, 12, MUTED, Align::Centre);
            }
            else if (v.choosing)
            {
                ui.raised(x + 14, by, cw - 28, 44, GOLD, 3.0f, 2.0f, lit ? TEXT : INK);
                ui.text(x + cw * 0.5f, by + 9, "Play", Font::Display, 26, GOLD_INK, Align::Centre);
                ui.hit(x, y, cw, h, DIFFICULTY_PICK + static_cast<int>(i));
            }
        }

        // Back to the title (the lobby)
        if (v.choosing and v.canBack)
        {
            const float kw = ui.measure("BKSP", Font::Display, 15) + 12.0f;
            const float bw = 12.0f + kw + 10.0f + ui.measure("Back", Font::Display, 24) + 16.0f;
            const bool over = ui.hover(66, 590, bw, 44);
            ui.box(66, 590, bw, 44, CARD, 2.0f, over ? GOLD : LINE);
            ui.box(78, 601, kw, 22, INK, 0.0f);
            ui.text(78 + kw * 0.5f, 604, "BKSP", Font::Display, 15, GOLD, Align::Centre);
            ui.text(78 + kw + 10, 598, "Back", Font::Display, 24, TEXT);
            ui.hit(66, 590, bw, 44, DIFFICULTY_BACK);
        }
    }

    int drawArmory(Ui& ui, const ArmoryView& v)
    {
        int hovered = -1;
        ui.section("armory");
        ui.anchor(Anchor::TopLeft);
        ui.rectPx(0, 0, ui.width(), ui.height(), withAlpha(INK, 0.58f));

        ui.anchor(Anchor::Centre);
        ui.raised(40, 60, 1200, 600, PANEL, 4.0f);

        if (v.choosingSpell)
        {
            drawFirstSpell(ui, v);
            return -1;
        }

        ui.text(66, 84, "CHOOSE YOUR WEAPON", Font::Display, 40, TEXT, Align::Left, 1.0f);
        ui.text(66, 128, "You keep this weapon for the whole run. Level ups unlock its techniques.", Font::Text500, 13, TEXT_DIM);

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
            if (over)
                hovered = static_cast<int>(i);

            Colour role = KIND_WEAPON;
            if (w.role == "ranged") role = KIND_STAT;
            if (w.role == "support") role = GOOD;

            const bool locked = not w.locked.empty();
            ui.raised(x, y, cw, h, locked ? CARD_DIM : CARD, 3.0f, 2.0f, w.chosen ? GOLD : (over and not locked ? TEXT_DIM : INK));

            float cy = y + 14.0f;
            ui.keycap(x + 14, cy, 22, std::to_string(i + 1), GOLD, GOLD_INK, 18, 0.0f);
            ui.rect(x + 46, cy + 7, 8, 8, role);
            ui.text(x + 60, cy + 4, w.role.empty() ? "WEAPON" : w.role, Font::Text700, 11, role, Align::Left, 1.5f);
            cy += 36.0f;

            ui.box(x + 14, cy, 60, 60, WELL, 2.0f, role);
            ui.icon(w.icon, x + 23, cy + 9, 6.0f, role);
            cy += 72.0f;

            // The name: smaller on narrow cards (many weapons) before it is cut short
            float nameSize = 30.0f;
            while (nameSize > 20.0f and ui.measure(w.name, Font::Display, nameSize) > cw - 28)
                nameSize -= 1.0f;
            ui.text(x + 14, cy + (30.0f - nameSize) * 0.5f, ui.fit(w.name, Font::Display, nameSize, cw - 28), Font::Display, nameSize, TEXT);
            cy += 40.0f;

            cy += ui.paragraph(x + 14, cy, cw - 28, w.text, Font::Text500, 13, TEXT_DIM, 18.0f, 2) + 6.0f;
            if (w.regen > 0.0f)
            {
                ui.text(x + 14, cy, "+" + std::to_string(w.regen).substr(0, 3) + " health per second", Font::Text600, 12, GOOD);
                cy += 20.0f;
            }
            cy += 8.0f;

            const std::string techniques = "TECHNIQUES (FROM LEVEL UPS)";
            ui.text(x + 14, cy, ui.measure(techniques, Font::Text700, 10, 1.5f) <= cw - 28 ? techniques : "TECHNIQUES", Font::Text700, 10, MUTED, Align::Left, 1.5f);
            cy += 16.0f;
            std::string skills;
            for (const auto& name : w.skills)
                skills += (skills.empty() ? "" : ", ") + name;
            ui.paragraph(x + 14, cy, cw - 28, skills.empty() ? "None" : skills, Font::Text600, 12, skills.empty() ? FAINT : TEXT, 16.0f, 2);

            // Take it (or taken, or locked: what unlocks it)
            const float by = y + h - 14 - 44;
            if (locked)
            {
                ui.rect(x + 3, y + 3, cw - 6, h - 6, withAlpha(INK, 0.45f));
                ui.box(x + 14, by - 22, cw - 28, 66, WELL, 2.0f, LINE);
                ui.text(x + 26, by - 14, "LOCKED", Font::Text700, 11, MUTED, Align::Left, 1.5f);
                ui.paragraph(x + 26, by + 2, cw - 52, w.locked, Font::Text600, 12, TEXT_DIM, 16.0f, 2);
                continue;
            }
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

        ui.text(66, 640, v.picked and v.players > 1 ? "Waiting for the other players\xE2\x80\xA6 You can still change your choice."
                                                  : "Press 1-" + std::to_string(n) + " or click to choose. Hover a weapon to see its attacks.", Font::Text500, 12, FAINT);

        // The hovered weapon's attacks, on top of the cards (after a moment, fading in)
        if (v.tipCard >= 0 and v.tipCard < static_cast<int>(v.weapons.size()) and v.tipAlpha > 0.0f)
        {
            const auto& w = v.weapons[static_cast<size_t>(v.tipCard)];
            auto faded = [&](const Colour& c) { return withAlpha(c, v.tipAlpha * c.a / 255.0f); };
            const auto tipScope = ui.scope(900);

            const float tw = 280.0f;
            const float x = std::clamp(66.0f + static_cast<float>(v.tipCard) * (cw + gap) + cw * 0.5f - tw * 0.5f, 50.0f, 1230.0f - tw);
            const float y = 420.0f;   // under the cards' techniques
            const auto light = ui.wrap(w.light, Font::Text500, 13, tw - 28.0f);
            const auto heavy = ui.wrap(w.heavy, Font::Text500, 13, tw - 28.0f);
            const float th = 14.0f + 16.0f + 18.0f * static_cast<float>(light.size()) + 10.0f + 16.0f + 18.0f * static_cast<float>(heavy.size()) + 14.0f;

            ui.rect(x + 3, y + 3, tw, th, faded(INK));
            ui.box(x, y, tw, th, faded(PANEL), 2.0f, faded(GOLD));
            float ty = y + 14.0f;
            ui.text(x + 14, ty, "LIGHT ATTACK", Font::Text700, 10, faded(GOLD), Align::Left, 1.5f);
            ty += 16.0f;
            ty += ui.paragraph(x + 14, ty, tw - 28.0f, w.light, Font::Text500, 13, faded(TEXT), 18.0f) + 10.0f;
            ui.text(x + 14, ty, "HEAVY ATTACK", Font::Text700, 10, faded(GOLD), Align::Left, 1.5f);
            ty += 16.0f;
            ui.paragraph(x + 14, ty, tw - 28.0f, w.heavy, Font::Text500, 13, faded(TEXT), 18.0f);
        }

        return hovered;
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
            ui.text(90, 368, v.arenaLine, Font::Text500, 13, ARENA_LINE);
            ui.hit(72, 318, 472, 78, TITLE_ARENA);
        }

        struct Item { std::string key, name, hint; int action; };
        const Item items[] = {
            {"T", "Training ground", "dummies, enemies that come back", TITLE_TRAINING},
            {"M", "Multiplayer", v.canHost ? "host a game, or join a friend's" : "join a friend's game", TITLE_MULTIPLAYER},
            {"A", "Achievements", std::to_string(v.goalsDone) + " of " + std::to_string(v.goals) + " reached", TITLE_ACHIEVEMENTS},
            {"O", "Options", "mouse, view, sound", TITLE_OPTIONS},
        };

        for (int k = 0; k < 4; ++k)
        {
            const auto itemScope = ui.scope(static_cast<uint64_t>(k));
            const float y = 404.0f + static_cast<float>(k) * 50.0f;
            const bool over = ui.hover(72, y, 472, 44);
            ui.box(72, y, 472, 44, CARD, 2.0f, over ? GOLD : LINE);
            ui.keycap(84, y + 9, 26, items[k].key, GOLD, GOLD_INK, 20, 0.0f);
            ui.text(124, y + 7, items[k].name, Font::Display, 28, TEXT);
            ui.text(530, y + 15, items[k].hint, Font::Text500, 12, MUTED, Align::Right);
            ui.hit(72, y, 472, 44, items[k].action);
        }

        ui.text(72, 616, "BEST SCORE", Font::Text700, 11, MUTED, Align::Left, 1.5f);
        const float bw = ui.text(72, 630, grouped(v.best), Font::Display, 40, TEXT);
        ui.text(72 + bw + 12, 648, v.best > 0 ? "your best run so far" : "no run yet", Font::Text500, 12, MUTED);

        drawQuit(ui, 72, 672, v.quitArmed);
        ui.text(248, 682, "F1 overlay \xC2\xB7 ` console \xC2\xB7 Esc options", Font::Text500, 11, FAINT);
    }

    namespace
    {
        /** A disc in pixel steps (the UI draws rectangles): a row of 2 px each. */
        void pixelDisc(Ui& ui, float cx, float cy, float r, const Colour& c)
        {
            for (float dy = -r; dy < r; dy += 2.0f)
            {
                const float mid = dy + 1.0f;
                const float half = std::floor(std::sqrt(std::max(0.0f, r * r - mid * mid)) / 2.0f) * 2.0f;
                if (half > 0.0f)
                    ui.rect(cx - half, cy + dy, half * 2.0f, 2.0f, c);
            }
        }
    }

    void drawMacaron(Ui& ui, const MacaronView& v)
    {
        const auto scope = ui.scope(Ui::idOf("macaron"));
        ui.anchor(Anchor::TopRight);

        // In: the badge pops (a little past its size, then back), the panel slides down; out: everything fades
        const float t = v.age;
        const float in = std::clamp(t / 0.25f, 0.0f, 1.0f);
        const float out = std::clamp((MacaronView::LIFE - t) / 0.5f, 0.0f, 1.0f);
        const float alpha = in * out;
        if (alpha <= 0.0f)
            return;
        const float pop = t < 0.18f ? 1.25f * (t / 0.18f) : (t < 0.32f ? 1.25f - 0.25f * ((t - 0.18f) / 0.14f) : 1.0f);
        auto faded = [&](const Colour& c) { return withAlpha(c, alpha * c.a / 255.0f); };

        const float w = 380.0f, h = 66.0f;
        const float x = 1256.0f - w, y = 86.0f - 16.0f * (1.0f - in);
        const float cx = x + 4.0f, cy = y + h * 0.5f;

        // The panel, the badge's tail under it
        ui.rect(x + 3, y + 3, w, h, faded(INK));
        ui.box(x, y, w, h, faded(PANEL), 2.0f, faded(GOLD));
        const float tx = x + 52.0f, tw = w - 64.0f;
        ui.text(tx, y + 9, "GOAL REACHED", Font::Text700, 11, faded(GOLD), Align::Left, 1.5f);
        ui.text(tx, y + 23, ui.fit(v.name, Font::Display, 24, tw), Font::Display, 24, faded(TEXT));
        if (not v.unlocks.empty())
            ui.text(tx, y + 47, ui.fit("New: " + v.unlocks, Font::Text600, 12, tw), Font::Text600, 12, faded(GOOD));

        // The ribbons hanging from the badge
        for (int k = 0; k < 2; ++k)
        {
            const float rx = cx - 13.0f + static_cast<float>(k) * 14.0f;
            ui.rect(rx - 1, cy + 14, 12, 26, faded(INK));
            ui.rect(rx, cy + 14, 10, 24, faded(HEALTH));
            ui.rect(rx + 3, cy + 32, 4, 6, faded(INK));
        }

        // The macaron: an ink edge, a gold ring with notches, a darker heart and the goal's icon
        const float r = 30.0f * pop;
        pixelDisc(ui, cx, cy, r + 3.0f, faded(INK));
        pixelDisc(ui, cx, cy, r, faded(GOLD));
        for (int k = 0; k < 12; ++k)
        {
            const float a = static_cast<float>(k) * 3.14159265f / 6.0f;
            ui.rect(std::round(cx + std::cos(a) * (r - 1.0f)) - 2.0f, std::round(cy + std::sin(a) * (r - 1.0f)) - 2.0f, 4, 4, faded(hex(0xC9A24A)));
        }
        pixelDisc(ui, cx, cy, r - 7.0f, faded(GOLD_INK));
        if (not v.icon.empty())
            ui.icon(v.icon, cx - 13.0f * pop, cy - 13.0f * pop, 3.7f * pop, faded(GOLD));

        // Sparks flying off it as it comes
        if (t < 0.7f)
        {
            const float s = t / 0.7f;
            for (int k = 0; k < 10; ++k)
            {
                const float a = static_cast<float>(k) * 0.628f + 0.3f;
                const float d = r + 6.0f + 34.0f * s;
                ui.rect(std::round(cx + std::cos(a) * d) - 2.0f, std::round(cy + std::sin(a) * d) - 2.0f, 4, 4, withAlpha(GOLD, (1.0f - s) * out));
            }
        }
    }

    void drawAchievements(Ui& ui, const AchievementsView& v)
    {
        ui.section("achievements");
        ui.anchor(Anchor::TopLeft);
        ui.rectPx(0, 0, ui.width(), ui.height(), withAlpha(INK, 0.6f));
        ui.hit(0, 0, ui.baseX(ui.width()), ui.baseY(ui.height()), -1);

        ui.anchor(Anchor::Centre);
        ui.raised(40, 40, 1200, 640, PANEL, 4.0f);

        const float tw = ui.text(66, 62, "ACHIEVEMENTS", Font::Display, 40, TEXT, Align::Left, 1.0f);
        ui.text(66 + tw + 18, 78, std::to_string(v.done) + " of " + std::to_string(v.goals.size()) + " reached \xC2\xB7 " + std::to_string(v.unlockedPercent) + "% of the game unlocked",
                Font::Text600, 14, TEXT_DIM);
        ui.text(66, 108, "Goals count over all your runs. Each one unlocks new weapons, spells, techniques or items for the next runs.", Font::Text500, 13, MUTED);

        // Close (Esc)
        {
            const float cw = 22.0f + 8.0f + ui.measure("Close", Font::Text600, 13) + 18.0f;
            const float cx = 1214 - cw, cy = 66;
            const bool over = ui.hover(cx, cy, cw, 30);
            ui.box(cx, cy, cw, 30, CARD, 2.0f, over ? GOLD : LINE);
            const float kw = ui.measure("ESC", Font::Display, 13) + 8.0f;
            ui.box(cx + 6, cy + 5, kw, 20, TEXT_DIM, 0.0f);
            ui.text(cx + 6 + kw * 0.5f, cy + 6, "ESC", Font::Display, 13, INK, Align::Centre);
            ui.text(cx + 6 + kw + 8, cy + 7, "Close", Font::Text600, 13, TEXT);
            ui.hit(cx, cy, cw, 30, ACHIEVEMENTS_CLOSE);
        }

        // The goals: three columns, a page at a time
        const int pages = std::max(1, (static_cast<int>(v.goals.size()) + ACHIEVEMENTS_PER_PAGE - 1) / ACHIEVEMENTS_PER_PAGE);
        const int page = std::clamp(v.page, 0, pages - 1);
        const float cw = 372.0f, ch = 92.0f, gap = 16.0f;

        for (int k = 0; k < ACHIEVEMENTS_PER_PAGE; ++k)
        {
            const size_t i = static_cast<size_t>(page * ACHIEVEMENTS_PER_PAGE + k);
            if (i >= v.goals.size())
                break;

            const auto goalScope = ui.scope(i);
            const auto& g = v.goals[i];
            const float x = 66.0f + static_cast<float>(k % 3) * (cw + gap);
            const float y = 140.0f + static_cast<float>(k / 3) * (ch + 10.0f);

            ui.box(x, y, cw, ch, g.done ? CARD : CARD_DIM, 2.0f, g.done ? GOLD : LINE);

            // Its icon: the first thing it unlocks (in colour once reached)
            ui.box(x + 10, y + 10, 48, 48, WELL, 2.0f, g.done ? GOLD : LINE);
            if (not g.icon.empty())
                ui.icon(g.icon, x + 16, y + 16, 4.5f, g.done ? GOLD : FAINT);
            if (g.done)
                ui.icon("check", x + 44, y + 50, 1.5f, GOOD);

            const float tx = x + 70, tw2 = cw - 80;
            ui.text(tx, y + 8, ui.fit(g.name, Font::Display, 22, tw2), Font::Display, 22, g.done ? GOLD : TEXT);
            ui.text(tx, y + 34, ui.fit(g.text, Font::Text500, 13, tw2), Font::Text500, 13, TEXT_DIM);
            ui.text(tx, y + 52, ui.fit("Unlocks: " + g.unlocks, Font::Text600, 12, tw2), Font::Text600, 12, g.done ? GOOD : MUTED);

            // How far: a bar, and the numbers
            const float ratio = std::clamp(g.have / std::max(0.001f, g.goal), 0.0f, 1.0f);
            const std::string count = g.done ? "Done" : grouped(static_cast<int>(g.have)) + " / " + grouped(static_cast<int>(g.goal));
            const float nw = ui.text(x + cw - 10, y + 70, count, Font::Text600, 11, g.done ? GOOD : MUTED, Align::Right);
            const float bw = cw - 80 - nw - 10;
            ui.rect(tx, y + 75, bw, 6, withAlpha(INK, 0.7f));
            ui.rect(tx, y + 75, std::round(bw * ratio), 6, g.done ? GOOD : GOLD);
        }

        // The pages
        if (pages > 1)
        {
            const float y = 652;
            auto arrow = [&](float x, const std::string& label, int action, bool on) {
                const auto arrowScope = ui.scope(static_cast<uint64_t>(action));
                const float w = ui.measure(label, Font::Text600, 13) + 28.0f;
                const bool over = on and ui.hover(x, y - 6, w, 30);
                ui.box(x, y - 6, w, 30, CARD, 2.0f, over ? GOLD : LINE);
                ui.text(x + w * 0.5f, y + 1, label, Font::Text600, 13, on ? TEXT : FAINT, Align::Centre);
                if (on)
                    ui.hit(x, y - 6, w, 30, action);
                return w;
            };
            const std::string where = "Page " + std::to_string(page + 1) + " of " + std::to_string(pages);
            ui.text(640, y, where, Font::Text600, 14, TEXT_DIM, Align::Centre);
            arrow(560 - 110, "< Previous", ACHIEVEMENTS_PREV, page > 0);
            arrow(720, "Next >", ACHIEVEMENTS_NEXT, page + 1 < pages);
        }
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

        // ---- The training ground, the options
        struct Item { std::string key, name; int action; };
        const Item items[] = {{"T", "Training", TITLE_TRAINING}, {"O", "Options", TITLE_OPTIONS}};
        for (int k = 0; k < 2; ++k)
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

            // A port the player chose may be forwarded by hand: the address stands, whatever UPnP said
            if (v.manualPort and (v.router == LobbyView::Router::Closed or v.router == LobbyView::Router::Off))
            {
                const float aw = ui.text(130, 638, (v.externalAddress.empty() ? std::string("your public address") : v.externalAddress) + port, Font::Display, 22, TEXT);
                ui.text(130 + aw + 10, 643, ui.fit("forward TCP " + std::to_string(v.port) + ", UDP " + std::to_string(v.port + 1), Font::Text600, 11, 414 - aw - 10), Font::Text600, 11, WARNING);
            }
            else switch (v.router)
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

        // Taller with the duel's winner and the players' recap, wider with the recap
        const bool recap = not v.recap.empty();
        const float duelH = v.champion.empty() ? 0.0f : 58.0f;
        const float recapH = recap ? 4.0f + 22.0f * static_cast<float>(v.recap.size()) : 0.0f;
        const size_t goalRows = std::min<size_t>(v.goals.size(), 4);
        const float goalsH = v.goals.empty() ? 0.0f : 30.0f + 40.0f * static_cast<float>(goalRows) + (goalRows < v.goals.size() ? 20.0f : 0.0f);
        const float panelW = recap ? 800.0f : 640.0f, panelH = 420.0f + duelH + recapH + goalsH;
        // With the damage by source: a panel of its own on the right, the two centred together
        const bool sources = not v.sources.empty();
        const float sideW = 300.0f, sideGap = 12.0f;
        const float px = 640.0f - (panelW + (sources ? sideGap + sideW : 0.0f)) * 0.5f, py = std::max(8.0f, 360.0f - panelH * 0.5f);
        const float L = px + 34.0f, R = px + panelW - 34.0f, W = R - L;

        ui.anchor(Anchor::Centre);
        ui.raised(px, py, panelW, panelH, PANEL, 4.0f);
        ui.rect(px + 2, py + 2, panelW - 4, 6, accent);

        const std::string title = not v.won ? "DEFEATED" : (v.youChampion ? "CHAMPION" : "VICTORY");
        ui.textShadow(L, py + 26, title, Font::Display, 84, accent, 4.0f, Align::Left, 2.0f);

        std::string subtitle = v.won ? "All " + std::to_string(v.waves) + " waves, and " + v.boss + " falls."
                                     : "At wave " + std::to_string(v.wave) + " of " + std::to_string(v.waves) + ".";
        if (v.won and not v.champion.empty())
            subtitle = "All " + std::to_string(v.waves) + " waves, then the final duel.";
        if (not v.difficulty.empty())
            subtitle = v.difficulty + " \xC2\xB7 " + subtitle;

        // A win that opened the next level
        if (not v.opened.empty())
        {
            ui.text(R, py + 42, "NOW OPEN", Font::Text700, 11, MUTED, Align::Right, 1.5f);
            ui.text(R, py + 58, ui.fit(v.opened, Font::Display, 30, 220), Font::Display, 30, GOLD, Align::Right);
        }
        ui.text(L, py + 112, ui.fit(subtitle, Font::Text600, 16, W), Font::Text600, 16, SUBTITLE);

        // Who won the final duel
        float y = py + 144;
        if (duelH > 0.0f)
        {
            ui.box(L, y, W, 46, hex(0x2A2414), 2.0f, GOLD);
            ui.text(L + 16, y + 16, "DUEL WINNER", Font::Text700, 12, GOLD, Align::Left, 1.5f);
            const std::string who = v.youChampion ? v.champion + " (you)" : v.champion;
            ui.text(L + 150, y + 6, ui.fit(who, Font::Display, 32, W - 170), Font::Display, 32, TEXT);
            y += duelH;
        }

        // The waves: reached, the one it ended on, the rest
        for (int k = 0; k < v.waves; ++k)
        {
            const float x = L + static_cast<float>(k) * (W + 3.0f) / static_cast<float>(v.waves);
            const float w = std::floor((W + 3.0f) / static_cast<float>(v.waves)) - 3.0f;
            Colour c = withAlpha(INK, 0.6f);
            if (k < v.wave - 1 or (v.won and k < v.waves))
                c = TEXT;
            else if (k == v.wave - 1)
                c = HEALTH;
            ui.rect(std::round(x), y, w, 10, c);
        }

        // The score, the best, the run in numbers
        y += 28;
        ui.box(L, y, W, 96, WELL);
        ui.text(L + 18, y + 14, "SCORE", Font::Text700, 11, MUTED, Align::Left, 1.5f);
        const float sw = ui.text(L + 18, y + 28, grouped(v.score), Font::Display, 56, TEXT);

        if (v.score >= v.best and v.score > 0)
        {
            const float tw = ui.measure("BEST!", Font::Display, 18) + 12.0f;
            ui.rect(L + 18 + sw + 12, y + 46, tw, 22, GOLD);
            ui.text(L + 18 + sw + 18, y + 47, "BEST!", Font::Display, 18, GOLD_INK);
        }
        else
        {
            ui.text(L + 18 + sw + 12, y + 50, "(best " + grouped(v.best) + ")", Font::Text600, 13, MUTED);
        }

        const int seconds = static_cast<int>(v.time);
        const std::string time = std::to_string(seconds / 60) + ":" + (seconds % 60 < 10 ? "0" : "") + std::to_string(seconds % 60);
        const std::pair<std::string, std::string> stats[] = {{"KILLS", std::to_string(v.kills)}, {"LEVEL", std::to_string(v.level)}, {"TIME", time}};

        for (int k = 0; k < 3; ++k)
        {
            const float x = R - 18.0f - static_cast<float>(3 - k) * 86.0f;
            ui.text(x, y + 18, stats[k].first, Font::Text700, 11, MUTED, Align::Left, 1.5f);
            ui.text(x, y + 36, stats[k].second, Font::Display, 34, TEXT);
        }
        y += 110;

        // What each player did: a row each, the columns right-aligned
        if (recap)
        {
            const bool healed = std::any_of(v.recap.begin(), v.recap.end(), [](const EndView::Recap& r) { return r.healing > 0; });
            std::vector<std::string> heads = {"LEVEL", "KILLS", "DAMAGE"};
            if (healed)
                heads.push_back("HEALING");
            for (const char* h : {"SHARDS", "XP", "PRESENTS", "ITEMS", "FALLS"})
                heads.push_back(h);

            // Each column as wide as its heading needs; the name takes the rest
            std::vector<float> right;
            float used = 0.0f;
            for (const auto& h : heads)
                right.push_back(used += std::max(58.0f, ui.measure(h, Font::Text700, 11, 1.0f) + 24.0f));
            const float nameW = W - used;

            ui.text(L + 10, y, "PLAYER", Font::Text700, 11, MUTED, Align::Left, 1.5f);
            for (size_t c = 0; c < heads.size(); ++c)
                ui.text(L + nameW + right[c] - 10.0f, y, heads[c], Font::Text700, 11, MUTED, Align::Right, 1.0f);
            y += 20;

            for (size_t i = 0; i < v.recap.size(); ++i)
            {
                const auto rowScope = ui.scope(i);
                const EndView::Recap& r = v.recap[i];
                const float ry = y + 22.0f * static_cast<float>(i);
                if (r.you)
                    ui.rect(L, ry, W, 22, withAlpha(TEXT, 0.07f));

                const Colour ink = r.champion ? GOLD : (r.you ? TEXT : TEXT_DIM);
                const std::string name = r.name + (r.you ? " (you)" : "");
                ui.text(L + 10, ry + 3, ui.fit(name, Font::Text700, 14, nameW - 16), Font::Text700, 14, ink);

                std::vector<std::string> cells = {std::to_string(r.level), std::to_string(r.kills), grouped(r.damage)};
                if (healed)
                    cells.push_back(grouped(r.healing));
                for (int value : {r.shards, r.xp, r.presents, r.items, r.downs})
                    cells.push_back(grouped(value));

                for (size_t c = 0; c < cells.size(); ++c)
                    ui.text(L + nameW + right[c] - 10.0f, ry + 3, cells[c], Font::Text600, 14, ink, Align::Right);
            }
            y += 22.0f * static_cast<float>(v.recap.size()) + 16.0f;
        }

        // The goals this run reached: what is new for the next runs
        if (not v.goals.empty())
        {
            const auto goalsScope = ui.scope(Ui::idOf("goals"));
            ui.text(L, y, "GOALS REACHED", Font::Text700, 11, GOLD, Align::Left, 1.5f);
            y += 22;
            for (size_t i = 0; i < goalRows; ++i)
            {
                const auto rowScope = ui.scope(i);
                const auto& g = v.goals[i];
                ui.box(L, y, W, 34, hex(0x2A2414), 2.0f, GOLD);
                const float nw = ui.text(L + 12, y + 5, ui.fit(g.name, Font::Display, 22, W * 0.4f), Font::Display, 22, GOLD);
                ui.text(L + 12 + nw + 14, y + 10, ui.fit("New: " + g.unlocks, Font::Text600, 13, W - nw - 40), Font::Text600, 13, TEXT);
                y += 40;
            }
            if (goalRows < v.goals.size())
                ui.text(L, y, "and " + std::to_string(v.goals.size() - goalRows) + " more: see Achievements on the title", Font::Text500, 12, MUTED);
        }

        // Your damage, by what dealt it
        if (sources)
        {
            const auto sideScope = ui.scope(Ui::idOf("sources"));
            const float sx = px + panelW + sideGap, sL = sx + 22.0f, sW = sideW - 44.0f;
            const size_t rows = std::min<size_t>(v.sources.size(), static_cast<size_t>(std::max(1.0f, (panelH - 80.0f) / 40.0f)));
            const float sideH = std::min(panelH, 74.0f + 40.0f * static_cast<float>(rows) + (rows < v.sources.size() ? 24.0f : 0.0f));
            ui.raised(sx, py, sideW, sideH, PANEL, 4.0f);
            ui.rect(sx + 2, py + 2, sideW - 4, 6, accent);
            ui.text(sL, py + 28, "YOUR DAMAGE", Font::Text700, 12, MUTED, Align::Left, 1.5f);

            int total = 0, most = 1;
            for (const auto& s : v.sources)
            {
                total += s.damage;
                most = std::max(most, s.damage);
            }
            ui.text(sx + sideW - 22.0f, py + 22, grouped(total), Font::Display, 24, TEXT, Align::Right);

            float ry = py + 60.0f;
            for (size_t i = 0; i < rows; ++i)
            {
                const auto rowScope = ui.scope(i);
                const EndView::Source& s = v.sources[i];
                const std::string share = std::to_string(static_cast<int>(std::lround(100.0f * static_cast<float>(s.damage) / static_cast<float>(std::max(1, total))))) + "%";
                const float vw = ui.text(sL + sW, ry, share, Font::Text600, 12, MUTED, Align::Right);
                const float dw = ui.text(sL + sW - vw - 10.0f, ry - 3, grouped(s.damage), Font::Display, 20, TEXT, Align::Right);
                ui.text(sL, ry, ui.fit(s.name, Font::Text600, 14, sW - vw - dw - 24.0f), Font::Text600, 14, TEXT_DIM);
                ui.rect(sL, ry + 22, sW, 6, withAlpha(INK, 0.6f));
                ui.rect(sL, ry + 22, std::max(2.0f, std::round(sW * static_cast<float>(s.damage) / static_cast<float>(most))), 6, s.colour);
                ry += 40.0f;
            }
            if (rows < v.sources.size())
                ui.text(sL, ry, "and " + std::to_string(v.sources.size() - rows) + " more", Font::Text500, 12, FAINT);
        }

        const float by = py + 342.0f + duelH + recapH + goalsH;

        // Again, or the training ground
        const float aw = 22.0f + ui.measure("ENTER", Font::Display, 16) + 12.0f + ui.measure("Play again", Font::Display, 28) + 24.0f;
        bool over = ui.hover(L, by, aw, 48);
        ui.raised(L, by, aw, 48, GOLD, 3.0f, 2.0f, over ? TEXT : INK);
        const float kw = ui.measure("ENTER", Font::Display, 16) + 12.0f;
        ui.box(L + 12, by + 13, kw, 22, INK, 0.0f);
        ui.text(L + 12 + kw * 0.5f, by + 15, "ENTER", Font::Display, 16, GOLD, Align::Centre);
        ui.text(L + 12 + kw + 12, by + 8, "Play again", Font::Display, 28, GOLD_INK);
        ui.hit(L, by, aw, 48, END_AGAIN);

        if (v.solo)
        {
            const float tx = L + aw + 12.0f;
            const float tw = 12.0f + 26.0f + 12.0f + ui.measure("Training ground", Font::Display, 28) + 16.0f;
            over = ui.hover(tx, by, tw, 48);
            ui.box(tx, by, tw, 48, CARD, 2.0f, over ? GOLD : LINE);
            ui.keycap(tx + 12, by + 11, 26, "T", GOLD, GOLD_INK, 20, 0.0f);
            ui.text(tx + 50, by + 8, "Training ground", Font::Display, 28, TEXT);
            ui.hit(tx, by, tw, 48, END_TRAINING);
        }
    }
}
