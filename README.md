 OpenGL Gravity Simulator for macOS

This is a 3D gravity simulation using OpenGL, GLFW, and GLM, adapted for macOS.

 Features

- gravity_sim: Main gravity simulator with full physics simulation
- gravity_sim_3Dgrid: 3D grid-based gravity simulator with visual grid
- 3D_test: Basic OpenGL triangle test to verify setup

 Dependencies

The following libraries are required and should be installed via Homebrew:

```bash
brew install glfw glew glm
```

 Building the Project .

 Using Make (Recommended)
bash
 Build all applications
make all

 Build individual applications
make gravity_sim
make gravity_sim_3Dgrid
make 3D_test



 Using VS Code Tasks
- Cmd+Shift+P -> "Tasks: Run Task" -> Select the desired build task
- Available tasks:
  - `build-gravity-sim`: Build the main gravity simulator
  - `build-3dgrid`: Build the 3D grid version
  - `build-3dtest`: Build the OpenGL test
  - `build-all`: Build all applications


 Run main gravity simulator
./gravity_sim

 Run 3D grid gravity simulator
./gravity_sim_3Dgrid

 Run OpenGL test
./3D_test

VS Code
Select from available configurations:
  - "Debug Gravity Simulator"
  - "Debug 3D Grid Simulator"
  - "Debug 3D Test"



 Controls

 <!-- Main Gravity Simulator (gravity_sim) deprecated and replaced by 3D below
- Mouse: Look around (camera rotation)
- W/A/S/D: Move camera
- Space: Pause/unpause simulation
- Mouse clicks: Add objects to the simulation
- Scroll: Zoom in/out -->

 3D Grid Simulator (gravity_sim_3Dgrid)
- Mouse: Look around (camera rotation)
- W/A/S/D: Move camera
- Space: Pause/unpause simulation
- Mouse clicks: Add objects to the simulation
- Scroll: Zoom in/out

 Technical Details

 OpenGL Version
- OpenGL 3.3
- Shaders written in GLSL 3.30

 Physics
- Implements Newtonian gravity simulation
- Real-time physics calculations
- Configurable object properties (mass, density, radius)

 Graphics
- 3D sphere rendering with lighting
- Camera system with mouse and keyboard controls
- Grid visualization (3D grid version)


 Troubleshooting

 Common Issues
1. OpenGL Context Errors: Ensure you're running on macOS 10.9+ with OpenGL 3.3 support
2. Library Not Found: Make sure Homebrew libraries are properly installed
3. Compilation Errors: Check that Xcode command line tools are installed

 Performance
- The simulation is CPU-intensive for large numbers of objects (3D version by default removes objects for better compatability)
- Performance depends on the number of gravitational bodies
- Consider reducing object count for better frame rates


 PS
- This project was originally found on youtube where it was developed for Windows and has been adapted for macOS
