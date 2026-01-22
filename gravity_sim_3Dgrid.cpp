#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>

const char* vertexShaderSource = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
})glsl";

const char* fragmentShaderSource = R"glsl(
#version 330 core
out vec4 FragColor;
uniform vec4 objectColor; // Add this uniform
void main() {
    FragColor = objectColor; // Use the uniform color
}
)glsl";

bool running = true;
bool pause = false;
float simulationSpeed = 1.0f; // Default simulation speed multiplier
glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f,  1.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f,  0.0f);
float lastX = 400.0, lastY = 300.0;
float yaw = -90;
float pitch =0.0;
float deltaTime = 0.0;
float lastFrame = 0.0;

const double G = 6.6743e-11; // m^3 kg^-1 s^-2
const float c = 299792458.0;
float initMass = 5.0f * pow(10, 20) / 5;

GLFWwindow* StartGLU();
GLuint CreateShaderProgram(const char* vertexSource, const char* fragmentSource);
void CreateVBOVAO(GLuint& VAO, GLuint& VBO, const float* vertices, size_t vertexCount);
void UpdateCam(GLuint shaderProgram, glm::vec3 cameraPos);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

void mouse_callback(GLFWwindow* window, double xpos, double ypos);
glm::vec3 sphericalToCartesian(float r, float theta, float phi);
void DrawGrid(GLuint shaderProgram, GLuint gridVAO, size_t vertexCount);

// Function prototype for renderText (declare it before using it)
void renderText(const std::string& text, float x, float y, float scale, GLuint shaderProgram, GLint colorLoc);

class Object {
    public:
        GLuint VAO, VBO;
        glm::vec3 position = glm::vec3(400, 300, 0);
        glm::vec3 velocity = glm::vec3(0, 0, 0);
        size_t vertexCount;
        glm::vec4 color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

        bool Initalizing = false;
        bool Launched = false;
        bool target = false;

        float mass;
        float density;  // kg / m^3  HYDROGEN
        float radius;

        glm::vec3 LastPos = position;
        
        // Trail as spheres
        struct TrailSphere {
            glm::vec3 position;
            GLuint VAO, VBO;
            size_t vertexCount;
        };
        
        std::vector<TrailSphere> trailSpheres;
        int maxTrailLength = 30; // Fewer, larger spheres
        bool hasTrail = false; // Flag to determine if this object should have a trail

        Object(glm::vec3 initPosition, glm::vec3 initVelocity, float mass, float density = 3344) {   
            this->position = initPosition;
            this->velocity = initVelocity;
            this->mass = mass;
            this->density = density;
            this->radius = pow(((3 * this->mass/this->density)/(4 * 3.14159265359)), (1.0f/3.0f)) / 100000;
            
            // Initialize trail vectors (but don't create any spheres yet)
            trailSpheres.clear();

            // Generate vertices (centered at origin)
            std::vector<float> vertices = Draw();
            vertexCount = vertices.size();

            CreateVBOVAO(VAO, VBO, vertices.data(), vertexCount);
        }

        std::vector<float> Draw() {
            std::vector<float> vertices;
            int stacks = 10;
            int sectors = 10;

            // Generate circumference points using integer steps
            for(float i = 0.0f; i <= stacks; ++i){
                float theta1 = (i / stacks) * glm::pi<float>();
                float theta2 = (i+1) / stacks * glm::pi<float>();
                for (float j = 0.0f; j < sectors; ++j){
                    float phi1 = j / sectors * 2 * glm::pi<float>();
                    float phi2 = (j+1) / sectors * 2 * glm::pi<float>();
                    glm::vec3 v1 = sphericalToCartesian(radius, theta1, phi1);
                    glm::vec3 v2 = sphericalToCartesian(radius, theta1, phi2);
                    glm::vec3 v3 = sphericalToCartesian(radius, theta2, phi1);
                    glm::vec3 v4 = sphericalToCartesian(radius, theta2, phi2);

                    // Triangle 1: v1-v2-v3
                    vertices.insert(vertices.end(), {v1.x, v1.y, v1.z}); //      /|
                    vertices.insert(vertices.end(), {v2.x, v2.y, v2.z}); //     / |
                    vertices.insert(vertices.end(), {v3.x, v3.y, v3.z}); //    /__|
                    
                    // Triangle 2: v2-v4-v3
                    vertices.insert(vertices.end(), {v2.x, v2.y, v2.z});
                    vertices.insert(vertices.end(), {v4.x, v4.y, v4.z});
                    vertices.insert(vertices.end(), {v3.x, v3.y, v3.z});
                }   
            }
            return vertices;
        }
        
