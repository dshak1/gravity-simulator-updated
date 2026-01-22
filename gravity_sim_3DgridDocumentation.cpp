// ============================================================================
// 3D GRAVITY SIMULATOR WITH GRID VISUALIZATION
// Real-time N-body physics simulation using OpenGL 3.3
// ============================================================================

// Graphics libraries - handles OpenGL, window management, and 3D math
#include <GL/glew.h>                        // OpenGL extension loader - gives us modern GL functions
#include <GLFW/glfw3.h>                     // Window creation and input handling (cross-platform)
#include <glm/glm.hpp>                      // Math library - vectors (vec3, vec4) and matrices (mat4)
#include <glm/gtc/matrix_transform.hpp>     // Matrix operations like translate(), perspective()
#include <glm/gtc/type_ptr.hpp>             // Converts GLM types to raw pointers for OpenGL

// Standard C++ libraries
#include <vector>                            // Dynamic arrays for objects and vertices
#include <iostream>                          // Console output for debugging

// ============================================================================
// SHADER PROGRAMS (GLSL) - Run on GPU
// ============================================================================

// VERTEX SHADER - Transforms 3D positions to screen coordinates
// Runs once per vertex, applies Model-View-Projection (MVP) transformation
const char* vertexShaderSource = R"glsl(
#version 330 core                           // OpenGL 3.3 shader version
layout(location=0) in vec3 aPos;            // Input: vertex position from VBO
uniform mat4 model;                         // Model matrix: object position/rotation
uniform mat4 view;                          // View matrix: camera transformation
uniform mat4 projection;                    // Projection matrix: perspective view
void main() {
    // Transform vertex through MVP pipeline: Local → World → Camera → Screen
    gl_Position = projection * view * model * vec4(aPos, 1.0);
})glsl";

// FRAGMENT SHADER - Colors each pixel on screen
// Runs once per pixel, determines final color
const char* fragmentShaderSource = R"glsl(
#version 330 core
out vec4 FragColor;                         // Output: RGBA color for this pixel
uniform vec4 objectColor;                   // Color passed from CPU (set per object)
void main() {
    FragColor = objectColor;                // Simple flat shading - just use the color
}
)glsl";

// ============================================================================
// GLOBAL STATE VARIABLES
// ============================================================================

// Application state
bool running = true;                        // Main loop control - false to exit program
bool pause = false;                         // Pause physics but keep rendering
float simulationSpeed = 1.0f;               // Time multiplier: 0.5x = slow-mo, 10x = fast

// Camera system - First-person free-fly camera
glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f,  1.0f);   // Camera position in world
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);   // Direction camera is facing
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f,  0.0f);   // Up vector (Y-axis)

// Mouse state for look-around
float lastX = 400.0, lastY = 300.0;         // Previous mouse position
float yaw = -90;                            // Horizontal rotation (left/right)
float pitch = 0.0;                          // Vertical rotation (up/down)

// Frame timing - for smooth framerate-independent movement
float deltaTime = 0.0;                      // Time since last frame (seconds)
float lastFrame = 0.0;                      // Timestamp of previous frame

// Window dimensions
int windowWidth = 800;
int windowHeight = 600;

// Physics constants
const double G = 6.6743e-11;                // Gravitational constant (m³ kg⁻¹ s⁻²)
const float c = 299792458.0;                // Speed of light (m/s) - used for relativity viz
float initMass = 5.0f * pow(10, 20) / 5;    // Default mass for new objects

// ============================================================================
// FUNCTION PROTOTYPES - Defined later in the file
// ============================================================================
GLFWwindow* StartGLU();                                                     // Initialize GLFW, create window
GLuint CreateShaderProgram(const char* vertexSource, const char* fragmentSource);  // Compile shaders
void CreateVBOVAO(GLuint& VAO, GLuint& VBO, const float* vertices, size_t vertexCount);  // Upload geometry to GPU
void UpdateCam(GLuint shaderProgram, glm::vec3 cameraPos);                 // Send camera matrix to shader
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);  // Keyboard input
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);     // Mouse clicks
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);           // Mouse wheel zoom
void framebuffer_size_callback(GLFWwindow* window, int width, int height);          // Window resize
void mouse_callback(GLFWwindow* window, double xpos, double ypos);                  // Mouse movement (look around)
glm::vec3 sphericalToCartesian(float r, float theta, float phi);                    // Coordinate conversion
void DrawGrid(GLuint shaderProgram, GLuint gridVAO, size_t vertexCount);            // Render the grid
void renderText(const std::string& text, float x, float y, float scale, GLuint shaderProgram, GLint colorLoc);  // Text rendering

