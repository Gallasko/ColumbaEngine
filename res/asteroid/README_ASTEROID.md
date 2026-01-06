# Asteroid Game Scripts

This directory contains the scripts for the Asteroid game demo.

## Current Implementation

### Scripts

1. **init_player.pg** - Initializes the player ship with physics variables, key state tracking, and bullet system
2. **move_player.pg** - Handles key press events (sets key states to 1)
3. **release_player.pg** - Handles key release events (sets key states to 0)
4. **update_player.pg** - Updates player physics every frame based on key states and handles bullet firing
5. **update_bullets.pg** - Manages bullet spawning, movement, and lifetime
6. **spawn_enemies.pg** - Spawns and updates asteroids with physics

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
- **Space** - Fire bullet (fully implemented!)

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
- `playerVelX`, `playerVelY` - Current velocity (pixels/second)
- `playerRotation` - Current rotation angle (radians internally, converted to degrees for display)
- `playerRotationSpeed` - How fast ship rotates (3.0 radians/second ≈ 172°/second)
- `playerThrustAccel` - Thrust acceleration (200 pixels/second²)
- `playerMaxSpeed` - Speed cap (300 pixels/second)
- `playerDrag` - Friction coefficient (0.99)
- `keyLeft`, `keyRight`, `keyUp`, `keyDown`, `keySpace` - Key states (0 or 1)

**Note on Rotation**:
- Physics calculations use **radians** (for sin/cos in thrust direction)
- Visual display uses **degrees** (converted automatically in update_player.pg)

## Bullet System

### Bullet Properties:
- **Fire Rate**: 0.15 seconds between shots (prevents bullet spam)
- **Speed**: 400 pixels/second (faster than player max speed)
- **Lifetime**: 1.5 seconds before auto-removal
- **Size**: 5x5 pixels (small projectile)
- **Behavior**: Travels in straight line, no screen wrapping

### How It Works:
1. Player presses Space → Fire cooldown checked
2. If cooldown ready → Bullet spawn data stored in sysData
3. Bullet system spawns entity and applies velocity
4. Bullet moves each frame until lifetime expires or goes off-screen
5. Bullet automatically removed to prevent memory leaks

All bullet data stored in `sysData`:
- `bulletCount` - Total bullets fired (index counter)
- `bullet[N]` - Entity ID of active bullet
- `bullet[N]_x`, `bullet[N]_y` - Spawn position
- `bullet[N]_vx`, `bullet[N]_vy` - Velocity
- `bullet[N]_lifetime` - Remaining lifetime
- `bullet[N]_spawn` - Flag to trigger spawning (1 = spawn, 0 = spawned)

## Asteroid Health and Splitting System

### Three-Tier Asteroid System:

The game features a three-tier asteroid system where larger asteroids break apart into smaller ones:

1. **Big Asteroids** (60px)
   - Health: 3 hits required
   - Splits into: 2 medium asteroids
   - Speed: 50-150 px/s

2. **Medium Asteroids** (40px)
   - Health: 2 hits required
   - Splits into: 2 small asteroids
   - Speed: 60-120 px/s (spawned from big asteroids)

3. **Small Asteroids** (20px)
   - Health: 1 hit required
   - Splits into: Nothing (completely destroyed)
   - Speed: 80-150 px/s (spawned from medium asteroids)

### How It Works:

1. When a bullet hits an asteroid, the asteroid's health decreases by 1
2. If health reaches 0:
   - Big asteroids spawn 2 medium asteroids at the destruction location
   - Medium asteroids spawn 2 small asteroids at the destruction location
   - Small asteroids are completely destroyed with no fragments
3. Fragment asteroids inherit the parent's position but get random velocities in opposite directions
4. Each fragment has its own rotation speed for visual variety

### Asteroid Component Data:

Each asteroid stores the following in its `Asteroid` component:
- `size` - Visual size in pixels (60/40/20)
- `vx`, `vy` - Velocity components
- `rotSpeed` - Rotation speed
- `health` - Current health (decreases when hit)
- `maxHealth` - Maximum health for this asteroid type
- `type` - Asteroid type: "big", "medium", or "small"

### Spawning Custom Asteroids:

To spawn a specific asteroid type, set these values in `sysData` before calling the spawn script:
- `spawnAsteroidType` - "big", "medium", or "small" (defaults to "big")
- `spawnAsteroidX`, `spawnAsteroidY` - Optional spawn position
- `spawnAsteroidVX`, `spawnAsteroidVY` - Optional velocity

## Next Steps for Full Game

### TODO:
1. ✅ Random asteroid spawning with velocity
2. ✅ Screen wrapping (asteroids and player)
3. ✅ Rotation animation (asteroids)
4. ✅ Player ship rotation (left/right arrows)
5. ✅ Player ship thrust (up arrow)
6. ✅ Player velocity/momentum
7. ✅ Shooting bullets (spacebar with fire rate limiting)
8. ✅ Collision detection (bullets vs asteroids)
9. ✅ Asteroid health system and splitting (big(3hp) → 2 medium(2hp) → 2 small(1hp))
10. ⬜ Collision detection (player vs asteroids)
11. ⬜ Score tracking
12. ⬜ Lives system
13. ⬜ Game over/win conditions
14. ⬜ Different textures for different sized asteroids
15. ⬜ Sound effects

## Module Usage

The spawn script uses these modules:
- `texture` - For creating visual entities
- `string` - For string concatenation and conversion
- `ecs` - For entity management
- `math` - For trigonometry (sin, cos, sqrt)
- `random` - For random number generation
- `algorithm` - For utility functions like `contain()` to check if keys exist in tables

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