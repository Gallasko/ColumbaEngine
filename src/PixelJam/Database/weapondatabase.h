#pragma once

#include "Weapons/weapon.h"

#include "Tiled_Lib/MapData.h"

namespace pg
{
    struct WeaponDatabase : public System<StoragePolicy>
    {
        void addWeapon(const WeaponData& weaponData)
        {
            Weapon weapon;

            weapon.name = weaponData.name;
            weapon.damage = weaponData.damage;
            weapon.projectileSpeed = weaponData.projectileSpeed;
            weapon.projectileLifeTime = weaponData.projectileLifeTime;
            weapon.projectileSize = weaponData.projectileSize;
            weapon.pattern = weaponData.pattern;
            weapon.bulletCount = weaponData.bulletCount;
            weapon.bulletSpreadAngle = weaponData.bulletSpreadAngle;
            weapon.reloadTimeMs = weaponData.reloadTimeMs;
            weapon.barrelSize = weaponData.barrelSize;
            weapon.ammo = weaponData.ammo;

            weapons[weaponData.id] = weapon;
        }

        void addWeapon(int id, const Weapon& weapon)
        {
            weapons[id] = weapon;
        }

        void removeWeapon(int id)
        {
            auto it = weapons.find(id);

            if (it == weapons.end())
            {
                LOG_ERROR("WeaponDatabase", "Weapon with ID " << id << " not found.");
                return;
            }

            weapons.erase(id);
        }

        const Weapon& getWeapon(int id) const
        {
            return weapons.at(id);
        }

        bool hasWeapon(int id) const
        {
            return weapons.find(id) != weapons.end();
        }

        std::map<int, Weapon> weapons;
    };
}