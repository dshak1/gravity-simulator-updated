# OpenGL Gravity Simulator for macOS

This is a 3D gravity simulation using OpenGL, GLFW, and GLM, adapted for macOS.

## Features

- **gravity_sim**: Main gravity simulator with full physics simulation
- **gravity_sim_3Dgrid**: 3D grid-based gravity simulator with visual grid
- **3D_test**: Basic OpenGL triangle test to verify setup

## Dependencies

The following libraries are required and should be installed via Homebrew:

```bash
brew install glfw glew glm
```

## Building the Project .

### Using Make (Recommended)
bash
# Build all applications
make all

# Build individual applications
make gravity_sim
make gravity_sim_3Dgrid
make 3D_test

# Clean build artifacts
make clean
```

### Using VS Code Tasks
- **Cmd+Shift+P** → "Tasks: Run Task" → Select the desired build task
- Available tasks:
  - `build-gravity-sim`: Build the main gravity simulator
  - `build-3dgrid`: Build the 3D grid version
  - `build-3dtest`: Build the OpenGL test
  - `build-all`: Build all applications

## Running the Applications

### Command Line
```bash
# Run main gravity simulator
./gravity_sim

# Run 3D grid gravity simulator
./gravity_sim_3Dgrid

# Run OpenGL test
./3D_test
```

### VS Code
- **F5** to run with debugging
- Select from available configurations:
  - "Debug Gravity Simulator"
  - "Debug 3D Grid Simulator"
  - "Debug 3D Test"

## Controls

### Main Gravity Simulator (`gravity_sim`)
- **Mouse**: Look around (camera rotation)
- **W/A/S/D**: Move camera
- **Space**: Pause/unpause simulation
- **Mouse clicks**: Add objects to the simulation
- **Scroll**: Zoom in/out

### 3D Grid Simulator (`gravity_sim_3Dgrid`)
- **Mouse**: Look around (camera rotation)
- **W/A/S/D**: Move camera
- **Space**: Pause/unpause simulation
- **Mouse clicks**: Add objects to the simulation
- **Scroll**: Zoom in/out

## Technical Details

### OpenGL Version
- Uses OpenGL 3.3 Core Profile
- Shaders written in GLSL 3.30
- Optimized for macOS compatibility

### Physics
- Implements Newtonian gravity simulation
- Real-time physics calculations
- Configurable object properties (mass, density, radius)

### Graphics
- 3D sphere rendering with lighting
- Camera system with mouse and keyboard controls
- Grid visualization (3D grid version)

## Troubleshooting

### Common Issues

1. **OpenGL Context Errors**: Ensure you're running on macOS 10.9+ with OpenGL 3.3 support
2. **Library Not Found**: Make sure Homebrew libraries are properly installed
3. **Compilation Errors**: Check that Xcode command line tools are installed

### Performance
- The simulation is CPU-intensive for large numbers of objects
- Performance depends on the number of gravitational bodies
- Consider reducing object count for better frame rates

## Project Structure

```
gravity_sim-main/
├── gravity_sim.cpp           # Main gravity simulator
├── gravity_sim_3Dgrid.cpp    # 3D grid gravity simulator
├── 3D_test.cpp               # OpenGL test application
├── Makefile                  # Build configuration
└── .vscode/                  # VS Code configuration
    ├── c_cpp_properties.json # IntelliSense configuration
    ├── tasks.json            # Build tasks
    ├── launch.json           # Debug configuration
    └── settings.json         # Editor settings
```

## Notes

- This project was originally developed for Windows and has been adapted for macOS
- The OpenGL context setup has been optimized for macOS compatibility
- All build configurations use Clang++ with appropriate framework linkage
