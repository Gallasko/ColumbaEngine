#include "stdafx.h"

#include "ui.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "2D/simple2dobject.h"
#include "Renderer/camera.h"
#include "UI/ttftext.h"

using namespace pg;

namespace arena
{
    namespace
    {
        constexpr size_t UI_VIEWPORT = 1;

        constant::Vector4D rgba(const ui::Colour& c) { return {c.r, c.g, c.b, c.a}; }

        // The sizes each font is rasterised at: a text is drawn from the nearest one at or above its size
        const std::vector<int> DISPLAY_SIZES = {15, 20, 26, 34, 44, 56, 72, 96, 128};   // the atlas holds 128 px at most
        const std::vector<int> TEXT_SIZES = {11, 14, 18, 24, 32};
        const std::vector<int> MONO_SIZES = {13, 17, 26};

        const char* fontFile(ui::Font f)
        {
            switch (f)
            {
                case ui::Font::Display: return "res/font/Jersey15/Jersey15-Regular.ttf";
                case ui::Font::Text500: return "res/font/Rubik/Rubik-500.ttf";
                case ui::Font::Text600: return "res/font/Rubik/Rubik-600.ttf";
                case ui::Font::Text700: return "res/font/Rubik/Rubik-700.ttf";
                case ui::Font::Mono: return "res/font/Inter/static/Inter_18pt-Regular.ttf";
            }
            return "";
        }

        const char* fontKey(ui::Font f)
        {
            switch (f)
            {
                case ui::Font::Display: return "ui-display";
                case ui::Font::Text500: return "ui-text500";
                case ui::Font::Text600: return "ui-text600";
                case ui::Font::Text700: return "ui-text700";
                case ui::Font::Mono: return "ui-mono";
            }
            return "";
        }

        const std::vector<int>& sizesOf(ui::Font f)
        {
            if (f == ui::Font::Display)
                return DISPLAY_SIZES;
            if (f == ui::Font::Mono)
                return MONO_SIZES;
            return TEXT_SIZES;
        }

        using Icon = std::vector<std::string>;

