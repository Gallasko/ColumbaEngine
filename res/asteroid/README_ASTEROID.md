# Asteroid Game Scripts

This directory contains the scripts for the Asteroid game demo.

## Current Implementation

### Scripts

1. **init_player.pg** - Initializes the player ship with physics variables and key state tracking
2. **move_player.pg** - Handles key press events (sets key states to 1)
3. **release_player.pg** - Handles key release events (sets key states to 0)
4. **update_player.pg** - Updates player physics every frame based on key states (supports multiple simultaneous keys!)
5. **spawn_enemies.pg** - Spawns and updates asteroids with physics

### spawn_enemies.pg Features

The asteroid spawn system includes:

- **Random spawn positions**: Asteroids spawn from random edges of the screen
- **Random velocities**: Each asteroid moves toward the center with random speed (50-150 px/s)
- **Direction variation**: Slight randomness in direction prevents all asteroids converging at center
- **Rotation**: Each asteroid rotates at a random speed (-2 to 2 rad/s)
- **Screen wrapping**: Asteroids wrap around when they leave the screen
- **Data storage**: All asteroid data stored in sysData for easy access:
  - `asteroid[N]` - Entity ID
  - `asteroid[N]_vx` - X velocity
  - `asteroid[N]_vy` - Y velocity
  - `asteroid[N]_rotSpeed` - Rotation speed
  - `asteroid[N]_size` - Size of asteroid
  - `asteroidCount` - Total number of active asteroids

## Configuration

### Screen Size
Current: 800x600 (lines 8-9 in spawn_enemies.pg)

To change, modify:
```javascript
var screenWidth = 800
var screenHeight = 600
```

### Spawn Rate
Current: Every 2 seconds (line 60 in spawn_enemies.pg)

To change, modify:
```javascript
if (sysData["spawnTimer"] > 2.0)
```

### Asteroid Size Range
Current: 30-60 pixels (line 73 in spawn_enemies.pg)

To change, modify:
```javascript
var size = randomInt(30, 60)
```

### Speed Range
Current: 50-150 px/s (line 122 in spawn_enemies.pg)

To change, modify:
```javascript
var speed = randomRange(50, 150)
```

## Player Controls

### Keyboard Controls:
- **Left Arrow** - Rotate ship counter-clockwise
- **Right Arrow** - Rotate ship clockwise
- **Up Arrow** - Apply thrust in direction ship is facing
- **Down Arrow** - Brake (reduce velocity quickly)
- **Space** - Fire bullet (TODO: not yet implemented)

**🎮 Multi-Key Support**: You can now hold multiple keys at once! For example:
- Hold **Left + Up** to rotate left while thrusting
- Hold **Right + Up** to rotate right while thrusting
- Hold **Down + Left/Right** to brake while rotating

### Player Physics:
The player ship uses realistic momentum-based physics:
- **Rotation**: 3.0 radians per second
- **Thrust Acceleration**: 200 pixels per second²
- **Max Speed**: 300 pixels per second
- **Drag**: 0.99 (slight friction)
- **Screen Wrapping**: Player wraps to opposite edge when leaving screen

All player data stored in `sysData`:
- `playerId` - Entity ID
- `playerVelX`, `playerVelY` - Current velocity
- `playerRotation` - Current rotation angle (radians, 0 = up)
- `playerRotationSpeed` - How fast ship rotates
- `playerThrustAccel` - Thrust acceleration
- `playerMaxSpeed` - Speed cap
- `playerDrag` - Friction coefficient
- `keyLeft`, `keyRight`, `keyUp`, `keyDown`, `keySpace` - Key states (0 or 1)

## Next Steps for Full Game

### TODO:
1. ✅ Random asteroid spawning with velocity
2. ✅ Screen wrapping (asteroids and player)
3. ✅ Rotation animation (asteroids)
4. ✅ Player ship rotation (left/right arrows)
5. ✅ Player ship thrust (up arrow)
6. ✅ Player velocity/momentum
7. ⬜ Shooting bullets (spacebar)
8. ⬜ Collision detection (bullets vs asteroids, player vs asteroids)
9. ⬜ Asteroid splitting (large → 2 medium → 2 small)
10. ⬜ Score tracking
11. ⬜ Lives system
12. ⬜ Game over/win conditions
13. ⬜ Different textures for different sized asteroids
14. ⬜ Sound effects

## Module Usage

The spawn script uses these modules:
- `texture` - For creating visual entities
- `string` - For string concatenation and conversion
- `ecs` - For entity management
- `math` - For trigonometry (sin, cos, sqrt)
- `random` - For random number generation

## How Asteroid Data is Stored

Each asteroid's persistent data is stored in `sysData` using a key pattern:

```
asteroid0 = 164           // Entity ID
asteroid0_vx = 85.3       // X velocity
asteroid0_vx = -42.1      // Y velocity
asteroid0_rotSpeed = 1.2  // Rotation speed
asteroid0_size = 45       // Size in pixels

asteroid1 = 165
asteroid1_vx = -120.5
...

asteroidCount = 2         // Total active asteroids
```

This allows easy iteration and management of all asteroids.