        void UpdatePos(){
            this->position[0] += this->velocity[0] / 94 * simulationSpeed;
            this->position[1] += this->velocity[1] / 94 * simulationSpeed;
            this->position[2] += this->velocity[2] / 94 * simulationSpeed;
            this->radius = pow(((3 * this->mass/this->density)/(4 * 3.14159265359)), (1.0f/3.0f)) / 100000;
            
            // Update trail after position change
            if (hasTrail) {
                UpdateTrail();
            }
        }
        void UpdateVertices() {
            // Generate new vertices with current radius
            std::vector<float> vertices = Draw();
            
            // Update the VBO with new vertex data
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        }
        glm::vec3 GetPos() const {
            return this->position;
        }
        void accelerate(float x, float y, float z){
            this->velocity[0] += x / 96 * simulationSpeed;
            this->velocity[1] += y / 96 * simulationSpeed;
            this->velocity[2] += z / 96 * simulationSpeed;
        }
        float CheckCollision(const Object& other) {
            float dx = other.position[0] - this->position[0];
            float dy = other.position[1] - this->position[1];
            float dz = other.position[2] - this->position[2];
            float distance = std::pow(dx*dx + dy*dy + dz*dz, (1.0f/2.0f));
            if (other.radius + this->radius > distance){
                return -0.2f;
            }
            return 1.0f;
        }

        // Method to update the trail positions
        void UpdateTrail() {
            if (!hasTrail) return; // Skip if trail is not enabled
            
            // Only add a new sphere every few frames
            static int frameCount = 0;
            frameCount++;
            if (frameCount % 5 == 0) { // Add a new sphere every 5 frames
                // Create a new trail sphere
                TrailSphere newSphere;
                newSphere.position = position;
                
                // Create a smaller sphere
                float trailSphereRadius = radius * 0.3f; // 30% the size of the main object
                std::vector<float> vertices;
                int stacks = 8;
                int sectors = 8;
                
                // Generate the sphere vertices
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

                        // Triangle 1
                        vertices.insert(vertices.end(), {v1.x, v1.y, v1.z});
                        vertices.insert(vertices.end(), {v2.x, v2.y, v2.z});
                        vertices.insert(vertices.end(), {v3.x, v3.y, v3.z});
                        
                        // Triangle 2
                        vertices.insert(vertices.end(), {v2.x, v2.y, v2.z});
                        vertices.insert(vertices.end(), {v4.x, v4.y, v4.z});
                        vertices.insert(vertices.end(), {v3.x, v3.y, v3.z});
                    }   
                }
                
                newSphere.vertexCount = vertices.size();
                CreateVBOVAO(newSphere.VAO, newSphere.VBO, vertices.data(), vertices.size());
                
                // Add to trail
                trailSpheres.push_back(newSphere);
                
                // Keep trail at maximum length
                if (trailSpheres.size() > static_cast<size_t>(maxTrailLength)) {
                    // Clean up the oldest sphere
                    glDeleteVertexArrays(1, &trailSpheres[0].VAO);
                    glDeleteBuffers(1, &trailSpheres[0].VBO);
                    trailSpheres.erase(trailSpheres.begin());
                }
                
                // Remove trail size debug print
            }
        }
        
        // Method to draw the trail
        void DrawTrail(GLuint shaderProgram, GLint colorLoc) {
            if (!hasTrail || trailSpheres.empty()) return;
            
            // Use a bright, high-contrast color for the trail
            glUniform4f(colorLoc, 1.0f, 0.0f, 0.0f, 1.0f); // Bright red
            
            // Draw each sphere in the trail
            for (size_t i = 0; i < trailSpheres.size(); ++i) {
                // Fade the color based on age (older spheres are more transparent)
                float alpha = (float)(i + 1) / trailSpheres.size(); // 0.0 to 1.0
                glUniform4f(colorLoc, 1.0f, 0.0f, 0.0f, alpha);
                
                // Position the sphere
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, trailSpheres[i].position);
                GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
                
                // Draw the sphere
                glBindVertexArray(trailSpheres[i].VAO);
                glDrawArrays(GL_TRIANGLES, 0, trailSpheres[i].vertexCount / 3);
            }
            
            glBindVertexArray(0);
        }
};
std::vector<Object> objs = {};

