#pragma once

#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "ECS/entitysystem.h"
#include "Renderer/renderer.h"

#include "glm/glm.hpp"

namespace pg
{
    struct TTFTextSystem;
}

namespace arena
{
    /**
     * The look of the UI (UI_SPEC.md, style B): colour tokens (0-255), the 7 x 7 pixel icons.
     */
    namespace ui
    {
        using Colour = glm::vec4;

        constexpr Colour hex(unsigned v, float alpha = 1.0f)
        {
            return Colour(static_cast<float>((v >> 16) & 0xFF), static_cast<float>((v >> 8) & 0xFF), static_cast<float>(v & 0xFF), 255.0f * alpha);
        }

        inline Colour withAlpha(Colour c, float alpha) { return Colour(c.r, c.g, c.b, 255.0f * alpha); }

        const Colour INK = hex(0x0B0C10);
        const Colour EMPTY_PIP = hex(0x0B0C10, 0.55f);
        const Colour PANEL = hex(0x101218, 0.95f);
        const Colour CARD = hex(0x1C1F28);
        const Colour CARD_DIM = hex(0x15171E);
        const Colour WELL = hex(0x0F1116);
        const Colour LINE = hex(0x3A3E4A);
        const Colour TEXT = hex(0xECEEF2);
        const Colour TEXT_DIM = hex(0xB9BECB);
        const Colour MUTED = hex(0x8E94A3);
        const Colour FAINT = hex(0x6B7180);
        const Colour GOLD = hex(0xFFD678);
        const Colour GOLD_INK = hex(0x2A1E08);
        const Colour HEALTH = hex(0xD6403A);
        const Colour HEALTH_NUMBER = hex(0xF2675F);
        const Colour STAMINA = hex(0x56C46E);
        const Colour EXHAUSTED = hex(0xC4A046);
        const Colour EXHAUSTED_TEXT = hex(0xE2C46E);
        const Colour MANA = hex(0x548CEC);
        const Colour MANA_LOW = hex(0x3A5282);
        const Colour MANA_TEXT = hex(0x8DB4F5);
        const Colour POISE = hex(0xE8C454);
        const Colour SHARDS = hex(0xBEA0FF);
        const Colour SHARD_ON_GOLD = hex(0x6A4BC8);
        const Colour BOSS = hex(0xE678FF);
        const Colour BOSS_ENRAGED = hex(0xFF4FA0);
        const Colour BOSS_NAME_ENRAGED = hex(0xFF8AC0);
        const Colour BOSS_TAG_INK = hex(0x2A0A30);
        const Colour BOSS_EMPTY = hex(0x2A1630);
        const Colour WARNING = hex(0xFF786E);
        const Colour GOOD = hex(0x8AE59E);
        const Colour COOLING = hex(0x9AA0AE);
        const Colour FIRE = hex(0xFF6A3D);
        const Colour FIRE_TEXT = hex(0xFF7A4D);
        const Colour ICE = hex(0x9EE8FF);
        const Colour VOLT = hex(0xFFE14A);

        const Colour KIND_STAT = hex(0x96C8FF);
        const Colour KIND_WEAPON = hex(0xFFBE78);
        const Colour KIND_SKILL = hex(0x8CE6AA);
        const Colour KIND_ELEMENT = hex(0xFF9696);
        const Colour KIND_FORM = hex(0xD2AAFF);
        const Colour KIND_MODIFIER = hex(0xFF9EC8);

        /** An element's colour ("fire", "ice", "volt"), and the one its names are written in. */
        Colour elementColour(const std::string& element);
        Colour elementText(const std::string& element);
        /** A shop offer kind's colour ("Stat", "Weapon", "Skill", "Element", "Spell form", "Modifier"). */
        Colour kindColour(const std::string& kind);

        /** A 7 x 7 icon: seven rows, '#' filled. An unknown name is empty. */
        const std::vector<std::string>& icon(const std::string& name);

        enum class Font { Display, Text500, Text600, Text700, Mono };

        /** Where the base layout's coordinates are measured from: the window's edges, or the centred 1280 x 720 frame. */
        enum class Anchor { TopLeft, Top, TopRight, Left, Centre, Right, BottomLeft, Bottom, BottomRight };

        enum class Align { Left, Centre, Right };

        constexpr float BASE_W = 1280.0f, BASE_H = 720.0f;
    }

