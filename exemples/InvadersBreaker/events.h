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

struct PowerUpCollectedEvent { PowerUpType type; };

struct ScreenFlashEvent { constant::Vector4D color; float duration; };

struct ScreenShakeEvent { float trauma; };

// Todo add this event in the main ecs
struct RemoveEntityEvent
{
    RemoveEntityEvent(_unique_id prefabId) : prefabId(prefabId) {}
    RemoveEntityEvent(const RemoveEntityEvent& rhs) : prefabId(rhs.prefabId) {}

    RemoveEntityEvent& operator=(const RemoveEntityEvent& rhs)
    {
        prefabId = rhs.prefabId;
        return *this;
    }

    _unique_id prefabId;
};

struct AppliedPowerUp { std::string name; };

struct NewWaveStarted { int waveNumber; };