        const std::map<std::string, Icon>& icons()
        {
            static const std::map<std::string, Icon> all = {
                {"wall",      {"#######", "##.####", "#######", "####.##", "#######", "##.####", "#######"}},
                {"pit",       {".......", ".......", "##...##", ".#...#.", ".#...#.", ".#####.", "......."}},
                {"mine",      {".#####.", "#..#..#", "...#...", "...#...", "...#...", "...#...", "...#..."}},
                {"whirlwind", {"#######", "#.....#", "#.###.#", "#.#.#.#", "#.#...#", "#.#####", "#......"}},
                {"leap",      {"....###", ".....##", "....#.#", "...#...", "..#....", ".#.....", "#......"}},
                {"fire",      {"...#...", "..##...", "..###.#", ".####.#", "#######", "#######", ".#####."}},
                {"ice",       {"#..#..#", ".#.#.#.", "..###..", "#######", "..###..", ".#.#.#.", "#..#..#"}},
                {"volt",      {"...###.", "..###..", ".###...", "#######", "...###.", "..###..", ".##...."}},
                {"lock",      {"..###..", ".#...#.", ".#...#.", "#######", "###.###", "###.###", "#######"}},
                {"shard",     {"...#...", "..###..", ".#####.", "#######", ".#####.", "..###..", "...#..."}},
                {"skull",     {".#####.", "#######", "#..#..#", "#######", ".##.##.", ".#####.", ".#.#.#."}},
                {"check",     {".......", "......#", ".....##", "#...##.", "##.##..", ".###...", "..#...."}},
                {"arrow_left",  {"...#...", "..##...", ".######", "#######", ".######", "..##...", "...#..."}},
                {"arrow_right", {"...#...", "...##..", "######.", "#######", "######.", "...##..", "...#..."}},
                {"keen",      {"......#", ".....##", "....##.", "...##..", "#.##...", ".##....", "#.#...."}},
                {"beam",      {"......#", ".....#.", "#######", "#######", ".....#.", "......#", "......."}},
                {"spear",     {"......#", ".....##", "....###", "...#...", "..#....", ".#.....", "#......"}},
                {"chain",     {"##.....", "#.#....", ".###...", "..#.#..", "...###.", "....#.#", ".....##"}},
                {"crown",     {"#..#..#", "##.#.##", "#######", "#######", ".......", ".......", "......."}},
                {"bolt",      {".......", "....##.", "#..####", ".######", "#..####", "....##.", "......."}},
                {"zone",      {".......", ".......", ".#####.", "#.....#", "#.....#", ".#####.", "......."}},
                {"nova",      {"#..#..#", ".#...#.", "...#...", "#.###.#", "...#...", ".#...#.", "#..#..#"}},
                {"multiply",  {".......", "#...#..", ".#...#.", "..#...#", ".#...#.", "#...#..", "......."}},
                {"extend",    {".......", "#...#..", "#...##.", "#######", "#...##.", "#...#..", "......."}},
                {"enlarge",   {"###.###", "##...##", "#.....#", ".......", "#.....#", "##...##", "###.###"}},
                {"empower",   {"...#...", "..###..", ".#####.", "...#...", "...#...", "..###..", "..###.."}},
                {"heart",     {".##.##.", "#######", "#######", "#######", ".#####.", "..###..", "...#..."}},
                {"shield",    {"#######", "#######", "#######", "#######", ".#####.", "..###..", "...#..."}},
                {"dash",      {".......", "...##..", "##.###.", "...####", "##.###.", "...##..", "......."}},
                {"drop",      {"...#...", "..###..", ".#####.", "#######", "#######", ".#####.", "..###.."}},
                {"clock",     {".#####.", "#..#..#", "#..#..#", "#..##.#", "#.....#", "#.....#", ".#####."}},
                {"target",    {".#####.", "#.....#", "#.###.#", "#.#.#.#", "#.###.#", "#.....#", ".#####."}},
                {"plus",      {"..###..", "..###..", "#######", "#######", "#######", "..###..", "..###.."}},
                {"magnet",    {"##...##", "##...##", "#.....#", "#.....#", "#.....#", ".#...#.", "..###.."}},
                {"wind",      {"#####..", "......#", "..####.", ".......", "######.", "......#", "#####.."}},
                {"eye",       {"...#...", "..#.#..", ".#...#.", "#..#..#", ".#...#.", "..#.#..", "...#..."}},
                {"stacks",    {".......", "#######", ".......", "#######", ".......", "#######", "......."}},
                {"sword",     {"......#", ".....#.", "....#..", "...#...", "#.#....", ".#.....", "#.#...."}},
                {"dagger",    {".......", "......#", ".....#.", "....#..", ".##....", ".#.....", "#......"}},
                {"star",      {"...#...", "...#...", "#######", ".#####.", "..###..", ".##.##.", "#.....#"}},
                {"you",       {"..###..", "..###..", "...#...", "#######", "...#...", "..#.#..", ".#...#."}},
            };
            return all;
        }
    }

    namespace ui
    {
        Colour elementColour(const std::string& element)
        {
            if (element == "fire") return FIRE;
            if (element == "ice") return ICE;
            if (element == "volt") return VOLT;
            return TEXT;
        }

        Colour elementText(const std::string& element)
        {
            return element == "fire" ? FIRE_TEXT : elementColour(element);
        }

        Colour kindColour(const std::string& kind)
        {
            if (kind == "Stat") return KIND_STAT;
            if (kind == "Weapon") return KIND_WEAPON;
            if (kind == "Skill") return KIND_SKILL;
            if (kind == "Element") return KIND_ELEMENT;
            if (kind == "Spell form") return KIND_FORM;
            if (kind == "Modifier") return KIND_MODIFIER;
            return TEXT_DIM;
        }

        const std::vector<std::string>& icon(const std::string& name)
        {
            static const std::vector<std::string> none;
            auto it = icons().find(name);
            return it != icons().end() ? it->second : none;
        }
    }

    // ---- Setup ------------------------------------------------------------------------------------

    void Ui::registerFonts(TTFTextSystem* ttf)
    {
        for (ui::Font f : {ui::Font::Display, ui::Font::Text500, ui::Font::Text600, ui::Font::Text700, ui::Font::Mono})
            for (int size : sizesOf(f))
                ttf->registerFont(fontFile(f), std::string(fontKey(f)) + "-" + std::to_string(size), size);
    }

    void Ui::init(EntitySystem* e, MasterRenderer* renderer, float width, float height)
    {
        ecs = e;
        ttf = ecs->getSystem<TTFTextSystem>();

        // The 2D camera of viewport 1, one unit per window pixel
        auto camEntity = ecs->createEntity();
        auto cam2d = ecs->_attach<BaseCamera2D>(camEntity);
        cam2d->setNearPlane(-1.0f);
        cam2d->setFarPlane(1.0f);
        camera = camEntity.id;
        renderer->queueRegisterCamera(camEntity.id);

        resize(width, height);
    }