    /**
     * An immediate-mode 2D layer on the HUD viewport: each frame, the screens draw what they show
     * (rects, text, icons) in the 1280 x 720 base layout, and register where they can be clicked.
     *
     * Behind it, engine entities that last: each piece drawn keeps its own entity from one frame to
     * the next, found by where it is drawn from (the file and line of the call, the scopes around it,
     * and how many times that place drew already this frame). A piece that comes or goes never moves
     * another onto a different entity, so the engine only hears of what really changed (a piece
     * handed another's entity changes its position, colour and text at once, and the engine does not
     * apply those in the same pass: it flickered). Loops whose items may draw more or less put each
     * item in its own scope.
     *
     * Scale: s = min(window / base), every number times s then rounded, so pips and borders stay
     * crisp. Draw order is the order of the calls: each piece goes just above whatever it overlaps
     * that was drawn before it (pieces side by side share a level and batch together).
     */
#define UI_HERE int line_ = __builtin_LINE(), const char* file_ = __builtin_FILE()

    class Ui
    {
    public:
        using Key = uint64_t;

        /** The fonts, at the sizes the layout picks from. Before the first frame. */
        static void registerFonts(pg::TTFTextSystem* ttf);

        void init(pg::EntitySystem* ecs, pg::MasterRenderer* renderer, float width, float height);
        void resize(float width, float height);

        void begin();
        void end();

        /** A part of the screen with its own entities. */
        void section(const std::string& name);

        /** An item of a loop: what it draws keeps its entities whatever the items before it drew. */
        class Scope
        {
        public:
            Scope(Ui& ui, uint64_t id);
            ~Scope();
            Scope(const Scope&) = delete;
            Scope& operator=(const Scope&) = delete;

        private:
            Ui& ui;
        };
        Scope scope(uint64_t id) { return Scope(*this, id); }
        /** A string as a scope id (a part's, a spell's name). */
        static uint64_t idOf(const std::string& s);

        void anchor(ui::Anchor a) { current = a; }
        ui::Anchor anchor() const { return current; }

        float scale() const { return s; }
        float width() const { return w; }
        float height() const { return h; }

        /** Base coordinates (current anchor) to window pixels, and back. */
        float X(float x) const;
        float Y(float y) const;
        float baseX(float px) const;
        float baseY(float py) const;

        // ---- Drawing, in base coordinates (the trailing line and file: where it is drawn from) ----
        void rect(float x, float y, float w, float h, const ui::Colour& c, UI_HERE);
        /** A box with a border: the border colour at full size, the fill inset by `border`. */
        void box(float x, float y, float w, float h, const ui::Colour& fill, float border = 2.0f, const ui::Colour& borderColour = ui::INK, UI_HERE);
        /** An outline only. */
        void frame(float x, float y, float w, float h, float thickness, const ui::Colour& c, UI_HERE);
        /** A dashed outline (dash and gap of `dash`). */
        void dashed(float x, float y, float w, float h, float thickness, float dash, const ui::Colour& c, UI_HERE);
        /** A box with a hard ink shadow offset by `shadow`. */
        void raised(float x, float y, float w, float h, const ui::Colour& fill, float shadow, float border = 2.0f, const ui::Colour& borderColour = ui::INK, UI_HERE);

        /** Text with its top at y. Returns its width (base units). `tracking`: extra px between letters. */
        float text(float x, float y, const std::string& t, ui::Font f, float size, const ui::Colour& c, ui::Align a = ui::Align::Left, float tracking = 0.0f, UI_HERE);
        /** Text with a hard ink copy under it, offset by `shadow`. */
        float textShadow(float x, float y, const std::string& t, ui::Font f, float size, const ui::Colour& c, float shadow, ui::Align a = ui::Align::Left, float tracking = 0.0f,
                         UI_HERE);
        float measure(const std::string& t, ui::Font f, float size, float tracking = 0.0f) const;
        /** From a text's top to its baseline (base units): texts of different sizes on one baseline. */
        float ascent(ui::Font f, float size) const;
        /** Text whose baseline is at `baseline`. */
        float textOn(float x, float baseline, const std::string& t, ui::Font f, float size, const ui::Colour& c, ui::Align a = ui::Align::Left, float tracking = 0.0f,
                     float shadow = 0.0f, UI_HERE);

        /** A rect in window pixels, whatever the anchor (full-width bars, the screen's edges). */
        void rectPx(float x0, float y0, float x1, float y1, const ui::Colour& c, UI_HERE);

        /** The text cut to `width`, with an ellipsis when it had to be cut. */
        std::string fit(const std::string& t, ui::Font f, float size, float width) const;
        /** Words onto lines no wider than `width` (base units). */
        std::vector<std::string> wrap(const std::string& t, ui::Font f, float size, float width) const;
        /** Wrapped text, `lineHeight` apart; at most `maxLines` (0: all). Returns the height used. */
        float paragraph(float x, float y, float width, const std::string& t, ui::Font f, float size, const ui::Colour& c, float lineHeight, int maxLines = 0, UI_HERE);

        /** A 7 x 7 icon at `cell` px per cell, with an ink copy offset by `shadow` (0: none). */
        void icon(const std::string& name, float x, float y, float cell, const ui::Colour& c, float shadow = 0.0f, UI_HERE);