// ============================================================================
// OBJECT CLASS - Represents a celestial body (planet, moon, etc.)
// Encapsulates physics properties, rendering data, and trail system
// ============================================================================
class Object {
    public:
        // === RENDERING DATA (GPU buffers) ===
        GLuint VAO, VBO;                        // Vertex Array Object & Vertex Buffer Object for sphere mesh
        size_t vertexCount;                     // Number of vertices in the sphere
        glm::vec4 color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);  // RGBA color (default: red)

        // === PHYSICS DATA ===
        glm::vec3 position = glm::vec3(400, 300, 0);  // Position in 3D space
        glm::vec3 velocity = glm::vec3(0, 0, 0);      // Velocity vector (direction + speed)
        float mass;                             // Mass in kilograms (affects gravity)
        float density;                          // Density in kg/m³ (determines radius from mass)
        float radius;                           // Visual radius (calculated from mass/density)

        // === STATE FLAGS ===
        bool Initalizing = false;               // True when user is positioning new object
        bool Launched = false;                  // True when object is released into simulation
        bool target = false;                    // Unused - could be for targeting system
        bool hasTrail = false;                  // Whether this object leaves a visible trail

        // === TRAIL SYSTEM ===
        glm::vec3 LastPos = position;           // Previous position (for trail calculation)
        
        // Sub-structure for each trail sphere
        struct TrailSphere {
            glm::vec3 position;                 // Where this trail sphere is located
            GLuint VAO, VBO;                    // Separate mesh for this trail sphere
            size_t vertexCount;
        };
        
        std::vector<TrailSphere> trailSpheres;  // Collection of all trail spheres (history)
        int maxTrailLength = 30;                // Max trail spheres to keep (older ones deleted)

        // === CONSTRUCTOR - Create a new celestial object ===
        // initPosition: starting position in 3D space
        // initVelocity: initial velocity vector
        // mass: object mass in kg
        // density: material density in kg/m³ (default 3344 = Moon's density)
        Object(glm::vec3 initPosition, glm::vec3 initVelocity, float mass, float density = 3344) {   
            this->position = initPosition;
            this->velocity = initVelocity;
            this->mass = mass;
            this->density = density;
            
            // Calculate radius from mass using sphere volume formula: V = 4πr³/3
            // Density ρ = m/V, solving for r: r = ∛(3m/4πρ)
            // Divide by 100000 to scale down for visualization
            this->radius = pow(((3 * this->mass/this->density)/(4 * 3.14159265359)), (1.0f/3.0f)) / 100000;
            
            trailSpheres.clear();               // Start with no trail spheres

            // Generate sphere mesh (array of vertices)
            std::vector<float> vertices = Draw();
            vertexCount = vertices.size();

            // Upload vertices to GPU (creates VAO and VBO)
            CreateVBOVAO(VAO, VBO, vertices.data(), vertexCount);
        }

        // === DRAW METHOD - Generate sphere geometry using spherical coordinates ===
        // Creates a mesh by dividing sphere into stacks (latitude) and sectors (longitude)
        // Returns array of vertex positions (x, y, z) as triangles
        std::vector<float> Draw() {
            std::vector<float> vertices;
            int stacks = 10;                    // Horizontal bands (latitude lines)
            int sectors = 10;                   // Vertical slices (longitude lines)
                                                // 10x10 = 100 quads = 200 triangles = 600 vertices

            // Loop through stacks (top to bottom) and sectors (around circle)
            for(float i = 0.0f; i <= stacks; ++i){
                // theta = polar angle (0 to π) - goes from north pole to south pole
                float theta1 = (i / stacks) * glm::pi<float>();
                float theta2 = (i+1) / stacks * glm::pi<float>();
                
                for (float j = 0.0f; j < sectors; ++j){
                    // phi = azimuthal angle (0 to 2π) - goes around equator
                    float phi1 = j / sectors * 2 * glm::pi<float>();
                    float phi2 = (j+1) / sectors * 2 * glm::pi<float>();
                    
                    // Get 4 corner points of this quad using spherical → Cartesian conversion
                    glm::vec3 v1 = sphericalToCartesian(radius, theta1, phi1);  // Top-left
                    glm::vec3 v2 = sphericalToCartesian(radius, theta1, phi2);  // Top-right
                    glm::vec3 v3 = sphericalToCartesian(radius, theta2, phi1);  // Bottom-left
                    glm::vec3 v4 = sphericalToCartesian(radius, theta2, phi2);  // Bottom-right

                    // Split quad into 2 triangles (OpenGL only renders triangles)
                    // Triangle 1: v1-v2-v3
                    vertices.insert(vertices.end(), {v1.x, v1.y, v1.z}); //      /|
                    vertices.insert(vertices.end(), {v2.x, v2.y, v2.z}); //     / |
                    vertices.insert(vertices.end(), {v3.x, v3.y, v3.z}); //    /__|
                    
                    // Triangle 2: v2-v4-v3 (completes the quad)
                    vertices.insert(vertices.end(), {v2.x, v2.y, v2.z});
                    vertices.insert(vertices.end(), {v4.x, v4.y, v4.z});
                    vertices.insert(vertices.end(), {v3.x, v3.y, v3.z});
                }   
            }
            return vertices;                    // Returns flat array: [x1,y1,z1, x2,y2,z2, ...]
        }
        
        // === UPDATE POSITION - Integrate velocity into position (Euler method) ===
        void UpdatePos(){
            // position += velocity * deltaTime (simplified with /94 scale factor)
            this->position[0] += this->velocity[0] / 94 * simulationSpeed;
            this->position[1] += this->velocity[1] / 94 * simulationSpeed;
            this->position[2] += this->velocity[2] / 94 * simulationSpeed;
            
            // Recalculate radius in case mass changed
            this->radius = pow(((3 * this->mass/this->density)/(4 * 3.14159265359)), (1.0f/3.0f)) / 100000;
            
            // Update trail if this object has one enabled
            if (hasTrail) {
                UpdateTrail();
            }
        }
        
        // === UPDATE VERTICES - Rebuild sphere mesh when radius changes ===
        void UpdateVertices() {
            std::vector<float> vertices = Draw();        // Generate new sphere with current radius
            
            glBindBuffer(GL_ARRAY_BUFFER, VBO);          // Select this object's VBO
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);  // Upload new data
        }
        
        // === GET POSITION - Accessor method ===
        glm::vec3 GetPos() const {
            return this->position;
        }
        
        // === ACCELERATE - Apply force (changes velocity) ===
        // In physics: Force = mass × acceleration, so acceleration = Force / mass
        void accelerate(float x, float y, float z){
            // velocity += acceleration * deltaTime (with /96 scale factor)
            this->velocity[0] += x / 96 * simulationSpeed;
            this->velocity[1] += y / 96 * simulationSpeed;
            this->velocity[2] += z / 96 * simulationSpeed;
        }
        
        // === CHECK COLLISION - Simple sphere-sphere overlap detection ===
        // Returns damping factor: -0.2 if colliding (reverse + dampen), 1.0 if not
        float CheckCollision(const Object& other) {
            // Calculate distance between centers
            float dx = other.position[0] - this->position[0];
            float dy = other.position[1] - this->position[1];
            float dz = other.position[2] - this->position[2];
            float distance = std::pow(dx*dx + dy*dy + dz*dz, (1.0f/2.0f));  // sqrt(dx² + dy² + dz²)
            
            // If distance < sum of radii, spheres are overlapping
            if (other.radius + this->radius > distance){
                return -0.2f;                           // Bounce: reverse velocity and reduce by 80%
            }
            return 1.0f;                                // No collision: keep velocity as-is
        }

        // === UPDATE TRAIL - Add new trail sphere at current position ===
        // Creates breadcrumb trail showing object's path through space
        void UpdateTrail() {
            if (!hasTrail) return;              // Skip if trail disabled for this object
            
            // Don't create trail sphere every frame (too many objects, performance hit)
            static int frameCount = 0;
            frameCount++;
            if (frameCount % 5 == 0) {          // Only add trail sphere every 5 frames
                // Create a new trail sphere at current object position
                TrailSphere newSphere;
                newSphere.position = position;          // Capture current position
                
                // Make trail spheres smaller and lower detail than main object
                float trailSphereRadius = radius * 0.3f;    // 30% of main object size
                std::vector<float> vertices;
                int stacks = 8;                         // Lower detail (main object uses 10)
                int sectors = 8;                        // Fewer triangles = better performance
                
                // Generate sphere vertices (same algorithm as Draw() but with smaller radius)
                for(float i = 0.0f; i <= stacks; ++i){
                    float theta1 = (i / stacks) * glm::pi<float>();
                    float theta2 = (i+1) / stacks * glm::pi<float>();
                    for (float j = 0.0f; j < sectors; ++j){
                        float phi1 = j / sectors * 2 * glm::pi<float>();
                        float phi2 = (j+1) / sectors * 2 * glm::pi<float>();
                        glm::vec3 v1 = sphericalToCartesian(trailSphereRadius, theta1, phi1);
                        glm::vec3 v2 = sphericalToCartesian(trailSphereRadius, theta1, phi2);
                        glm::vec3 v3 = sphericalToCartesian(trailSphereRadius, theta2, phi1);
                        glm::vec3 v4 = sphericalToCartesian(trailSphereRadius, theta2, phi2);

                        // Two triangles per quad
                        vertices.insert(vertices.end(), {v1.x, v1.y, v1.z});
                        vertices.insert(vertices.end(), {v2.x, v2.y, v2.z});
                        vertices.insert(vertices.end(), {v3.x, v3.y, v3.z});
                        
                        vertices.insert(vertices.end(), {v2.x, v2.y, v2.z});
                        vertices.insert(vertices.end(), {v4.x, v4.y, v4.z});
                        vertices.insert(vertices.end(), {v3.x, v3.y, v3.z});
                    }   
                }
                
                // Upload trail sphere to GPU
                newSphere.vertexCount = vertices.size();
                CreateVBOVAO(newSphere.VAO, newSphere.VBO, vertices.data(), vertices.size());
                
                // Add to end of trail vector (most recent)
                trailSpheres.push_back(newSphere);
                
                // Limit trail length (FIFO queue - delete oldest when full)
                if (trailSpheres.size() > maxTrailLength) {
                    glDeleteVertexArrays(1, &trailSpheres[0].VAO);  // Free GPU memory
                    glDeleteBuffers(1, &trailSpheres[0].VBO);
                    trailSpheres.erase(trailSpheres.begin());       // Remove from vector
                }
            }
        }
        
        // === DRAW TRAIL - Render all trail spheres with alpha fading ===
        // Older spheres are more transparent, creating a "fade out" effect
        void DrawTrail(GLuint shaderProgram, GLint colorLoc) {
            if (!hasTrail || trailSpheres.empty()) return;  // Nothing to draw
            
            // Trail color: red (can customize per object)
            glUniform4f(colorLoc, 1.0f, 0.0f, 0.0f, 1.0f);
            
            // Draw each sphere in chronological order (oldest first)
            for (size_t i = 0; i < trailSpheres.size(); ++i) {
                // Calculate alpha based on age: oldest=0.0 (invisible), newest=1.0 (opaque)
                float alpha = (float)(i + 1) / trailSpheres.size();
                glUniform4f(colorLoc, 1.0f, 0.0f, 0.0f, alpha);
                
                // Create model matrix: just translate to sphere's position
                glm::mat4 model = glm::mat4(1.0f);          // Identity matrix
                model = glm::translate(model, trailSpheres[i].position);  // Move to position
                GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
                
                // Draw this trail sphere
                glBindVertexArray(trailSpheres[i].VAO);
                glDrawArrays(GL_TRIANGLES, 0, trailSpheres[i].vertexCount / 3);
            }
            
            glBindVertexArray(0);                           // Unbind VAO
        }
};  // End of Object class