    void Ui::resize(float width, float height)
    {
        if (width <= 0.0f or height <= 0.0f)
            return;

        w = width;
        h = height;
        s = std::min(w / ui::BASE_W, h / ui::BASE_H);
        fx = std::floor((w - ui::BASE_W * s) * 0.5f);
        fy = std::floor((h - ui::BASE_H * s) * 0.5f);

        if (auto* cam = ecs ? ecs->getComponent<BaseCamera2D>(camera) : nullptr)
        {
            cam->setWidth(w);
            cam->setHeight(h);
        }

        gridW = std::max(1, static_cast<int>(std::ceil(w / CELL)));
        gridH = std::max(1, static_cast<int>(std::ceil(h / CELL)));
        measured.clear();
    }

    void Ui::begin()
    {
        for (auto& [name, p] : pools)
        {
            p.usedQuads = 0;
            p.usedTexts = 0;
        }

        grid.assign(static_cast<size_t>(gridW * gridH), BASE_LEVEL - 1.0f);
        regions.clear();
        current = ui::Anchor::TopLeft;
        section("main");
    }

    void Ui::end()
    {
        // What was not drawn this frame is hidden
        for (auto& [name, p] : pools)
        {
            for (size_t i = p.usedQuads; i < p.quads.size(); ++i)
            {
                QuadSlot& q = p.quads[i];
                if (q.visible or not q.known)
                {
                    if (auto* pos = ecs->getComponent<PositionComponent>(q.id))
                    {
                        pos->setVisible(false);
                        q.visible = false;
                        q.known = true;
                    }
                }
            }

            for (size_t i = p.usedTexts; i < p.texts.size(); ++i)
            {
                TextSlot& t = p.texts[i];
                if (t.visible or not t.known)
                {
                    if (auto* pos = ecs->getComponent<PositionComponent>(t.id))
                    {
                        pos->setVisible(false);
                        t.visible = false;
                        t.known = true;
                    }
                }
            }
        }

        lastRegions.swap(regions);

        if (measured.size() > 4000)
            measured.clear();
    }

    void Ui::section(const std::string& name)
    {
        pool = &pools[name];
    }

    // ---- Coordinates ------------------------------------------------------------------------------

    float Ui::X(float x) const
    {
        switch (current)
        {
            case ui::Anchor::TopLeft: case ui::Anchor::Left: case ui::Anchor::BottomLeft:
                return std::round(x * s);
            case ui::Anchor::TopRight: case ui::Anchor::Right: case ui::Anchor::BottomRight:
                return w - std::round((ui::BASE_W - x) * s);
            default:
                return fx + std::round(x * s);
        }
    }

    float Ui::Y(float y) const
    {
        switch (current)
        {
            case ui::Anchor::TopLeft: case ui::Anchor::Top: case ui::Anchor::TopRight:
                return std::round(y * s);
            case ui::Anchor::BottomLeft: case ui::Anchor::Bottom: case ui::Anchor::BottomRight:
                return h - std::round((ui::BASE_H - y) * s);
            default:
                return fy + std::round(y * s);
        }
    }

    float Ui::baseX(float px) const
    {
        switch (current)
        {
            case ui::Anchor::TopLeft: case ui::Anchor::Left: case ui::Anchor::BottomLeft:
                return px / s;
            case ui::Anchor::TopRight: case ui::Anchor::Right: case ui::Anchor::BottomRight:
                return ui::BASE_W - (w - px) / s;
            default:
                return (px - fx) / s;
        }
    }

    float Ui::baseY(float py) const
    {
        switch (current)
        {
            case ui::Anchor::TopLeft: case ui::Anchor::Top: case ui::Anchor::TopRight:
                return py / s;
            case ui::Anchor::BottomLeft: case ui::Anchor::Bottom: case ui::Anchor::BottomRight:
                return ui::BASE_H - (h - py) / s;
            default:
                return (py - fy) / s;
        }
    }

