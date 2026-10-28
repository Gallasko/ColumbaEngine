#include "stdafx.h"

#include "hud.h"

#include <cmath>

#include "2D/simple2dobject.h"
#include "UI/ttftext.h"

using namespace pg;

namespace arena
{
    namespace
    {
        constexpr size_t UI_VIEWPORT = 1;

        constant::Vector4D rgba(const glm::vec4& c) { return {c.r, c.g, c.b, c.a}; }

        const glm::vec4 PANEL(12, 14, 22, 170);
        const glm::vec4 TEXT(236, 238, 242, 255);
        const glm::vec4 HEALTH(214, 64, 58, 255);
        const glm::vec4 STAMINA(86, 196, 110, 255);
        const glm::vec4 MANA(84, 140, 236, 255);
        const glm::vec4 MANA_LOW(58, 82, 130, 255);
        const glm::vec4 EXHAUSTED(196, 160, 70, 255);
        const glm::vec4 POISE(232, 196, 84, 255);

        constexpr float BAR_W = 260.0f, BAR_H = 12.0f, MARGIN = 18.0f;
        constexpr float ENEMY_BAR_W = 70.0f, ENEMY_BAR_H = 6.0f;
        constexpr size_t MAX_ENEMY_BARS = 32;

        /** The bars from the bottom up: mana, stamina, health. */
        float barY(float h, int row) { return h - MARGIN - BAR_H - static_cast<float>(row) * (BAR_H + 6.0f); }
    }

    pg::_unique_id Hud::makeQuad(float x, float y, float z, float qw, float qh, const glm::vec4& c)
    {
        auto comp = makeUiSimple2DShape(ecs, Shape2D::Square, qw, qh, rgba(c));
        comp.get<PositionComponent>()->setX(x);
        comp.get<PositionComponent>()->setY(y);
        comp.get<PositionComponent>()->setZ(z);
        comp.get<ViewportComponent>()->setViewport(UI_VIEWPORT);
        return comp.entity.id;
    }

    pg::_unique_id Hud::makeText(float x, float y, float z, const std::string& font, const std::string& t, float scale, const glm::vec4& c)
    {
        auto comp = makeTTFText(ecs, x, y, z, font, t, scale, rgba(c));
        comp.get<ViewportComponent>()->setViewport(UI_VIEWPORT);
        return comp.entity.id;
    }

    void Hud::place(pg::_unique_id id, float x, float y, float qw, float qh, bool visible)
    {
        if (auto* p = ecs->getComponent<PositionComponent>(id))
        {
            p->setX(x);
            p->setY(y);
            p->setWidth(qw);
            p->setHeight(qh);
            p->setVisible(visible);
        }
    }

    void Hud::setDashes(float charges, int max)
    {
        if (std::fabs(charges - lastDashes) < 0.01f)
            return;

        lastDashes = charges;

        for (int k = 0; k < static_cast<int>(dashBack.size()); ++k)
        {
            const float x = MARGIN + BAR_W + 10.0f + static_cast<float>(k) * 20.0f;
            const float fill = glm::clamp(charges - static_cast<float>(k), 0.0f, 1.0f);
            show(dashBack[k], k < max);
            place(dashFill[k], x, barY(h, 1), 16.0f * fill, BAR_H, k < max and fill > 0.0f);
            color(dashFill[k], fill >= 1.0f ? glm::vec4(225, 232, 245, 255) : glm::vec4(120, 126, 140, 255));
        }
    }

    void Hud::setHurt(float amount)
    {
        amount = glm::clamp(amount, 0.0f, 1.0f);

        // Only a change goes to the renderer
        if (std::fabs(amount - lastHurt) < 0.004f)
            return;

        lastHurt = amount;
        show(hurt, amount > 0.0f);
        color(hurt, glm::vec4(210, 36, 28, 62.0f * amount));
    }

    void Hud::color(pg::_unique_id id, const glm::vec4& c)
    {
        if (auto* s = ecs->getComponent<Simple2DObject>(id))
            s->setColors(rgba(c));
    }

    void Hud::text(pg::_unique_id id, const std::string& t)
    {
        if (auto* s = ecs->getComponent<TTFText>(id))
            s->setText(t);
    }