// === GLOBAL SIMULATION STATE ===
std::vector<Object> objs = {};              // All celestial bodies in the simulation

// === GRID SYSTEM ===
std::vector<float> CreateGridVertices(float size, int divisions, const std::vector<Object>& objs);  // Forward declaration
GLuint gridVAO, gridVBO;                    // GPU buffers for the grid mesh (shows spacetime curvature)


// ============================================================================
// MAIN - Entry point of the program
// ============================================================================
int main() {
    // === INITIALIZATION ===
    
    // Create window and initialize OpenGL context
    GLFWwindow* window = StartGLU();
    
    // Compile and link shaders into a program
    GLuint shaderProgram = CreateShaderProgram(vertexShaderSource, fragmentShaderSource);

    // Get uniform locations (so we can send data from CPU to GPU)
    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint objectColorLoc = glGetUniformLocation(shaderProgram, "objectColor");
    glUseProgram(shaderProgram);                // Activate our shader

    // === SETUP INPUT CALLBACKS ===
    glfwSetCursorPosCallback(window, mouse_callback);       // Mouse movement = camera rotation
    glfwSetScrollCallback(window, scroll_callback);         // Scroll wheel = zoom
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);  // Hide cursor, enable infinite movement

    // Center the mouse position (prevents initial jump when mouse moves)
    lastX = windowWidth / 2.0f;
    lastY = windowHeight / 2.0f;

    // === SETUP CAMERA ===
    GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    cameraPos = glm::vec3(0.0f, 1000.0f, 5000.0f);  // Start camera above and back from origin

    // === CREATE INITIAL OBJECTS - Earth-Moon system ===
    objs = {
        // Moon: position (384,400 km scaled), velocity (1022 m/s scaled), mass, density
        Object(glm::vec3(3844, 0, 0), glm::vec3(0, 0, 228), 7.34767309*pow(10, 22), 3344),
        
        // Earth: at origin (0,0,0), stationary, with Earth's real mass and density
        Object(glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), 5.97219*pow(10, 24), 5515),
    };
    
    // Customize object appearances
    objs[0].color = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);  // Moon: grey
    objs[0].hasTrail = true;                             // Moon leaves red trail showing orbit
    objs[1].color = glm::vec4(0.0f, 0.3f, 0.8f, 1.0f);  // Earth: blue
    // === PRINT CONTROL INSTRUCTIONS TO CONSOLE ===
    std::cout << "===== SIMULATION SPEED CONTROLS =====" << std::endl;
    std::cout << "Press 0: Normal speed (1.0x)" << std::endl;
    std::cout << "Press 1: Slow motion (0.5x)" << std::endl;
    std::cout << "Press 2: Fast (2.0x)" << std::endl;
    std::cout << "Press 3: Faster (5.0x)" << std::endl;
    std::cout << "Press 4: Super fast (10.0x)" << std::endl;
    std::cout << "===================================" << std::endl;
    std::cout << "===== CAMERA CONTROLS =====" << std::endl;
    std::cout << "Hold X: 5x camera movement speed" << std::endl;
    std::cout << "WASD: Move camera" << std::endl;
    std::cout << "Mouse: Look around" << std::endl;
    std::cout << "Space/Shift: Up/Down" << std::endl;
    std::cout << "===================================" << std::endl;
    
    // === SETUP GRID ===
    // Create grid mesh (will be warped by gravity each frame)
    std::vector<float> gridVertices = CreateGridVertices(100000.0f, 50, objs);
    CreateVBOVAO(gridVAO, gridVBO, gridVertices.data(), gridVertices.size());
    
    // Debug output: print calculated radii
    std::cout<<"Earth radius: "<<objs[1].radius<<std::endl;
    std::cout<<"Moon radius: "<<objs[0].radius<<std::endl;

    // ========================================================================
    // MAIN GAME LOOP - Runs every frame (target: 60 FPS)
    // ========================================================================
    while (!glfwWindowShouldClose(window) && running == true) {
        
        // === CALCULATE DELTA TIME - Time since last frame ===
        // Used to make physics framerate-independent (60fps vs 144fps behaves same)
        float currentFrame = glfwGetTime();         // Get current time in seconds
        deltaTime = currentFrame - lastFrame;       // Time elapsed since last frame
        lastFrame = currentFrame;

        // === CLEAR SCREEN ===
        // Clear both color (visible pixels) and depth buffer (Z-order for 3D)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // === UPDATE PROJECTION MATRIX ===
        // Recalculate in case window was resized
        float aspectRatio = (float)windowWidth / (float)windowHeight;
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 750000.0f);
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

        // === REGISTER INPUT CALLBACKS ===
        glfwSetKeyCallback(window, keyCallback);            // Keyboard events
        glfwSetMouseButtonCallback(window, mouseButtonCallback);  // Mouse clicks
        // === CAMERA MOVEMENT - Handle continuous key presses ===
        // X key = speed boost (5x faster)
        float speedMultiplier = (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) ? 5.0f : 1.0f;
        float cameraSpeed = 1000.0f * deltaTime * speedMultiplier;  // Scale by deltaTime for framerate independence
        
        // WASD movement (relative to camera direction)
        if (glfwGetKey(window, GLFW_KEY_W)==GLFW_PRESS){
            cameraPos += cameraSpeed * cameraFront;  // Forward
        }
        if (glfwGetKey(window, GLFW_KEY_S)==GLFW_PRESS){
            cameraPos -= cameraSpeed * cameraFront;  // Backward
        }
        if (glfwGetKey(window, GLFW_KEY_A)==GLFW_PRESS){
            // Left: cross product of front and up gives right vector, negate for left
            cameraPos -= cameraSpeed * glm::normalize(glm::cross(cameraFront, cameraUp));
        }
        if (glfwGetKey(window, GLFW_KEY_D)==GLFW_PRESS){
            // Right: cross product of front and up
            cameraPos += cameraSpeed * glm::normalize(glm::cross(cameraFront, cameraUp));
        }
        
        // Vertical movement (absolute world Y-axis)
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS){
            cameraPos += cameraSpeed * cameraUp;     // Up
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS){
            cameraPos -= cameraSpeed * cameraUp;     // Down
        }
        
        // Pause toggle
        if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS){
            pause = true;                             // Freeze physics
        }
        if (glfwGetKey(window, GLFW_KEY_K) == GLFW_RELEASE){
            pause = false;                            // Resume physics
        }
        
        // Quit
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS){
            glfwTerminate();
            glfwWindowShouldClose(window);
            running = false;
        }
        // === UPDATE CAMERA MATRIX ===
        UpdateCam(shaderProgram, cameraPos);        // Send view matrix to shader
        
        // === OBJECT CREATION MODE ===
        // If user is creating a new object (last in vector is initializing)
        if (!objs.empty() && objs.back().Initalizing) {
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
                // Right-click and hold increases mass by 1% per second
                objs.back().mass *= 1.0 + 1.0 * deltaTime;
                
                // Recalculate radius (bigger mass = bigger sphere)
                objs.back().radius = pow(
                    (3 * objs.back().mass / objs.back().density) / 
                    (4 * 3.14159265359f), 
                    1.0f/3.0f
                ) / 100000.0f;
                
                // Rebuild mesh with new radius
                objs.back().UpdateVertices();
            }
        }

        // === RENDER GRID ===
        // Grid shows spacetime curvature (warps around massive objects)
        glUseProgram(shaderProgram);
        glUniform4f(objectColorLoc, 1.0f, 1.0f, 1.0f, 0.25f);  // White, 25% opacity
        
        // Regenerate grid vertices every frame (dynamic - displaces based on gravity)
        gridVertices = CreateGridVertices(10000.0f, 50, objs);
        glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
        glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_DYNAMIC_DRAW);
        DrawGrid(shaderProgram, gridVAO, gridVertices.size());

        // === PHYSICS AND RENDERING LOOP - Process each object ===
        for(auto& obj : objs) {
            // Set this object's color in shader
            glUniform4f(objectColorLoc, obj.color.r, obj.color.g, obj.color.b, obj.color.a);

            // === N-BODY PHYSICS: Calculate gravitational forces from all other objects ===
            // This is O(n²) complexity - every object affects every other object
            for(auto& obj2 : objs){
                // Skip if same object or either is being initialized by user
                if(&obj2 != &obj && !obj.Initalizing && !obj2.Initalizing){
                    
                    // Calculate distance vector between objects
                    float dx = obj2.GetPos()[0] - obj.GetPos()[0];
                    float dy = obj2.GetPos()[1] - obj.GetPos()[1];
                    float dz = obj2.GetPos()[2] - obj.GetPos()[2];
                    float distance = sqrt(dx * dx + dy * dy + dz * dz);  // Euclidean distance

                    if (distance > 0) {  // Avoid division by zero
                        // Normalize direction vector (unit vector pointing from obj to obj2)
                        std::vector<float> direction = {dx / distance, dy / distance, dz / distance};
                        
                        distance *= 1000;  // Scale factor for simulation units
                        
                        // Newton's Law of Universal Gravitation: F = G × m1 × m2 / r²
                        double Gforce = (G * obj.mass * obj2.mass) / (distance * distance);
                        
                        // Convert force to acceleration: a = F / m
                        float acc1 = Gforce / obj.mass;
                        std::vector<float> acc = {direction[0] * acc1, direction[1]*acc1, direction[2]*acc1};
                        
                        // Apply acceleration (changes velocity) if not paused
                        if(!pause){
                            obj.accelerate(acc[0], acc[1], acc[2]);
                        }

                        // Check for collision (sphere-sphere overlap)
                        obj.velocity *= obj.CheckCollision(obj2);  // Returns damping factor
                    }
                }
            }
            
            // If object is being created, update its radius as mass changes
            if(obj.Initalizing){
                obj.radius = pow(((3 * obj.mass/obj.density)/(4 * 3.14159265359)), (1.0f/3.0f)) / 100000;
                obj.UpdateVertices();
            }

            // === UPDATE POSITION - Integrate velocity into position ===
            if(!pause){
                obj.UpdatePos();  // position += velocity * deltaTime
            }
            // === RENDER THIS OBJECT ===
            
            // Create model matrix: translate to object's position in world
            glm::mat4 model = glm::mat4(1.0f);          // Start with identity matrix
            model = glm::translate(model, obj.position);  // Move to object position
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));  // Send to shader
            
            // Draw the sphere
            glBindVertexArray(obj.VAO);                 // Bind this object's vertex data
            glDrawArrays(GL_TRIANGLES, 0, obj.vertexCount / 3);  // Draw all triangles
            
            // === RENDER TRAIL (if enabled) ===
            if (obj.hasTrail) {
                // Use identity matrix for trail (spheres have position baked into vertices)
                glm::mat4 identityModel = glm::mat4(1.0f);
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(identityModel));
                obj.DrawTrail(shaderProgram, objectColorLoc);
            }
        }
        
        // === PRESENT FRAME ===
        glfwSwapBuffers(window);                        // Swap front/back buffers (double buffering)
        glfwPollEvents();                               // Process input events
    }  // End of main game loop

    // ========================================================================
    // CLEANUP - Free all GPU resources (prevent memory leaks)
    // ========================================================================
    
    // Delete all object buffers (VAOs and VBOs)
    for (auto& obj : objs) {
        glDeleteVertexArrays(1, &obj.VAO);          // Delete VAO on GPU
        glDeleteBuffers(1, &obj.VBO);               // Delete VBO on GPU
        
        // Also clean up trail spheres
        if (obj.hasTrail) {
            for (auto& sphere : obj.trailSpheres) {
                glDeleteVertexArrays(1, &sphere.VAO);
                glDeleteBuffers(1, &sphere.VBO);
            }
        }
    }

    // Delete grid buffers
    glDeleteVertexArrays(1, &gridVAO);
    glDeleteBuffers(1, &gridVBO);

    // Delete shader program
    glDeleteProgram(shaderProgram);
    
    // Cleanup GLFW
    glfwTerminate();

    return 0;  // Exit successfully
}  // End of main()