std::vector<float> CreateGridVertices(float size, int divisions, const std::vector<Object>& objs);

GLuint gridVAO, gridVBO; // 100x100 grid with 10 divisions


int main() {
    GLFWwindow* window = StartGLU();
    GLuint shaderProgram = CreateShaderProgram(vertexShaderSource, fragmentShaderSource);

    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint objectColorLoc = glGetUniformLocation(shaderProgram, "objectColor");
    glUseProgram(shaderProgram);

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    //projection matrix
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 750000.0f);
    GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
    cameraPos = glm::vec3(0.0f, 1000.0f,  5000.0f);

    
    objs = {
        Object(glm::vec3(3844, 0, 0), glm::vec3(0, 0, 228), 7.34767309*pow(10, 22), 3344),
        // Object(glm::vec3(-250, 0, 0), glm::vec3(0, -50, 0), 7.34767309*pow(10, 22), 3344),
        Object(glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), 5.97219*pow(10, 24), 5515),

    };
    
    // Set moon to grey color
    objs[0].color = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    // Enable trail for the moon
    objs[0].hasTrail = true;
    // Set earth to blue color
    objs[1].color = glm::vec4(0.0f, 0.3f, 0.8f, 1.0f);
    
    // Print simulation speed control instructions
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
    
    std::vector<float> gridVertices = CreateGridVertices(100000.0f, 50, objs);
    CreateVBOVAO(gridVAO, gridVBO, gridVertices.data(), gridVertices.size());
    std::cout<<"Earth radius: "<<objs[1].radius<<std::endl;
    std::cout<<"Moon radius: "<<objs[0].radius<<std::endl;

    while (!glfwWindowShouldClose(window) && running == true) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glfwSetKeyCallback(window, keyCallback);
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        
        // Handle continuous camera movement in main loop
        float speedMultiplier = (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) ? 5.0f : 1.0f;
        float cameraSpeed = 1000.0f * deltaTime * speedMultiplier;
        
        if (glfwGetKey(window, GLFW_KEY_W)==GLFW_PRESS){
            cameraPos += cameraSpeed * cameraFront;
        }
        if (glfwGetKey(window, GLFW_KEY_S)==GLFW_PRESS){
            cameraPos -= cameraSpeed * cameraFront;
        }
        if (glfwGetKey(window, GLFW_KEY_A)==GLFW_PRESS){
            cameraPos -= cameraSpeed * glm::normalize(glm::cross(cameraFront, cameraUp));
        }
        if (glfwGetKey(window, GLFW_KEY_D)==GLFW_PRESS){
            cameraPos += cameraSpeed * glm::normalize(glm::cross(cameraFront, cameraUp));
        }
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS){
            cameraPos += cameraSpeed * cameraUp;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS){
            cameraPos -= cameraSpeed * cameraUp;
        }
        if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS){
            pause = true;
        }
        if (glfwGetKey(window, GLFW_KEY_K) == GLFW_RELEASE){
            pause = false;
        }
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS){
            glfwTerminate();
            glfwWindowShouldClose(window);
            running = false;
        }
        
        UpdateCam(shaderProgram, cameraPos);
        if (!objs.empty() && objs.back().Initalizing) {
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
                // Increase mass by 1% per second
                objs.back().mass *= 1.0 + 1.0 * deltaTime;
                
                // Update radius based on new mass
                objs.back().radius = pow(
                    (3 * objs.back().mass / objs.back().density) / 
                    (4 * 3.14159265359f), 
                    1.0f/3.0f
                ) / 100000.0f;
                
                // Update vertex data
                objs.back().UpdateVertices();
            }
        }

        // Draw the grid
        glUseProgram(shaderProgram);
        glUniform4f(objectColorLoc, 1.0f, 1.0f, 1.0f, 0.25f); // White color with 50% transparency for the grid
        gridVertices = CreateGridVertices(10000.0f, 50, objs);
        glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
        glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_DYNAMIC_DRAW);
        DrawGrid(shaderProgram, gridVAO, gridVertices.size());

        // Draw the triangle
        for(auto& obj : objs) {
            glUniform4f(objectColorLoc, obj.color.r, obj.color.g, obj.color.b, obj.color.a);

            for(auto& obj2 : objs){
                if(&obj2 != &obj && !obj.Initalizing && !obj2.Initalizing){
                    float dx = obj2.GetPos()[0] - obj.GetPos()[0];
                    float dy = obj2.GetPos()[1] - obj.GetPos()[1];
                    float dz = obj2.GetPos()[2] - obj.GetPos()[2];
                    float distance = sqrt(dx * dx + dy * dy + dz * dz);

                    if (distance > 0) {
                        std::vector<float> direction = {dx / distance, dy / distance, dz / distance};
                        distance *= 1000;
                        double Gforce = (G * obj.mass * obj2.mass) / (distance * distance);
                        

                        float acc1 = Gforce / obj.mass;
                        std::vector<float> acc = {direction[0] * acc1, direction[1]*acc1, direction[2]*acc1};
                        if(!pause){
                            obj.accelerate(acc[0], acc[1], acc[2]);
                        }

                        //collision
                        obj.velocity *= obj.CheckCollision(obj2);
                    }
                }
            }
            if(obj.Initalizing){
                obj.radius = pow(((3 * obj.mass/obj.density)/(4 * 3.14159265359)), (1.0f/3.0f)) / 100000;
                obj.UpdateVertices();
            }

            //update positions
            if(!pause){
                obj.UpdatePos();
            }
            
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, obj.position); // Apply position here
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            
            // Draw the object
            glBindVertexArray(obj.VAO);
            glDrawArrays(GL_TRIANGLES, 0, obj.vertexCount / 3);
            
            // Draw the trail if this object has one
            if (obj.hasTrail) {
                // Reset model matrix for trail (don't want to translate the trail vertices)
                glm::mat4 identityModel = glm::mat4(1.0f);
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(identityModel));
                obj.DrawTrail(shaderProgram, objectColorLoc);
            }
        }
        
        // Just swap buffers and poll events without rendering text
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    for (auto& obj : objs) {
        glDeleteVertexArrays(1, &obj.VAO);
        glDeleteBuffers(1, &obj.VBO);
        // Clean up trail buffers
        if (obj.hasTrail) {
            for (auto& sphere : obj.trailSpheres) {
                glDeleteVertexArrays(1, &sphere.VAO);
                glDeleteBuffers(1, &sphere.VBO);
            }
        }
    }

    glDeleteVertexArrays(1, &gridVAO);
    glDeleteBuffers(1, &gridVBO);

    glDeleteProgram(shaderProgram);
    glfwTerminate();

    glfwTerminate();
    return 0;
}

