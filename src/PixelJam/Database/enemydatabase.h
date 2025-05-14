#pragma once

#include "ECS/system.h"

#include "Characters/enemy.h"

#include "Tiled_Lib/MapData.h"

namespace pg
{
    struct EnemyDatabase : public System<StoragePolicy>
    {
        void addEnemy(const EnemyData& enemyData)
        {
            EnemyComponentsData enemy;

            // Missing
            // enemy.flag.health = enemyData.health;

            enemy.canSpawn = enemyData.canSpawn;

            enemy.ai.chaseSpeed = enemyData.chaseSpeed;
            enemy.ai.idealDistance = enemyData.idealDistance;
            enemy.ai.orbitThreshold = enemyData.orbitThreshold;
            enemy.ai.attackDistance = enemyData.attackDistance;
            enemy.ai.cooldownTime = enemyData.cooldownTime;
            enemy.ai.wideUpTime = enemyData.wideUpTime;

            enemy.weaponId = enemyData.weaponId;

            enemies[enemyData.objId] = enemy;
        }

        void addEnemy(int id, const EnemyComponentsData& enemy)
        {
            enemies[id] = enemy;
        }

        void removeEnemy(int id)
        {
            auto it = enemies.find(id);

            if (it == enemies.end())
            {
                LOG_ERROR("EnemyDatabase", "Enemy with ID " << id << " not found.");
                return;
            }

            enemies.erase(id);
        }

        const EnemyComponentsData& getEnemy(int id) const
        {
            return enemies.at(id);
        }

        bool hasEnemy(int id) const
        {
            return enemies.find(id) != enemies.end();
        }

        std::map<int, EnemyComponentsData> enemies;
    };
}