// ////////////////////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////////////////////
// //                                                                        //
// //                  BOILERPLATE FUNCTIONS START HERE                     //
// //          (Standard OpenGL/GLFW setup and utility functions)           //
// //                                                                        //
// ////////////////////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////////////////////


// === START GLU - Initialize GLFW, create window, initialize OpenGL ===
// Returns: GLFWwindow pointer (or nullptr on failure)
GLFWwindow* StartGLU() {
    // Initialize GLFW library
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW, panic" << std::endl;
        return nullptr;
    }
    
    // Tell GLFW what OpenGL version we want (3.3 Core Profile)
    // Core profile = modern OpenGL, no legacy/deprecated functions
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);                      // OpenGL 3.x
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);                      // OpenGL x.3 → 3.3
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);     // Core (not compatibility)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);               // Required for macOS
    
    // Create window (800x600, title, not fullscreen, not shared context)
    GLFWwindow* window = glfwCreateWindow(800, 600, "Gravity Simulator 3D Grid", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return nullptr;
    }
    glfwMakeContextCurrent(window);  // Make this window's OpenGL context current

    // Initialize GLEW (loads OpenGL function pointers)
    glewExperimental = GL_TRUE;  // Enable modern OpenGL extensions
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW." << std::endl;
        glfwTerminate();
        return nullptr;
    }

    // Get actual framebuffer size (can differ from window size on Retina/HiDPI displays)
    glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
    
    // Enable depth testing (Z-buffer): closer objects occlude farther ones
    glEnable(GL_DEPTH_TEST);
    
    // Set viewport (rendering area) to full window
    glViewport(0, 0, windowWidth, windowHeight);
    
    // Enable alpha blending for transparency (grid, trails)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // Standard alpha blending formula
    
    // Register callback for window resize events
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    return window;  // Return window pointer to caller
}