GLFWwindow* StartGLU() {
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW, panic" << std::endl;
        return nullptr;
    }
    
    // Set OpenGL version hints for macOS compatibility
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);  // Required on macOS
    
    GLFWwindow* window = glfwCreateWindow(800, 600, "Gravity Simulator 3D Grid", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return nullptr;
    }
    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW." << std::endl;
        glfwTerminate();
        return nullptr;
    }

    glEnable(GL_DEPTH_TEST);
    
    // Get actual framebuffer size (important for Retina displays)
    int framebufferWidth, framebufferHeight;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Standard blending for transparency

    return window;
}

GLuint CreateShaderProgram(const char* vertexSource, const char* fragmentSource) {
    // Vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, nullptr);
    glCompileShader(vertexShader);

    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "Vertex shader compilation failed: " << infoLog << std::endl;
    }

    // Fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Fragment shader compilation failed: " << infoLog << std::endl;
    }

    // Shader program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed: " << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}
void CreateVBOVAO(GLuint& VAO, GLuint& VBO, const float* vertices, size_t vertexCount) {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertexCount * sizeof(float), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void UpdateCam(GLuint shaderProgram, glm::vec3 cameraPos) {
    glUseProgram(shaderProgram);
    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)window;
    (void)scancode;
    bool shiftPressed = (mods & GLFW_MOD_SHIFT) != 0;
    
    // Simulation speed control with number keys (when pressed, not held)
    if (action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_0: // Reset to normal speed
                simulationSpeed = 1.0f;
                std::cout << "Simulation speed: 1.0x (normal)" << std::endl;
                break;
            case GLFW_KEY_1: // 0.5x speed
                simulationSpeed = 0.5f;
                std::cout << "Simulation speed: 0.5x (slow)" << std::endl;
                break;
            case GLFW_KEY_2: // 2x speed
                simulationSpeed = 2.0f;
                std::cout << "Simulation speed: 2.0x" << std::endl;
                break;
            case GLFW_KEY_3: // 5x speed
                simulationSpeed = 5.0f;
                std::cout << "Simulation speed: 5.0x" << std::endl;
                break;
            case GLFW_KEY_4: // 10x speed
                simulationSpeed = 10.0f;
                std::cout << "Simulation speed: 10.0x (fast)" << std::endl;
                break;
        }
    }

    // init arrows pos up down left right
    if(!objs.empty() && objs[objs.size() - 1].Initalizing){
        if (key == GLFW_KEY_UP && (action == GLFW_PRESS || action == GLFW_REPEAT)){
            if (!shiftPressed) {
                objs[objs.size()-1].position[1] += 0.5;
            }
        };
        if (key == GLFW_KEY_DOWN && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            if (!shiftPressed) {
                objs[objs.size()-1].position[1] -= 0.5;
            }
        }
        if(key == GLFW_KEY_RIGHT && (action == GLFW_PRESS || action == GLFW_REPEAT)){
            objs[objs.size()-1].position[0] += 0.5;
        };
        if(key == GLFW_KEY_LEFT && (action == GLFW_PRESS || action == GLFW_REPEAT)){
            objs[objs.size()-1].position[0] -= 0.5;
        };
        if (key == GLFW_KEY_UP && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            objs[objs.size()-1].position[2] += 0.5;
        };

        if (key == GLFW_KEY_DOWN && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            objs[objs.size()-1].position[2] -= 0.5;
        }
    };
    
};
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; 
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if(pitch > 89.0f) pitch = 89.0f;
    if(pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods){
    (void)window;
    (void)mods;
    if (button == GLFW_MOUSE_BUTTON_LEFT){
        if (action == GLFW_PRESS){
            objs.emplace_back(glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0f, 0.0f, 0.0f), initMass);
            objs[objs.size()-1].Initalizing = true;
        };
        if (action == GLFW_RELEASE){
            objs[objs.size()-1].Initalizing = false;
            objs[objs.size()-1].Launched = true;
        };
    };
    // if (!objs.empty() && button == GLFW_MOUSE_BUTTON_RIGHT && objs[objs.size()-1].Initalizing) {
    //     if (action == GLFW_PRESS || action == GLFW_REPEAT) {
    //         objs[objs.size()-1].mass *= 1.2;}
    //         std::cout<<"MASS: "<<objs[objs.size()-1].mass<<std::endl;
    // }
};
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset){
    (void)window;
    (void)xoffset;
    float cameraSpeed = 50000.0f * deltaTime;
    if(yoffset>0){
        cameraPos += cameraSpeed * cameraFront;
    } else if(yoffset<0){
        cameraPos -= cameraSpeed * cameraFront;
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height){
    (void)window;
    glViewport(0, 0, width, height);
}

glm::vec3 sphericalToCartesian(float r, float theta, float phi){
    float x = r * sin(theta) * cos(phi);
    float y = r * cos(theta);
    float z = r * sin(theta) * sin(phi);
    return glm::vec3(x, y, z);
};
void DrawGrid(GLuint shaderProgram, GLuint gridVAO, size_t vertexCount) {
    glUseProgram(shaderProgram);
    glm::mat4 model = glm::mat4(1.0f); // Identity matrix for the grid
    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    glBindVertexArray(gridVAO);
    glPointSize(5.0f);
    glDrawArrays(GL_LINES, 0, vertexCount / 3);
    glBindVertexArray(0);
}
std::vector<float> CreateGridVertices(float size, int divisions, const std::vector<Object>& objs) {
    std::vector<float> vertices;
    float step = size / divisions;
    float halfSize = size / 2.0f;

    // x axis
    for (int yStep = 3; yStep <= 3; ++yStep) {
        float y = -halfSize*0.3f + yStep * step;
        for (int zStep = 0; zStep <= divisions; ++zStep) {
            float z = -halfSize + zStep * step;
            for (int xStep = 0; xStep < divisions; ++xStep) {
                float xStart = -halfSize + xStep * step;
                float xEnd = xStart + step;
                vertices.push_back(xStart); vertices.push_back(y); vertices.push_back(z);
                vertices.push_back(xEnd);   vertices.push_back(y); vertices.push_back(z);
            }
        }
    }

    // // yzxis
    // for (int xStep = 0; xStep <= divisions; ++xStep) {
    //     float x = -halfSize + xStep * step;
    //     for (int zStep = 0; zStep <= divisions; ++zStep) {
    //         float z = -halfSize + zStep * step;s
    //         for (int yStep = 0; yStep < divisions; ++yStep) {
    //             float yStart = -halfSize + yStep * step;
    //             float yEnd = yStart + step;
    //             vertices.push_back(x); vertices.push_back(yStart); vertices.push_back(z);
    //             vertices.push_back(x); vertices.push_back(yEnd);   vertices.push_back(z);
    //         }
    //     }
    // }

    // zaxis
    for (int xStep = 0; xStep <= divisions; ++xStep) {
        float x = -halfSize + xStep * step;
        for (int yStep = 3; yStep <= 3; ++yStep) {
            float y = -halfSize*0.3f + yStep * step;
            for (int zStep = 0; zStep < divisions; ++zStep) {
                float zStart = -halfSize + zStep * step;
                float zEnd = zStart + step;
                vertices.push_back(x); vertices.push_back(y); vertices.push_back(zStart);
                vertices.push_back(x); vertices.push_back(y); vertices.push_back(zEnd);
            }
        }
    }
    

    // displacement
    // for (int i = 0; i < vertices.size(); i += 3) {
    //     glm::vec3 vertexPos(vertices[i], vertices[i+1], vertices[i+2]);
    //     glm::vec3 totalDisplacement(0.0f);

    //     for (const auto& obj : objs) {
    //         glm::vec3 toObject = obj.GetPos() - vertexPos;
    //         float distance = glm::length(toObject);

    //         float distance_m = distance * 1000.0f;
            
    //         float strength = (G * obj.mass) / (distance_m * distance_m);
    //         glm::vec3 displacement = glm::normalize(toObject) * strength;

    //         totalDisplacement += -displacement * (2/distance);
    //     }

    //     vertexPos += totalDisplacement; 

    //     // Update vertex data
    //     vertices[i]   = vertexPos[0];
    //     vertices[i+1] = vertexPos[1];
    //     vertices[i+2] = vertexPos[2];
    // }
    for (size_t i = 0; i < vertices.size(); i += 3) {
        glm::vec3 vertexPos(vertices[i], vertices[i+1], vertices[i+2]);
        glm::vec3 totalDisplacement(0.0f);
        

        for (const auto& obj : objs) {
            glm::vec3 toObject = obj.GetPos() - vertexPos;
            float distance = glm::length(toObject);

            float distance_m = distance * 1000.0f;
            float rs = (2*G*obj.mass)/(c*c);

            float z = 2 * sqrt(rs*(distance_m - rs)) * 100.0f;
            totalDisplacement += z;
            

        }
        
        vertexPos += totalDisplacement; 

         vertices[i+1] = vertexPos[1] / 15.0f - 3000.0f;
    }
    

    return vertices;
}

// Function to render text for displaying simulation speed
void renderText(const std::string& text, float x, float y, float scale, GLuint shaderProgram, GLint colorLoc) {
    // Save current shader settings
    GLint currentModelLoc = glGetUniformLocation(shaderProgram, "model");
    
    // Set color for text (white)
    glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
    
    // Setup orthographic projection for 2D rendering
    glm::mat4 projection = glm::ortho(0.0f, 800.0f, 0.0f, 600.0f, -1.0f, 1.0f);
    GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
    
    // Setup view matrix (identity for 2D)
    glm::mat4 view = glm::mat4(1.0f);
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    
    // Create simple text vertices (just a small square for each character)
    float characterSize = 10.0f * scale;
    float spacing = characterSize * 0.5f;
    
    // Render each character as a small square
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
    
    // Restore 3D projection
    projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 750000.0f);
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
}









//check up




