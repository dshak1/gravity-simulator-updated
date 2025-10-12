#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>
#include <iostream>
#include <stdexcept>

// Modern C++ RAII Resource Management for OpenGL Objects
// Demonstrates advanced C++ features for EA SPORTS Software Engineer Co-op

namespace EASports {
namespace Graphics {

// RAII wrapper for OpenGL VAO (Vertex Array Object)
class VAOWrapper {
private:
    GLuint m_id;
    bool m_valid;
    
public:
    VAOWrapper() : m_id(0), m_valid(false) {
        glGenVertexArrays(1, &m_id);
        m_valid = (m_id != 0);
        if (!m_valid) {
            throw std::runtime_error("Failed to generate VAO");
        }
    }
    
    ~VAOWrapper() {
        if (m_valid && m_id != 0) {
            glDeleteVertexArrays(1, &m_id);
        }
    }
    
    // Disable copy constructor and assignment operator
    VAOWrapper(const VAOWrapper&) = delete;
    VAOWrapper& operator=(const VAOWrapper&) = delete;
    
    // Allow move constructor and assignment operator
    VAOWrapper(VAOWrapper&& other) noexcept 
        : m_id(other.m_id), m_valid(other.m_valid) {
        other.m_id = 0;
        other.m_valid = false;
    }
    
    VAOWrapper& operator=(VAOWrapper&& other) noexcept {
        if (this != &other) {
            if (m_valid && m_id != 0) {
                glDeleteVertexArrays(1, &m_id);
            }
            
            m_id = other.m_id;
            m_valid = other.m_valid;
            
            other.m_id = 0;
            other.m_valid = false;
        }
        return *this;
    }
    
    GLuint getId() const { return m_id; }
    bool isValid() const { return m_valid; }
    
    void bind() const {
        if (m_valid) {
            glBindVertexArray(m_id);
        }
    }
    
    static void unbind() {
        glBindVertexArray(0);
    }
};

// RAII wrapper for OpenGL VBO (Vertex Buffer Object)
class VBOWrapper {
private:
    GLuint m_id;
    bool m_valid;
    
public:
    VBOWrapper() : m_id(0), m_valid(false) {
        glGenBuffers(1, &m_id);
        m_valid = (m_id != 0);
        if (!m_valid) {
            throw std::runtime_error("Failed to generate VBO");
        }
    }
    
    ~VBOWrapper() {
        if (m_valid && m_id != 0) {
            glDeleteBuffers(1, &m_id);
        }
    }
    
    // Disable copy constructor and assignment operator
    VBOWrapper(const VBOWrapper&) = delete;
    VBOWrapper& operator=(const VBOWrapper&) = delete;
    
    // Allow move constructor and assignment operator
    VBOWrapper(VBOWrapper&& other) noexcept 
        : m_id(other.m_id), m_valid(other.m_valid) {
        other.m_id = 0;
        other.m_valid = false;
    }
    
    VBOWrapper& operator=(VBOWrapper&& other) noexcept {
        if (this != &other) {
            if (m_valid && m_id != 0) {
                glDeleteBuffers(1, &m_id);
            }
            
            m_id = other.m_id;
            m_valid = other.m_valid;
            
            other.m_id = 0;
            other.m_valid = false;
        }
        return *this;
    }
    
    GLuint getId() const { return m_id; }
    bool isValid() const { return m_valid; }
    
    void bind(GLenum target = GL_ARRAY_BUFFER) const {
        if (m_valid) {
            glBindBuffer(target, m_id);
        }
    }
    
    void bufferData(const void* data, size_t size, GLenum usage = GL_STATIC_DRAW, GLenum target = GL_ARRAY_BUFFER) const {
        if (m_valid) {
            glBindBuffer(target, m_id);
            glBufferData(target, size, data, usage);
        }
    }
    
    static void unbind(GLenum target = GL_ARRAY_BUFFER) {
        glBindBuffer(target, 0);
    }
};

// RAII wrapper for OpenGL Shader Program
class ShaderProgramWrapper {
private:
    GLuint m_id;
    bool m_valid;
    
public:
    ShaderProgramWrapper() : m_id(0), m_valid(false) {
        m_id = glCreateProgram();
        m_valid = (m_id != 0);
        if (!m_valid) {
            throw std::runtime_error("Failed to create shader program");
        }
    }
    
    ~ShaderProgramWrapper() {
        if (m_valid && m_id != 0) {
            glDeleteProgram(m_id);
        }
    }
    
    // Disable copy constructor and assignment operator
    ShaderProgramWrapper(const ShaderProgramWrapper&) = delete;
    ShaderProgramWrapper& operator=(const ShaderProgramWrapper&) = delete;
    
    // Allow move constructor and assignment operator
    ShaderProgramWrapper(ShaderProgramWrapper&& other) noexcept 
        : m_id(other.m_id), m_valid(other.m_valid) {
        other.m_id = 0;
        other.m_valid = false;
    }
    
    ShaderProgramWrapper& operator=(ShaderProgramWrapper&& other) noexcept {
        if (this != &other) {
            if (m_valid && m_id != 0) {
                glDeleteProgram(m_id);
            }
            
            m_id = other.m_id;
            m_valid = other.m_valid;
            
            other.m_id = 0;
            other.m_valid = false;
        }
        return *this;
    }
    
