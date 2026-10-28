#pragma once

#include <string>
#include <vector>

#include "ECS/entitysystem.h"
#include "Renderer/renderer.h"

#include "glm/glm.hpp"

namespace arena
{
    /**
     * The 2D layer on top of the arena (viewport 1): health and stamina bars, enemy bars,
     * the debug overlay text, the console and the content error banner.
     * Holds entity ids only and looks components up when it writes them.
     */
    class Hud
    {
    public:
        static constexpr const char* FONT = "arena";
        static constexpr const char* MONO = "arena-mono";

        void init(pg::EntitySystem* ecs, pg::MasterRenderer* renderer, float width, float height);
        void resize(float width, float height);

        void setOverlay(const std::string& text, bool visible);
        void setBanner(const std::string& text);         // content errors, red, top centre
        void setCenter(const std::string& text);         // "YOU DIED"
        void setWeapon(const std::string& text);
        void setConsole(bool open, const std::vector<std::string>& lines, const std::string& input);

        /** manaLow: not enough for the cheapest spell (the bar dims). */
        void setBars(float health, float stamina, bool exhausted, float mana = 1.0f, bool manaLow = false);
        void setCrosshair(bool visible);

        /** One bar per entry, screen pixels; extra bars are hidden. */
        struct EnemyBar
        {
            glm::vec2 position;   // centre
            float health;         // 0..1
            float poise;          // 0..1
            bool staggered;
        };
        void setEnemyBars(const std::vector<EnemyBar>& bars);

    private:
        pg::_unique_id makeQuad(float x, float y, float z, float w, float h, const glm::vec4& color);
        pg::_unique_id makeText(float x, float y, float z, const std::string& font, const std::string& text, float scale, const glm::vec4& color);
        void place(pg::_unique_id id, float x, float y, float w, float h, bool visible);
        void color(pg::_unique_id id, const glm::vec4& c);
        void text(pg::_unique_id id, const std::string& t);
        void show(pg::_unique_id id, bool visible);

        pg::EntitySystem* ecs = nullptr;
        float w = 1280.0f, h = 720.0f;

        pg::_unique_id overlay = 0, overlayBack = 0;
        pg::_unique_id banner = 0, center = 0, weapon = 0;
        pg::_unique_id consoleBack = 0, consoleText = 0;
        pg::_unique_id healthBack = 0, healthFill = 0, staminaBack = 0, staminaFill = 0, manaBack = 0, manaFill = 0;
        pg::_unique_id crossH = 0, crossV = 0;
        bool lastCrosshair = true;

        struct EnemyBarIds { pg::_unique_id back, fill, poise; };
        std::vector<EnemyBarIds> enemyBars;

        // Last values written, so unchanged text is not re-laid out every frame
        std::string lastOverlay, lastBanner, lastCenter, lastWeapon, lastConsole;
        bool lastOverlayVisible = true, lastConsoleOpen = false;
    };
}
