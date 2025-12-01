# PgEngine Profiler

A lightweight, intrusive profiler for tracking frame timing and system execution.

## Features

- **Timeline-based event recording** - Captures begin/end timestamps for all systems
- **Multi-threaded support** - Thread-safe with per-thread tracking
- **Frame markers** - Track complete frame execution
- **CSV export** - Export data for external analysis
- **Zero overhead when disabled** - Compiled out with `#ifndef PROFILE`

## Usage

### 1. Build with Profiling Enabled

Profiling is enabled by default in debug builds via the `-DPROFILE` flag in CMakeLists.txt.

```bash
cd build
cmake ..
make
```

### 2. Run Your Application

The profiler runs automatically when `PROFILE` is defined. It tracks:
- Frame timing
- ECS system execution
- Event dispatch
- Command dispatch
- Rendering
- Input processing

### 3. Export Profile Data

On application shutdown, `profile_data.csv` is automatically exported to the working directory.

The CSV contains:
```csv
frame,name,category,start_ms,duration_ms,thread_id
1,"Frame","Frame",0.123,16.667,12345
1,"EventDispatch","Event",0.145,0.234,12345
1,"Box Bouncer System","System",0.500,0.855,67890
...
```

### 4. Visualize Results

Use the included Python script to generate charts:

```bash
# Install dependencies (one-time)
pip install pandas matplotlib numpy

# Basic usage - shows last 1000 frames
python visualize_profile.py

# Show specific frame breakdown
python visualize_profile.py --frame 5000

# Show last 2000 frames
python visualize_profile.py --num-frames 2000

# Analyze specific frame range
python visualize_profile.py --start-frame 10000 --end-frame 15000

# Save plots to PNG files instead of displaying
python visualize_profile.py --output
```

## Visualization Options

The script generates 4 types of plots:

1. **Frame Timeline** - Horizontal bar chart showing execution times in a single frame
2. **Frame History** - Line graph + stacked area chart of performance over time
3. **Category Breakdown** - Pie charts and statistics showing time distribution
4. **Timeline Heatmap** - Heatmap of all systems across multiple frames

## Performance Impact

- **Event storage**: ~500k events (adjustable in profiler.cpp)
- **Per-frame overhead**: ~50-100 microseconds
- **Memory usage**: ~40 bytes per event (~20MB for full buffer)

## Categories

The profiler tracks these categories:

- **Frame** - Complete frame cycle
- **System** - ECS system execution
- **Event** - Event dispatch processing
- **Command** - Command dispatcher
- **Render** - Rendering operations
- **Input** - Input processing

## Advanced Usage

### Manual Instrumentation

Add profiling to your own code:

```cpp
#include "Profiler/profiler.h"

void myFunction() {
    PROFILE_SCOPE("MyFunction", "Custom");
    // Your code here
}

// Or manual begin/end
void myOtherFunction() {
    PROFILE_BEGIN("Processing", "Custom");
    // ... processing ...
    PROFILE_END("Processing", "Custom");
}
```

### Export Specific Frame Range

```cpp
// Export last 5000 frames
Profiler::instance().exportToCSV("profile.csv", 5000);

// Export all captured frames
Profiler::instance().exportAllToCSV("profile_full.csv");
```

### Runtime Control

```cpp
// Disable profiling at runtime
Profiler::instance().setEnabled(false);

// Clear buffer
Profiler::instance().clear();
```

## Analyzing Fast Frames

For very fast frames (< 1ms), you need many frames to see patterns:

```bash
# For 0.2ms frames, 5000 frames = 1 second of data
python visualize_profile.py --num-frames 5000

# Analyze the last second of execution
python visualize_profile.py --start-frame [MAX-5000] --end-frame [MAX]
```

The visualization script will automatically suggest good frame ranges if your dataset is large.

## Files

- `src/Engine/Profiler/profiler.h` - Profiler interface and macros
- `src/Engine/Profiler/profiler.cpp` - Profiler implementation
- `visualize_profile.py` - Visualization script
- `profile_data.csv` - Exported profile data (generated at runtime)

## Instrumentation Points

Currently instrumented:
- [window.cpp:655-702](src/Engine/window.cpp#L655-L702) - Frame, Render, Input, SwapBuffer
- [entitysystem.cpp:83-127](src/Engine/ECS/entitysystem.cpp#L83-L127) - EventDispatch, CommandDispatch, SaveManager
- [entitysystem.cpp:237-264](src/Engine/ECS/entitysystem.cpp#L237-L264) - All ECS systems

## Troubleshooting

### "Profiler is disabled, skipping CSV export"
Make sure you built with `-DPROFILE` flag.

### "No data found for frame X"
The frame might not be in the captured range. Check the frame range with:
```bash
python visualize_profile.py  # Shows frame range in summary
```

### CSV file is too large
Reduce the buffer size in `profiler.cpp` or export fewer frames:
```cpp
Profiler::instance().exportToCSV("profile.csv", 1000);  // Last 1000 frames only
```

### Visualization is too cluttered
Use frame range filtering:
```bash
python visualize_profile.py --start-frame 1000 --end-frame 2000
```

## Future Enhancements

Potential additions:
- GPU profiling with OpenGL timer queries
- Memory allocation tracking
- Hierarchical profiling (parent-child scopes)
- Real-time visualization overlay (using TTF text system)
- Chrome Tracing format export
- Tracy Profiler integration