    float Ui::levelFor(float x0, float y0, float x1, float y1)
    {
        const int cx0 = std::clamp(static_cast<int>(std::floor(x0 / CELL)), 0, gridW - 1);
        const int cy0 = std::clamp(static_cast<int>(std::floor(y0 / CELL)), 0, gridH - 1);
        const int cx1 = std::clamp(static_cast<int>(std::floor((x1 - 0.5f) / CELL)), 0, gridW - 1);
        const int cy1 = std::clamp(static_cast<int>(std::floor((y1 - 0.5f) / CELL)), 0, gridH - 1);

        float level = BASE_LEVEL - 1.0f;
        for (int cy = cy0; cy <= cy1; ++cy)
            for (int cx = cx0; cx <= cx1; ++cx)
                level = std::max(level, grid[static_cast<size_t>(cy * gridW + cx)]);

        level = std::min(level + 1.0f, MAX_LEVEL);

        for (int cy = cy0; cy <= cy1; ++cy)
            for (int cx = cx0; cx <= cx1; ++cx)
                grid[static_cast<size_t>(cy * gridW + cx)] = level;

        return level;
    }

    // ---- Drawing ----------------------------------------------------------------------------------

    void Ui::putQuad(float x0, float y0, float x1, float y1, const ui::Colour& c)
    {
        if (x1 <= x0 or y1 <= y0 or c.a <= 0.0f)
            return;

        const float z = levelFor(x0, y0, x1, y1);

        if (pool->usedQuads >= pool->quads.size())
        {
            auto comp = makeUiSimple2DShape(ecs, Shape2D::Square, x1 - x0, y1 - y0, rgba(c));
            comp.get<ViewportComponent>()->setViewport(UI_VIEWPORT);
            QuadSlot slot;
            slot.id = comp.entity.id;
            pool->quads.push_back(slot);
        }

        QuadSlot& q = pool->quads[pool->usedQuads++];
        auto* pos = ecs->getComponent<PositionComponent>(q.id);

        // Made this frame: its components come with the next one
        if (not pos)
            return;

        if (not q.known or q.x != x0 or q.y != y0 or q.w != x1 - x0 or q.h != y1 - y0 or q.z != z)
        {
            pos->setX(x0);
            pos->setY(y0);
            pos->setZ(z);
            pos->setWidth(x1 - x0);
            pos->setHeight(y1 - y0);
            q.x = x0;
            q.y = y0;
            q.w = x1 - x0;
            q.h = y1 - y0;
            q.z = z;
        }

        if (not q.known or q.c != c)
        {
            if (auto* shape = ecs->getComponent<Simple2DObject>(q.id))
                shape->setColors(rgba(c));
            q.c = c;
        }

        if (not q.known or not q.visible)
        {
            pos->setVisible(true);
            q.visible = true;
        }

        q.known = true;
    }

    void Ui::rect(float x, float y, float rw, float rh, const ui::Colour& c)
    {
        putQuad(X(x), Y(y), X(x + rw), Y(y + rh), c);
    }

    void Ui::box(float x, float y, float bw, float bh, const ui::Colour& fill, float border, const ui::Colour& borderColour)
    {
        if (border > 0.0f)
        {
            rect(x, y, bw, bh, borderColour);
            rect(x + border, y + border, bw - 2.0f * border, bh - 2.0f * border, fill);
        }
        else
        {
            rect(x, y, bw, bh, fill);
        }
    }

    void Ui::frame(float x, float y, float fw, float fh, float t, const ui::Colour& c)
    {
        rect(x, y, fw, t, c);
        rect(x, y + fh - t, fw, t, c);
        rect(x, y + t, t, fh - 2.0f * t, c);
        rect(x + fw - t, y + t, t, fh - 2.0f * t, c);
    }

    void Ui::dashed(float x, float y, float fw, float fh, float t, float dash, const ui::Colour& c)
    {
        for (float d = 0.0f; d < fw; d += dash * 2.0f)
        {
            rect(x + d, y, std::min(dash, fw - d), t, c);
            rect(x + d, y + fh - t, std::min(dash, fw - d), t, c);
        }

        for (float d = t; d < fh - t; d += dash * 2.0f)
        {
            rect(x, y + d, t, std::min(dash, fh - t - d), c);
            rect(x + fw - t, y + d, t, std::min(dash, fh - t - d), c);
        }
    }

    void Ui::raised(float x, float y, float rw, float rh, const ui::Colour& fill, float shadow, float border, const ui::Colour& borderColour)
    {
        if (shadow > 0.0f)
            rect(x + shadow, y + shadow, rw, rh, ui::INK);
        box(x, y, rw, rh, fill, border, borderColour);
    }