// === CREATE SHADER PROGRAM - Compile vertex and fragment shaders, link into program ===
// vertexSource: GLSL source code for vertex shader
// fragmentSource: GLSL source code for fragment shader
// Returns: Shader program ID (handle to use with glUseProgram)
GLuint CreateShaderProgram(const char* vertexSource, const char* fragmentSource) {
    
    // === COMPILE VERTEX SHADER ===
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);  // Create shader object
    glShaderSource(vertexShader, 1, &vertexSource, nullptr); // Set source code
    glCompileShader(vertexShader);                           // Compile GLSL → GPU code

    // Check for compilation errors
    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "Vertex shader compilation failed: " << infoLog << std::endl;
    }

    // === COMPILE FRAGMENT SHADER ===
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
    glCompileShader(fragmentShader);

    // Check for compilation errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Fragment shader compilation failed: " << infoLog << std::endl;
    }

    // === LINK SHADERS INTO PROGRAM ===
    GLuint shaderProgram = glCreateProgram();          // Create program object
    glAttachShader(shaderProgram, vertexShader);       // Attach vertex shader
    glAttachShader(shaderProgram, fragmentShader);     // Attach fragment shader
    glLinkProgram(shaderProgram);                      // Link them together

    // Check for linking errors
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed: " << infoLog << std::endl;
    }

    // Clean up: shader objects no longer needed after linking
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;  // Return program ID
}
// === CREATE VBO/VAO - Upload vertex data to GPU and configure vertex attributes ===
// VAO (out): Vertex Array Object ID - stores vertex attribute configuration
// VBO (out): Vertex Buffer Object ID - stores actual vertex data
// vertices: Array of floats (x, y, z positions)
// vertexCount: Total number of floats (not vertices - so vertexCount/3 = number of vertices)
void CreateVBOVAO(GLuint& VAO, GLuint& VBO, const float* vertices, size_t vertexCount) {
    // Generate buffer objects on GPU
    glGenVertexArrays(1, &VAO);  // Create VAO
    glGenBuffers(1, &VBO);       // Create VBO

    // Bind VAO first - all subsequent vertex attribute config will be stored in this VAO
    glBindVertexArray(VAO);
    
    // Bind VBO and upload vertex data to GPU
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertexCount * sizeof(float), vertices, GL_STATIC_DRAW);
    // GL_STATIC_DRAW = hint that data won't change often (good for objects)
    // GL_DYNAMIC_DRAW = hint that data changes every frame (good for grid)

    // Tell OpenGL how to interpret the vertex data
    // Attribute 0 (matches "layout(location=0)" in vertex shader)
    glVertexAttribPointer(
        0,                      // Attribute index (0 = position)
        3,                      // Size (3 floats per vertex: x, y, z)
        GL_FLOAT,               // Type of data
        GL_FALSE,               // Don't normalize
        3 * sizeof(float),      // Stride: bytes between consecutive vertices
        (void*)0                // Offset: where does this attribute start in the buffer
    );
    glEnableVertexAttribArray(0);  // Enable attribute 0
    
    glBindVertexArray(0);  // Unbind VAO (good practice)
}

// === UPDATE CAMERA - Calculate view matrix and send to shader ===
void UpdateCam(GLuint shaderProgram, glm::vec3 cameraPos) {
    glUseProgram(shaderProgram);  // Make sure this shader is active
    
    // Create view matrix using glm::lookAt
    // Params: camera position, target (where camera looks), up vector
    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    
    // Send view matrix to shader's "view" uniform
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
}

