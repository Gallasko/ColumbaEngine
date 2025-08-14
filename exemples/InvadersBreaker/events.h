#pragma once

struct GameStart {};

struct GamePaused {};

struct GameResume {};

struct GameEnd { bool won = true; };

struct AlienDestroyedEvent {};

struct PlayerHitEvent {};