    void Hud::show(pg::_unique_id id, bool visible)
    {
        if (auto* p = ecs->getComponent<PositionComponent>(id))
            p->setVisible(visible);
    }

    void Hud::init(EntitySystem* e, MasterRenderer* renderer, float width, float height)
    {
        ecs = e;
        w = width;
        h = height;

        // The 2D camera of viewport 1, one unit per pixel
        auto camEntity = ecs->createEntity();
        auto cam2d = ecs->_attach<BaseCamera2D>(camEntity);
        cam2d->setWidth(w);
        cam2d->setHeight(h);
        cam2d->setNearPlane(-1.0f);
        cam2d->setFarPlane(1.0f);
        renderer->queueRegisterCamera(camEntity.id);

        overlayBack = makeQuad(8, 8, 1, 10, 10, PANEL);
        overlay = makeText(16, 14, 2, MONO, "", 0.8f, TEXT);

        banner = makeText(w * 0.5f - 300.0f, 12, 3, FONT, "", 0.9f, glm::vec4(255, 120, 110, 255));
        center = makeText(w * 0.5f - 90.0f, h * 0.4f, 3, FONT, "", 1.8f, glm::vec4(255, 230, 220, 255));
        weapon = makeText(MARGIN, barY(h, 2) - 56.0f, 2, FONT, "", 0.9f, TEXT);   // two lines: weapons, abilities

        healthBack = makeQuad(MARGIN, barY(h, 2), 1, BAR_W, BAR_H, PANEL);
        healthFill = makeQuad(MARGIN, barY(h, 2), 2, BAR_W, BAR_H, HEALTH);
        staminaBack = makeQuad(MARGIN, barY(h, 1), 1, BAR_W, BAR_H, PANEL);
        staminaFill = makeQuad(MARGIN, barY(h, 1), 2, BAR_W, BAR_H, STAMINA);
        manaBack = makeQuad(MARGIN, barY(h, 0), 1, BAR_W, BAR_H, PANEL);
        manaFill = makeQuad(MARGIN, barY(h, 0), 2, BAR_W, BAR_H, MANA);

        // Crosshair: where the terrain abilities aim
        crossH = makeQuad(w * 0.5f - 7.0f, h * 0.5f - 1.0f, 2, 14.0f, 2.0f, glm::vec4(255, 255, 255, 190));
        crossV = makeQuad(w * 0.5f - 1.0f, h * 0.5f - 7.0f, 2, 2.0f, 14.0f, glm::vec4(255, 255, 255, 190));

        // Dash charges: pips right of the stamina bar
        for (int k = 0; k < 4; ++k)
        {
            const float x = MARGIN + BAR_W + 10.0f + static_cast<float>(k) * 20.0f;
            dashBack.push_back(makeQuad(x, barY(h, 1), 1, 16.0f, BAR_H, PANEL));
            dashFill.push_back(makeQuad(x, barY(h, 1), 2, 16.0f, BAR_H, glm::vec4(225, 232, 245, 255)));
            show(dashBack.back(), false);
            show(dashFill.back(), false);
        }

        // Over the world, under the rest of the HUD
        hurt = makeQuad(0, 0, 0, w, h, glm::vec4(200, 30, 24, 0));
        show(hurt, false);

        consoleBack = makeQuad(0, h - 230.0f, 5, w, 230.0f, glm::vec4(8, 10, 16, 220));
        consoleText = makeText(12, h - 222.0f, 6, MONO, "", 0.8f, TEXT);
        show(consoleBack, false);
        show(consoleText, false);

        for (size_t i = 0; i < MAX_ENEMY_BARS; ++i)
        {
            EnemyBarIds bar;
            bar.back = makeQuad(0, 0, 1, ENEMY_BAR_W, ENEMY_BAR_H, PANEL);
            bar.fill = makeQuad(0, 0, 2, ENEMY_BAR_W, ENEMY_BAR_H, HEALTH);
            bar.poise = makeQuad(0, 0, 2, ENEMY_BAR_W, 2.0f, POISE);
            show(bar.back, false);
            show(bar.fill, false);
            show(bar.poise, false);
            enemyBars.push_back(bar);
        }
    }

    void Hud::resize(float width, float height)
    {
        // The UI is laid out once for the start size; a resize scales it with the window
        (void)width;
        (void)height;
    }