        /** A key cap: a box with the key's name centred in it. */
        void keycap(float x, float y, float size, const std::string& key, const ui::Colour& fill, const ui::Colour& ink, float fontSize, float border = 2.0f,
                    const ui::Colour& borderColour = ui::INK, float shadow = 0.0f, UI_HERE);
        /** A key cap as wide as its label needs (at least `size`). Returns its width. */
        float keycapWide(float x, float y, float size, const std::string& key, const ui::Colour& fill, const ui::Colour& ink, float fontSize, float border = 2.0f,
                         const ui::Colour& borderColour = ui::INK, float shadow = 0.0f, UI_HERE);

        // ---- Clicks and the mouse ----
        /** Where a click does `action` this frame (base coordinates, current anchor). */
        void hit(float x, float y, float w, float h, int action);
        /** The action under a window pixel, from the regions of the last frame drawn; 0 for none. */
        int hitAt(float px, float py) const;
        void mouse(float px, float py) { mx = px; my = py; }
        /** The mouse over a base rect (current anchor). */
        bool hover(float x, float y, float w, float h) const;

        /** For the overlay: pieces drawn and changes sent to the engine last frame, and the most changes in a frame lately. */
        int piecesDrawn() const { return lastPieces; }
        int changesSent() const { return lastChanges; }
        int mostChanges() const { return mostChangesLately; }

    private:
        struct QuadSlot
        {
            pg::_unique_id id = 0;
            float x = -1, y = -1, w = -1, h = -1, z = -1;
            ui::Colour c = ui::Colour(-1);
            bool visible = false, known = false;
            int frame = -1;          // the last frame it was drawn in
        };

        struct TextSlot
        {
            pg::_unique_id id = 0;
            float x = -1, y = -1, z = -1, scale = -1, tracking = -1;
            std::string text, font;
            ui::Colour c = ui::Colour(-1);
            bool visible = false, known = false;
            int frame = -1;
        };

        struct Pool
        {
            std::vector<QuadSlot> quads;
            std::vector<TextSlot> texts;
            std::unordered_map<Key, size_t> quadKeys, textKeys;   // where a piece is drawn from -> its entity
            std::unordered_map<Key, int> drawn;                   // how many times a place drew this frame
        };

        struct Region
        {
            float x0, y0, x1, y1;
            int action;
        };

        static Key mix(Key k, uint64_t v);
        /** Where a call is drawn from, in the scopes around it. */
        Key site(int line, const char* file) const;
        /** The entity of the next piece drawn from `base` this frame (`count`: the slots there are now, for a new one). */
        size_t claim(std::unordered_map<Key, size_t>& keys, Key base, size_t count);

        /** The atlas for a font at a pixel size, and the scale to draw it at. */
        std::pair<std::string, float> pick(ui::Font f, float px) const;

        /** The draw level of a piece covering these window pixels: above what it overlaps. */
        float levelFor(float x0, float y0, float x1, float y1);

        // The drawing itself, by key
        void quadK(Key k, float x0, float y0, float x1, float y1, const ui::Colour& c);
        void rectK(Key k, float x, float y, float w, float h, const ui::Colour& c) { quadK(k, X(x), Y(y), X(x + w), Y(y + h), c); }
        void boxK(Key k, float x, float y, float w, float h, const ui::Colour& fill, float border, const ui::Colour& borderColour);
        void raisedK(Key k, float x, float y, float w, float h, const ui::Colour& fill, float shadow, float border, const ui::Colour& borderColour);
        float textK(Key k, float x, float y, const std::string& t, ui::Font f, float size, const ui::Colour& c, ui::Align a, float tracking);
        float textShadowK(Key k, float x, float y, const std::string& t, ui::Font f, float size, const ui::Colour& c, float shadow, ui::Align a, float tracking);

        pg::EntitySystem* ecs = nullptr;
        pg::TTFTextSystem* ttf = nullptr;
        pg::_unique_id camera = 0;
        float w = ui::BASE_W, h = ui::BASE_H, s = 1.0f, fx = 0.0f, fy = 0.0f;
        ui::Anchor current = ui::Anchor::TopLeft;

        std::map<std::string, Pool> pools;
        Pool* pool = nullptr;
        std::vector<Key> scopes;
        int frameCount = 0;
        int pieces = 0, changes = 0, lastPieces = 0, lastChanges = 0, mostChangesLately = 0, mostChangesNow = 0, statFrames = 0;

        // Draw levels: a coarse grid of the highest level drawn in each cell this frame
        static constexpr float CELL = 32.0f;
        static constexpr float BASE_LEVEL = 10.0f, MAX_LEVEL = 3900.0f;
        int gridW = 1, gridH = 1;
        std::vector<float> grid;

        std::vector<Region> regions, lastRegions;
        float mx = -1.0f, my = -1.0f;

        mutable std::unordered_map<std::string, float> measured;
    };
}