// === KEY CALLBACK - Handle discrete key press events (not continuous) ===
// Called automatically by GLFW when key state changes
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    bool shiftPressed = (mods & GLFW_MOD_SHIFT) != 0;  // Check if Shift is held
    
    // === SIMULATION SPEED CONTROL (number keys 0-4) ===
    // Only trigger on key press, not while held (avoids spam)
    if (action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_0:  // Normal speed
                simulationSpeed = 1.0f;
                std::cout << "Simulation speed: 1.0x (normal)" << std::endl;
                break;
            case GLFW_KEY_1:  // Slow motion
                simulationSpeed = 0.5f;
                std::cout << "Simulation speed: 0.5x (slow)" << std::endl;
                break;
            case GLFW_KEY_2:  // 2x fast
                simulationSpeed = 2.0f;
                std::cout << "Simulation speed: 2.0x" << std::endl;
                break;
            case GLFW_KEY_3:  // 5x fast
                simulationSpeed = 5.0f;
                std::cout << "Simulation speed: 5.0x" << std::endl;
                break;
            case GLFW_KEY_4:  // 10x super fast
                simulationSpeed = 10.0f;
                std::cout << "Simulation speed: 10.0x (fast)" << std::endl;
                break;
        }
    }

    // === ARROW KEYS - Position new object during initialization ===
    // If user is creating a new object, arrow keys move it in 3D space
    if(!objs.empty() && objs[objs.size() - 1].Initalizing){
        
        // Up/Down arrows without Shift: move along Y-axis
        if (key == GLFW_KEY_UP && (action == GLFW_PRESS || action == GLFW_REPEAT)){
            if (!shiftPressed) {
                objs[objs.size()-1].position[1] += 0.5;  // Up
            }
        };
        if (key == GLFW_KEY_DOWN && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            if (!shiftPressed) {
                objs[objs.size()-1].position[1] -= 0.5;  // Down
            }
        }
        
        // Left/Right arrows: move along X-axis
        if(key == GLFW_KEY_RIGHT && (action == GLFW_PRESS || action == GLFW_REPEAT)){
            objs[objs.size()-1].position[0] += 0.5;  // Right
        };
        if(key == GLFW_KEY_LEFT && (action == GLFW_PRESS || action == GLFW_REPEAT)){
            objs[objs.size()-1].position[0] -= 0.5;  // Left
        };
        
        // Up/Down arrows WITH Shift: move along Z-axis (depth)
        if (key == GLFW_KEY_UP && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            objs[objs.size()-1].position[2] += 0.5;  // Forward
        };
        if (key == GLFW_KEY_DOWN && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            objs[objs.size()-1].position[2] -= 0.5;  // Backward
        }
    };
};
// === MOUSE CALLBACK - Handle mouse movement for camera rotation (look around) ===
// Called automatically by GLFW when mouse moves
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {

    // Calculate how much mouse moved since last frame
    float xoffset = xpos - lastX;        // Horizontal movement
    float yoffset = lastY - ypos;        // Vertical movement (inverted: up = positive)
    lastX = xpos;                        // Update for next frame
    lastY = ypos;

    // Apply sensitivity (reduce movement for smoother control)
    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    // Update rotation angles
    yaw += xoffset;    // Horizontal rotation (left/right)
    pitch += yoffset;  // Vertical rotation (up/down)

    // Clamp pitch to prevent gimbal lock (camera flipping when looking straight up/down)
    if(pitch > 89.0f) pitch = 89.0f;    // Don't look too far up
    if(pitch < -89.0f) pitch = -89.0f;  // Don't look too far down

    // Convert yaw and pitch angles to direction vector (spherical → Cartesian)
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);  // Normalize to unit vector
}
// === MOUSE BUTTON CALLBACK - Handle mouse clicks (create new objects) ===
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods){
    if (button == GLFW_MOUSE_BUTTON_LEFT){
        // Left-click PRESS: Start creating new object
        if (action == GLFW_PRESS){
            // Create new object at origin with default mass
            objs.emplace_back(glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0f, 0.0f, 0.0f), initMass);
            objs[objs.size()-1].Initalizing = true;  // Enter "initialization mode"
        };
        
        // Left-click RELEASE: Finish creating object, launch it into simulation
        if (action == GLFW_RELEASE){
            objs[objs.size()-1].Initalizing = false;  // Exit initialization mode
            objs[objs.size()-1].Launched = true;      // Object now participates in physics
        };
    };
    
    // Right-click mass increase handled in main loop (continuous, not discrete event)
};
// === SCROLL CALLBACK - Mouse wheel zoom (move camera forward/backward) ===
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset){
    float cameraSpeed = 50000.0f * deltaTime;  // Fast zoom speed
    if(yoffset>0){                              // Scroll up = zoom in
        cameraPos += cameraSpeed * cameraFront;
    } else if(yoffset<0){                       // Scroll down = zoom out
        cameraPos -= cameraSpeed * cameraFront;
    }
}

// === SPHERICAL TO CARTESIAN - Convert (r, θ, φ) to (x, y, z) ===
// Used for generating sphere vertices from angles
// r: radius, theta: polar angle (0 to π), phi: azimuthal angle (0 to 2π)
glm::vec3 sphericalToCartesian(float r, float theta, float phi){
    float x = r * sin(theta) * cos(phi);   // Horizontal plane component
    float y = r * cos(theta);              // Vertical component
    float z = r * sin(theta) * sin(phi);   // Horizontal plane component
    return glm::vec3(x, y, z);
};

