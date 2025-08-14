#pragma once

struct GameStart {};

struct GamePaused {};

struct GameResume {};

struct GameEnd { bool won = true; };

struct AlienDestroyedEvent
{
    float x, y;
    int points;
};

struct PlayerHitEvent {};