    GLuint getId() const { return m_id; }
    bool isValid() const { return m_valid; }
    
    void use() const {
        if (m_valid) {
            glUseProgram(m_id);
        }
    }
    
    static void unuse() {
        glUseProgram(0);
    }
    
    GLint getUniformLocation(const std::string& name) const {
        if (m_valid) {
            return glGetUniformLocation(m_id, name.c_str());
        }
        return -1;
    }
    
    GLint getAttribLocation(const std::string& name) const {
        if (m_valid) {
            return glGetAttribLocation(m_id, name.c_str());
        }
        return -1;
    }
    
    void setUniform(const std::string& name, const glm::mat4& matrix) const {
        GLint location = getUniformLocation(name);
        if (location != -1) {
            glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
        }
    }
    
    void setUniform(const std::string& name, const glm::vec4& vector) const {
        GLint location = getUniformLocation(name);
        if (location != -1) {
            glUniform4fv(location, 1, glm::value_ptr(vector));
        }
    }
    
    void setUniform(const std::string& name, const glm::vec3& vector) const {
        GLint location = getUniformLocation(name);
        if (location != -1) {
            glUniform3fv(location, 1, glm::value_ptr(vector));
        }
    }
    
    void setUniform(const std::string& name, float value) const {
        GLint location = getUniformLocation(name);
        if (location != -1) {
            glUniform1f(location, value);
        }
    }
    
    void setUniform(const std::string& name, int value) const {
        GLint location = getUniformLocation(name);
        if (location != -1) {
            glUniform1i(location, value);
        }
    }
    
    void setUniform(const std::string& name, bool value) const {
        setUniform(name, value ? 1 : 0);
    }
};

// Modern C++ Resource Manager for OpenGL Objects
class OpenGLResourceManager {
private:
    std::unordered_map<std::string, std::unique_ptr<VAOWrapper>> m_vaos;
    std::unordered_map<std::string, std::unique_ptr<VBOWrapper>> m_vbos;
    std::unordered_map<std::string, std::unique_ptr<ShaderProgramWrapper>> m_shaders;
    
public:
    // VAO Management
    VAOWrapper& createVAO(const std::string& name) {
        auto vao = std::make_unique<VAOWrapper>();
        VAOWrapper& ref = *vao;
        m_vaos[name] = std::move(vao);
        return ref;
    }
    
    VAOWrapper* getVAO(const std::string& name) const {
        auto it = m_vaos.find(name);
        return (it != m_vaos.end()) ? it->second.get() : nullptr;
    }
    
    // VBO Management
    VBOWrapper& createVBO(const std::string& name) {
        auto vbo = std::make_unique<VBOWrapper>();
        VBOWrapper& ref = *vbo;
        m_vbos[name] = std::move(vbo);
        return ref;
    }
    
    VBOWrapper* getVBO(const std::string& name) const {
        auto it = m_vbos.find(name);
        return (it != m_vbos.end()) ? it->second.get() : nullptr;
    }
    
    // Shader Program Management
    ShaderProgramWrapper& createShaderProgram(const std::string& name) {
        auto shader = std::make_unique<ShaderProgramWrapper>();
        ShaderProgramWrapper& ref = *shader;
        m_shaders[name] = std::move(shader);
        return ref;
    }
    
    ShaderProgramWrapper* getShaderProgram(const std::string& name) const {
        auto it = m_shaders.find(name);
        return (it != m_shaders.end()) ? it->second.get() : nullptr;
    }
    
    // Resource cleanup
    void clear() {
        m_vaos.clear();
        m_vbos.clear();
        m_shaders.clear();
    }
    
    // Statistics
    size_t getVAOCount() const { return m_vaos.size(); }
    size_t getVBOCount() const { return m_vbos.size(); }
    size_t getShaderCount() const { return m_shaders.size(); }
    
    void printStatistics() const {
        std::cout << "=== OpenGL Resource Manager Statistics ===" << std::endl;
        std::cout << "VAOs: " << getVAOCount() << std::endl;
        std::cout << "VBOs: " << getVBOCount() << std::endl;
        std::cout << "Shaders: " << getShaderCount() << std::endl;
        std::cout << "==========================================" << std::endl;
    }
};

// Singleton instance for global resource management
class OpenGLManager {
private:
    static std::unique_ptr<OpenGLManager> s_instance;
    OpenGLResourceManager m_resourceManager;
    
    OpenGLManager() = default;
    
public:
    static OpenGLManager& getInstance() {
        if (!s_instance) {
            s_instance = std::make_unique<OpenGLManager>();
        }
        return *s_instance;
    }
    
    OpenGLResourceManager& getResourceManager() { return m_resourceManager; }
    
    // Utility functions
    static bool checkGLError(const std::string& operation) {
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cerr << "OpenGL Error in " << operation << ": " << error << std::endl;
            return false;
        }
        return true;
    }
    
    static void printOpenGLInfo() {
        std::cout << "=== OpenGL Information ===" << std::endl;
        std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
        std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
        std::cout << "Vendor: " << glGetString(GL_VENDOR) << std::endl;
        std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;
        std::cout << "=========================" << std::endl;
    }
};

} // namespace Graphics
} // namespace EASports