    void Hud::setOverlay(const std::string& t, bool visible)
    {
        if (visible != lastOverlayVisible)
        {
            show(overlay, visible);
            show(overlayBack, visible);
            lastOverlayVisible = visible;
        }

        if (not visible or t == lastOverlay)
            return;

        lastOverlay = t;
        text(overlay, t);

        // The panel follows the text block
        int lines = 1;
        size_t longest = 0, current = 0;

        for (size_t i = 0; i < t.size(); ++i)
        {
            if (t[i] == '\n')
            {
                ++lines;
                longest = std::max(longest, current);
                current = 0;
            }
            else if (t[i] == '\\' and i + 1 < t.size() and t[i + 1] == 'c')
            {
                // Skip a colour command: \c{r,g,b,a}
                const size_t close = t.find('}', i);
                i = close == std::string::npos ? i : close;
            }
            else
            {
                ++current;
            }
        }

        longest = std::max(longest, current);
        place(overlayBack, 8, 8, 16.0f + static_cast<float>(longest) * 7.6f, 14.0f + static_cast<float>(lines) * 16.5f, true);
    }

    void Hud::setBanner(const std::string& t)
    {
        if (t == lastBanner)
            return;

        lastBanner = t;
        text(banner, t);
    }

    void Hud::setCenter(const std::string& t)
    {
        if (t == lastCenter)
            return;

        lastCenter = t;
        text(center, t);
    }

    void Hud::setWeapon(const std::string& t)
    {
        if (t == lastWeapon)
            return;

        lastWeapon = t;
        text(weapon, t);
    }

    void Hud::setConsole(bool open, const std::vector<std::string>& lines, const std::string& input)
    {
        if (open != lastConsoleOpen)
        {
            show(consoleBack, open);
            show(consoleText, open);
            lastConsoleOpen = open;
        }

        if (not open)
            return;

        std::string all;
        for (const auto& l : lines)
            all += l + "\n";
        all += "> " + input + "_";

        if (all == lastConsole)
            return;

        lastConsole = all;
        text(consoleText, all);
    }

    void Hud::setCrosshair(bool visible)
    {
        if (visible == lastCrosshair)
            return;

        show(crossH, visible);
        show(crossV, visible);
        lastCrosshair = visible;
    }

    void Hud::setBars(float health, float stamina, bool exhausted, float mana, bool manaLow)
    {
        health = glm::clamp(health, 0.0f, 1.0f);
        stamina = glm::clamp(stamina, 0.0f, 1.0f);
        mana = glm::clamp(mana, 0.0f, 1.0f);

        place(healthFill, MARGIN, barY(h, 2), BAR_W * health, BAR_H, health > 0.0f);
        place(staminaFill, MARGIN, barY(h, 1), BAR_W * stamina, BAR_H, stamina > 0.0f);
        color(staminaFill, exhausted ? EXHAUSTED : STAMINA);
        place(manaFill, MARGIN, barY(h, 0), BAR_W * mana, BAR_H, mana > 0.0f);
        color(manaFill, manaLow ? MANA_LOW : MANA);
    }

    void Hud::setEnemyBars(const std::vector<EnemyBar>& bars)
    {
        for (size_t i = 0; i < enemyBars.size(); ++i)
        {
            const auto& ids = enemyBars[i];

            if (i >= bars.size())
            {
                show(ids.back, false);
                show(ids.fill, false);
                show(ids.poise, false);
                continue;
            }

            const auto& b = bars[i];
            const float x = b.position.x - ENEMY_BAR_W * 0.5f;
            const float y = b.position.y;

            place(ids.back, x, y, ENEMY_BAR_W, ENEMY_BAR_H, true);
            place(ids.fill, x, y, ENEMY_BAR_W * glm::clamp(b.health, 0.0f, 1.0f), ENEMY_BAR_H, b.health > 0.0f);
            place(ids.poise, x, y + ENEMY_BAR_H + 1.0f, ENEMY_BAR_W * glm::clamp(b.poise, 0.0f, 1.0f), 2.0f, b.poise > 0.0f);
            color(ids.fill, b.staggered ? POISE : HEALTH);
        }
    }
}