    std::pair<std::string, float> Ui::pick(ui::Font f, float px) const
    {
        const auto& sizes = sizesOf(f);
        int chosen = sizes.back();

        for (int size : sizes)
        {
            if (static_cast<float>(size) >= px * 0.92f)
            {
                chosen = size;
                break;
            }
        }

        return {std::string(fontKey(f)) + "-" + std::to_string(chosen), px / static_cast<float>(chosen)};
    }

    float Ui::measure(const std::string& t, ui::Font f, float size, float tracking) const
    {
        if (t.empty() or not ttf)
            return 0.0f;

        const auto [font, scale] = pick(f, size * s);
        const std::string key = font + "|" + std::to_string(scale) + "|" + std::to_string(tracking) + "|" + t;

        if (auto it = measured.find(key); it != measured.end())
            return it->second;

        const float width = ttf->measureText(font, t, scale, 0.0f, 0.0f, tracking * s).width / s;
        measured[key] = width;
        return width;
    }

    float Ui::ascent(ui::Font f, float size) const
    {
        if (not ttf)
            return size * 0.8f;

        const auto [font, scale] = pick(f, size * s);
        const std::string key = "ascent|" + font + "|" + std::to_string(scale);

        if (auto it = measured.find(key); it != measured.end())
            return it->second;

        const float a = ttf->measureText(font, "H", scale).ascender / s;
        measured[key] = a;
        return a;
    }

    float Ui::textOn(float x, float baseline, const std::string& t, ui::Font f, float size, const ui::Colour& c, ui::Align a, float tracking, float shadow)
    {
        return textShadow(x, baseline - ascent(f, size), t, f, size, c, shadow, a, tracking);
    }

    float Ui::text(float x, float y, const std::string& t, ui::Font f, float size, const ui::Colour& c, ui::Align a, float tracking)
    {
        if (t.empty())
            return 0.0f;

        const float tw = measure(t, f, size, tracking);
        float left = x;

        if (a == ui::Align::Right)
            left = x - tw;
        else if (a == ui::Align::Centre)
            left = x - tw * 0.5f;

        if (c.a <= 0.0f)
            return tw;

        const auto [font, scale] = pick(f, size * s);
        const float px = X(left), py = Y(y);
        const float z = levelFor(px, py, px + tw * s, py + size * s * 1.2f);

        if (pool->usedTexts >= pool->texts.size())
        {
            auto comp = makeTTFText(ecs, px, py, z, font, t, scale, rgba(c));
            comp.get<ViewportComponent>()->setViewport(UI_VIEWPORT);
            TextSlot slot;
            slot.id = comp.entity.id;
            pool->texts.push_back(slot);
        }

        TextSlot& slot = pool->texts[pool->usedTexts++];
        auto* pos = ecs->getComponent<PositionComponent>(slot.id);
        auto* comp = ecs->getComponent<TTFText>(slot.id);

        if (not pos or not comp)
            return tw;

        if (not slot.known or slot.font != font)
        {
            comp->setFontPath(font);
            slot.font = font;
        }

        if (not slot.known or slot.scale != scale)
        {
            comp->setScale(scale);
            slot.scale = scale;
        }

        if (not slot.known or slot.tracking != tracking * s)
        {
            comp->setLetterSpacing(tracking * s);
            slot.tracking = tracking * s;
        }

        if (not slot.known or slot.c != c)
        {
            comp->setColors(rgba(c));
            slot.c = c;
        }

        if (not slot.known or slot.text != t)
        {
            comp->setText(t);
            slot.text = t;
        }

        if (not slot.known or slot.x != px or slot.y != py or slot.z != z)
        {
            pos->setX(px);
            pos->setY(py);
            pos->setZ(z);
            slot.x = px;
            slot.y = py;
            slot.z = z;
        }

        if (not slot.known or not slot.visible)
        {
            pos->setVisible(true);
            slot.visible = true;
        }

        slot.known = true;
        return tw;
    }

    float Ui::textShadow(float x, float y, const std::string& t, ui::Font f, float size, const ui::Colour& c, float shadow, ui::Align a, float tracking)
    {
        if (shadow > 0.0f)
            text(x + shadow, y + shadow, t, f, size, ui::Colour(ui::INK.r, ui::INK.g, ui::INK.b, c.a), a, tracking);
        return text(x, y, t, f, size, c, a, tracking);
    }

