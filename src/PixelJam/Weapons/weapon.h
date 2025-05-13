#pragma once

#include <string>

namespace pg
{
    struct Bullet
    {

    };

    struct Weapon
    {
        std::string name;

        float damage = 1.0f;

        float projectileSpeed = 500.0f;
        float projectileLifeTime = 1000.0f;

        float projectileSize = 10.0f;

        Bullet bulletType;

        size_t bulletCount = 1;
        size_t bulletSpread = 0;
        float bulletSpreadAngle = 0.0f;

        float reloadTimeMs = 1000.0f;
        size_t barrelSize = 12;
    };
} // namespace pg