// === DRAW GRID - Render the grid lines ===
void DrawGrid(GLuint shaderProgram, GLuint gridVAO, size_t vertexCount) {
    glUseProgram(shaderProgram);
    
    // Grid stays at world origin (identity matrix)
    glm::mat4 model = glm::mat4(1.0f);
    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    glBindVertexArray(gridVAO);
    glPointSize(5.0f);  // Unused (we're drawing lines, not points)
    glDrawArrays(GL_LINES, 0, vertexCount / 3);  // Draw as lines (2 verts per line)
    glBindVertexArray(0);
}
// === CREATE GRID VERTICES - Generate 3D grid that warps around massive objects ===
// Visualizes spacetime curvature (Einstein's general relativity concept)
// size: grid size in world units
// divisions: how many grid cells (more = finer detail)
// objs: all objects (used to calculate gravitational displacement)
// Returns: flat array of vertex positions [x1,y1,z1, x2,y2,z2, ...] as line segments
std::vector<float> CreateGridVertices(float size, int divisions, const std::vector<Object>& objs) {
    std::vector<float> vertices;
    float step = size / divisions;      // Size of each grid cell
    float halfSize = size / 2.0f;       // Grid goes from -halfSize to +halfSize

    // === GENERATE GRID LINES ALONG X-AXIS ===
    // Creates horizontal lines parallel to X-axis
    for (int yStep = 3; yStep <= 3; ++yStep) {  // Only one Y layer (flat grid)
        float y = -halfSize*0.3f + yStep * step;  // Y position of this grid layer
        
        for (int zStep = 0; zStep <= divisions; ++zStep) {    // For each Z position
            float z = -halfSize + zStep * step;
            
            for (int xStep = 0; xStep < divisions; ++xStep) {  // Draw line segments along X
                float xStart = -halfSize + xStep * step;
                float xEnd = xStart + step;
                
                // Add line segment: start point, end point (6 floats total)
                vertices.push_back(xStart); vertices.push_back(y); vertices.push_back(z);
                vertices.push_back(xEnd);   vertices.push_back(y); vertices.push_back(z);
            }
        }
    }

    // Commented out: Y-axis lines (vertical lines)
    // Would make grid 3D instead of flat, but disabled for clarity
    
    // === GENERATE GRID LINES ALONG Z-AXIS ===
    // Creates horizontal lines parallel to Z-axis (perpendicular to X lines)
    for (int xStep = 0; xStep <= divisions; ++xStep) {    // For each X position
        float x = -halfSize + xStep * step;
        
        for (int yStep = 3; yStep <= 3; ++yStep) {  // Only one Y layer
            float y = -halfSize*0.3f + yStep * step;
            
            for (int zStep = 0; zStep < divisions; ++zStep) {  // Draw line segments along Z
                float zStart = -halfSize + zStep * step;
                float zEnd = zStart + step;
                
                // Add line segment: start point, end point
                vertices.push_back(x); vertices.push_back(y); vertices.push_back(zStart);
                vertices.push_back(x); vertices.push_back(y); vertices.push_back(zEnd);
            }
        }
    }
    // At this point we have a flat X-Z grid at Y ≈ 0
    // Commented out: Alternative displacement calculation using inverse-square law
    // This was the simple approach, but the Schwarzschild radius method below is more interesting
    
    // === GRAVITATIONAL DISPLACEMENT - Warp the grid ===
    // This is the cool part! Each vertex is displaced based on nearby massive objects
    // Creates "gravity wells" that visualize spacetime curvature
    
    float minz = 0.0f;  // Unused variable (could track minimum displacement)
    
    // Process every vertex (step by 3 because vertices are [x, y, z, x, y, z, ...])
    for (int i = 0; i < vertices.size(); i += 3) {
        glm::vec3 vertexPos(vertices[i], vertices[i+1], vertices[i+2]);  // Current vertex position
        glm::vec3 totalDisplacement(0.0f);  // Accumulate displacement from all objects
        
        // Check distance to each massive object
        for (const auto& obj : objs) {
            glm::vec3 toObject = obj.GetPos() - vertexPos;  // Vector from vertex to object
            float distance = glm::length(toObject);          // Euclidean distance
            
            float distance_m = distance * 1000.0f;  // Convert to meters (scale factor)
            
            // === SCHWARZSCHILD RADIUS - Event horizon of a black hole ===
            // rs = 2GM/c² (the radius where escape velocity = speed of light)
            // For Earth: ~9mm, For Moon: ~0.1mm
            // We use this to visualize gravitational field strength
            float rs = (2*G*obj.mass)/(c*c);
            
            // Displacement formula: creates a "well" shape
            // z = 2√(rs(d - rs)) - deeper well for larger mass or closer distance
            float z = 2 * sqrt(rs*(distance_m - rs)) * 100.0f;  // Scale factor 100 for visibility
            totalDisplacement += z;  // Accumulate (all objects contribute)
        }
        
        // Apply displacement to vertex
        vertexPos += totalDisplacement;
        
        // Update Y coordinate (vertical displacement creates the "well" effect)
        // Divide by 15 to reduce magnitude, subtract 3000 to position below objects
        vertices[i+1] = vertexPos[1] / 15.0f - 3000.0f;
    }
    // Now vertices array has displaced positions - grid bends around massive objects!
    return vertices;  // Return displaced grid vertices ready for rendering
}

// === RENDER TEXT - Simple text rendering (currently unused) ===
// This was an attempt to add on-screen text but is not called in main loop
void renderText(const std::string& text, float x, float y, float scale, GLuint shaderProgram, GLint colorLoc) {
    // Note: This function creates simple quads for each character (not real text rendering)
    // Proper text rendering would use a font texture atlas and FreeType library
    
    GLint currentModelLoc = glGetUniformLocation(shaderProgram, "model");
    glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);  // White text
    
    // Switch to 2D orthographic projection
    glm::mat4 projection = glm::ortho(0.0f, 800.0f, 0.0f, 600.0f, -1.0f, 1.0f);
    GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
    
    glm::mat4 view = glm::mat4(1.0f);  // Identity for 2D
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    
    float characterSize = 10.0f * scale;
    float spacing = characterSize * 0.5f;
    
    // Draw a small square for each character (placeholder, not actual glyphs)
    for (size_t i = 0; i < text.length(); i++) {
        float xpos = x + i * spacing;
        
        // Create a simple quad for each character
        float vertices[] = {
            xpos, y, 0.0f,
            xpos + characterSize, y, 0.0f,
            xpos, y + characterSize, 0.0f,
            xpos + characterSize, y, 0.0f,
            xpos + characterSize, y + characterSize, 0.0f,
            xpos, y + characterSize, 0.0f
        };
        
        // Create temporary VAO/VBO for this character
        GLuint textVAO, textVBO;
        glGenVertexArrays(1, &textVAO);
        glGenBuffers(1, &textVBO);
        
        glBindVertexArray(textVAO);
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        // Draw the character
        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(currentModelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        // Clean up
        glDeleteVertexArrays(1, &textVAO);
        glDeleteBuffers(1, &textVBO);
    }
    
    // Restore 3D projection (will be updated in main loop with current dimensions)
    float aspectRatio = (float)windowWidth / (float)windowHeight;
    projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 750000.0f);
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
}

// === FRAMEBUFFER SIZE CALLBACK - Handle window resize events ===
// Called automatically by GLFW when window size changes
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    windowWidth = width;    // Update global width
    windowHeight = height;  // Update global height
    glViewport(0, 0, width, height);  // Resize OpenGL viewport to match window
}


// ////////////////////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////////////////////
// //                                                                        //
// //                    BOILERPLATE FUNCTIONS END HERE                     //
// //                                                                        //
// ////////////////////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////////////////////