    std::string Ui::fit(const std::string& t, ui::Font f, float size, float width) const
    {
        if (measure(t, f, size) <= width)
            return t;

        // Whole characters off the end (not into a UTF-8 sequence) until it fits with the ellipsis
        std::string cut = t;
        while (not cut.empty())
        {
            do
                cut.pop_back();
            while (not cut.empty() and (static_cast<unsigned char>(cut.back()) & 0xC0) == 0x80);

            if (not cut.empty() and (static_cast<unsigned char>(cut.back()) & 0x80))
                cut.pop_back();

            const std::string candidate = cut + "\xE2\x80\xA6";
            if (measure(candidate, f, size) <= width)
                return candidate;
        }

        return "";
    }

    std::vector<std::string> Ui::wrap(const std::string& t, ui::Font f, float size, float width) const
    {
        std::vector<std::string> lines;
        std::string line, word;
        std::istringstream words(t);

        while (words >> word)
        {
            const std::string candidate = line.empty() ? word : line + " " + word;
            if (not line.empty() and measure(candidate, f, size) > width)
            {
                lines.push_back(line);
                line = word;
            }
            else
            {
                line = candidate;
            }
        }

        if (not line.empty())
            lines.push_back(line);
        return lines;
    }

    float Ui::paragraph(float x, float y, float width, const std::string& t, ui::Font f, float size, const ui::Colour& c, float lineHeight, int maxLines)
    {
        auto lines = wrap(t, f, size, width);

        if (maxLines > 0 and static_cast<int>(lines.size()) > maxLines)
        {
            lines.resize(static_cast<size_t>(maxLines));
            lines.back() += "\xE2\x80\xA6";
        }

        for (size_t i = 0; i < lines.size(); ++i)
            text(x, y + static_cast<float>(i) * lineHeight, lines[i], f, size, c);

        return static_cast<float>(lines.size()) * lineHeight;
    }

    void Ui::icon(const std::string& name, float x, float y, float cell, const ui::Colour& c, float shadow)
    {
        const auto& rows = ui::icon(name);

        auto draw = [&](float ox, float oy, const ui::Colour& colour) {
            for (size_t r = 0; r < rows.size(); ++r)
            {
                // A run of filled cells is one rect
                const std::string& row = rows[r];
                size_t k = 0;
                while (k < row.size())
                {
                    if (row[k] != '#')
                    {
                        ++k;
                        continue;
                    }

                    size_t end = k;
                    while (end < row.size() and row[end] == '#')
                        ++end;

                    rect(ox + static_cast<float>(k) * cell, oy + static_cast<float>(r) * cell, static_cast<float>(end - k) * cell, cell, colour);
                    k = end;
                }
            }
        };

        if (shadow > 0.0f)
            draw(x + shadow, y + shadow, ui::Colour(ui::INK.r, ui::INK.g, ui::INK.b, c.a));
        draw(x, y, c);
    }

    void Ui::keycap(float x, float y, float size, const std::string& key, const ui::Colour& fill, const ui::Colour& ink, float fontSize, float border,
                    const ui::Colour& borderColour, float shadow)
    {
        raised(x, y, size, size, fill, shadow, border, borderColour);
        text(x + size * 0.5f, y + (size - fontSize) * 0.5f - 1.0f, key, ui::Font::Display, fontSize, ink, ui::Align::Centre);
    }

    float Ui::keycapWide(float x, float y, float size, const std::string& key, const ui::Colour& fill, const ui::Colour& ink, float fontSize, float border,
                         const ui::Colour& borderColour, float shadow)
    {
        const float kw = std::max(size, std::ceil(measure(key, ui::Font::Display, fontSize)) + 2.0f * border + 8.0f);
        raised(x, y, kw, size, fill, shadow, border, borderColour);
        text(x + kw * 0.5f, y + (size - fontSize) * 0.5f - 1.0f, key, ui::Font::Display, fontSize, ink, ui::Align::Centre);
        return kw;
    }

    // ---- Clicks -----------------------------------------------------------------------------------

    void Ui::hit(float x, float y, float rw, float rh, int action)
    {
        if (action != 0)
            regions.push_back({X(x), Y(y), X(x + rw), Y(y + rh), action});
    }

    int Ui::hitAt(float px, float py) const
    {
        // The last drawn is on top
        for (auto it = lastRegions.rbegin(); it != lastRegions.rend(); ++it)
            if (px >= it->x0 and px < it->x1 and py >= it->y0 and py < it->y1)
                return it->action;

        return 0;
    }

    bool Ui::hover(float x, float y, float rw, float rh) const
    {
        return mx >= X(x) and mx < X(x + rw) and my >= Y(y) and my < Y(y + rh);
    }
}