/*
================================================================================
================================================================================
==                                                                            ==
==                     INTERVIEW QUESTION BANK                                ==
==                   (Extensive Technical Q&A)                                ==
==                                                                            ==
================================================================================
================================================================================

This section contains potential interview questions and detailed answers
about this 3D gravity simulator project. Study these to prepare for technical
interviews about C++, graphics programming, and physics simulation.

================================================================================
== SECTION 1: PROJECT OVERVIEW & ARCHITECTURE
================================================================================

Q1: What is this project?
A: Real-time 3D gravity simulator with N-body physics and spacetime curvature
   visualization. Demonstrates OpenGL 3.3, C++17, physics simulation, and 3D math.

Q2: Main game loop structure?
A: Calculate deltaTime → Clear buffers → Update matrices → Process input →
   Update camera → Physics (forces + integration) → Regenerate grid → Render →
   Swap buffers → Poll events. Runs ~60 FPS.

Q3: Major components?
A: Object class (celestial bodies), Grid system (curvature visualization),
   Camera (first-person), Shaders (GLSL), Input handlers, Physics engine.

Q4: Design patterns used?
A: OOP (Object class), Callbacks (GLFW), State Machine (object states),
   Resource Management (cleanup), Game Loop.

================================================================================
== SECTION 2: OPENGL & GRAPHICS
================================================================================

Q5: MVP transformation?
A: Model (object→world) × View (camera) × Projection (perspective) = vertex transform.
   gl_Position = projection * view * model * vertex

Q6: VAO vs VBO?
A: VBO stores vertex data on GPU. VAO stores configuration of how to read VBO.
   VAO remembers attribute pointers, avoiding reconfiguration.

Q7: Why depth testing?
A: Ensures closer objects occlude farther ones via Z-buffer. Without it,
   render order determines visibility (wrong for 3D).

Q8: Alpha blending purpose?
A: Enables transparency. Grid: 25% opacity. Trail: fading alpha.
   Formula: source*alpha + dest*(1-alpha)

Q9: What are uniforms?
A: CPU→GPU variables constant per draw call. Examples: matrices, colors.
   Set with glUniform*(), used in shaders.

Q10: GL_STATIC_DRAW vs GL_DYNAMIC_DRAW?
A: STATIC: Data rarely changes (spheres). DYNAMIC: Data changes often (grid).
   Performance hints for GPU optimization.

================================================================================
== SECTION 3: PHYSICS & MATH
================================================================================

Q11: Newton's Law implementation?
A: F = G × m₁ × m₂ / r²
   Calculate distance, force magnitude, convert to vector, then acceleration
   (a=F/m), apply to velocity.

Q12: Integration method?
A: Euler: velocity += accel*dt, position += velocity*dt
   Simple but not highly accurate. Better: Verlet, RK4.

Q13: Physics complexity?
A: O(n²) - nested loops for all object pairs.
   10 objects: 90 calculations. 100 objects: 4,950 calculations.

Q14: Optimize for 1000+ objects?
A: Barnes-Hut octree (O(n log n)), GPU compute shaders, spatial hashing,
   multithreading.

Q15: Schwarzschild radius?
A: rs = 2GM/c² - event horizon radius. Used to visualize gravity strength
   in grid. Earth: ~9mm, Sun: ~3km.

Q16: Sphere generation algorithm?
A: Divide into stacks (latitude) and sectors (longitude). Convert spherical
   (r,θ,φ) to Cartesian: x=r×sin(θ)×cos(φ), y=r×cos(θ), z=r×sin(θ)×sin(φ).
   Split quads into 2 triangles.

Q17: Radius from mass formula?
A: r = ∛(3m/4πρ) / 100000
   From V=4πr³/3 and ρ=m/V. Division scales for visualization.

================================================================================
== SECTION 4: CAMERA & INPUT
================================================================================

Q18: Camera system?
A: First-person free-fly with 6DOF. Position + front vector. Yaw/pitch from
   mouse. WASD movement, cross product for strafe. Space/Shift vertical.

Q19: Gimbal lock?
A: Loss of rotation at pitch=±90°. Prevented by clamping to ±89°.

Q20: deltaTime purpose?
A: Time since last frame. Multiply movement by deltaTime for framerate
   independence. Same physics at 60fps or 144fps.

Q21: Continuous vs discrete input?
A: Continuous: glfwGetKey() in main loop (WASD). Discrete: Callbacks for
   events (number keys for speed).

================================================================================
== SECTION 5: MEMORY & PERFORMANCE
================================================================================

Q22: GPU memory management?
A: Manual: glGenVertexArrays/glGenBuffers (create), glBufferData (upload),
   glDeleteVertexArrays/glDeleteBuffers (free). Cleanup prevents leaks.

Q23: Performance bottlenecks?
A: O(n²) physics, grid regeneration every frame, multiple draw calls.
   For 100+ objects, physics bottleneck.

Q24: Memory leaks prevention?
A: Delete all VAOs/VBOs at end, clean trail spheres when full, delete shader
   program, call glfwTerminate().

================================================================================
== SECTION 6: C++ FEATURES
================================================================================

Q25: Why C++17?
A: Modern features: range-based for with auto, emplace_back, better STL,
   constexpr. Balances features with compatibility.

Q26: Pass by reference usage?
A: const Object& avoids copying large objects. const guarantees no modification.
   CheckCollision(const Object& other) efficient.

Q27: Why auto& in loops?
A: for(auto& obj : objs) creates reference, not copy. & enables modification
   and avoids expensive copying.

Q28: std::vector<float> for vertices?
A: OpenGL expects flat array [x,y,z,x,y,z...]. vector<float> passes directly
   to glBufferData() without conversion.

================================================================================
== SECTION 7: LIBRARIES
================================================================================

Q29: What does GLFW do?
A: Cross-platform window creation, OpenGL context, input handling, event
   polling, time queries.

Q30: What does GLEW do?
A: Loads OpenGL function pointers at runtime based on driver capabilities.
   Must call glewInit() after context creation.

Q31: What does GLM provide?
A: Header-only math library: vectors, matrices, transformations (translate,
   rotate, scale), camera functions (lookAt, perspective). GLSL-compatible.

================================================================================
== SECTION 8: ADVANCED TOPICS
================================================================================

Q32: Add lighting how?
A: Phong shading: add normals, calculate ambient/diffuse/specular in fragment
   shader based on light direction and normal.

Q33: Add textures how?
A: Add UV coordinates, load images, create texture objects, bind and sample
   in fragment shader. Could add Earth/Moon textures.

Q34: Could parallelize physics?
A: Yes: GPU compute shaders (best), CPU multithreading (thread pool/OpenMP),
   or SIMD (SSE/AVX). GPU has thousands of parallel threads.

Q35: Implement Barnes-Hut?
A: Build octree (divide space into 8 cubes), store center of mass per node,
   use approximation for far nodes, recurse for close ones. O(n²) → O(n log n).

================================================================================
== INTERVIEW TIPS
================================================================================

✓ Start with high-level overview, go deep when asked
✓ Draw MVP pipeline on whiteboard
✓ Explain Newton's law formula
✓ Know O(n²) complexity and optimizations
✓ Discuss trade-offs (Euler vs RK4, simple collision, etc.)
✓ Mention what you'd improve (architecture, testing, performance)
✓ Show enthusiasm for graphics/physics!
✓ Practice explaining to non-technical person

REMEMBER:
- You built something real and impressive
- Admit what you don't know, show how you'd learn it
- Engineering is about trade-offs - discuss them
- Show what you learned from this project
- Be proud!

Good luck! 🚀
*/



