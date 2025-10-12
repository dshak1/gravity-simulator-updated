#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <functional>
#include <algorithm>
#include <numeric>
#include <cmath> 
#include <string>
#include <unordered_map>
#include <chrono>
#include <iostream>

// Modern C++ Physics Engine with Template-Based Calculations
// Demonstrates advanced C++ features for EA SPORTS Software Engineer Co-op

namespace EASports {
namespace Physics {

// Template-based vector operations for different precision types
template<typename T>
class Vector3D {
private:
    T m_x, m_y, m_z;
    
public:
    Vector3D(T x = T{}, T y = T{}, T z = T{}) : m_x(x), m_y(y), m_z(z) {}
    
    // Getters
    T x() const { return m_x; }
    T y() const { return m_y; }
    T z() const { return m_z; }
    
    // Setters
    void setX(T x) { m_x = x; }
    void setY(T y) { m_y = y; }
    void setZ(T z) { m_z = z; }
    
    // Vector operations
    Vector3D operator+(const Vector3D& other) const {
        return Vector3D(m_x + other.m_x, m_y + other.m_y, m_z + other.m_z);
    }
    
    Vector3D operator-(const Vector3D& other) const {
        return Vector3D(m_x - other.m_x, m_y - other.m_y, m_z - other.m_z);
    }
    
    Vector3D operator*(T scalar) const {
        return Vector3D(m_x * scalar, m_y * scalar, m_z * scalar);
    }
    
    Vector3D& operator+=(const Vector3D& other) {
        m_x += other.m_x;
        m_y += other.m_y;
        m_z += other.m_z;
        return *this;
    }
    
    Vector3D& operator-=(const Vector3D& other) {
        m_x -= other.m_x;
        m_y -= other.m_y;
        m_z -= other.m_z;
        return *this;
    }
    
    T magnitude() const {
        return std::sqrt(m_x * m_x + m_y * m_y + m_z * m_z);
    }
    
    Vector3D normalized() const {
        T mag = magnitude();
        if (mag > 0) {
            return Vector3D(m_x / mag, m_y / mag, m_z / mag);
        }
        return Vector3D{};
    }
    
    T dot(const Vector3D& other) const {
        return m_x * other.m_x + m_y * other.m_y + m_z * other.m_z;
    }
    
    Vector3D cross(const Vector3D& other) const {
        return Vector3D(
            m_y * other.m_z - m_z * other.m_y,
            m_z * other.m_x - m_x * other.m_z,
            m_x * other.m_y - m_y * other.m_x
        );
    }
    
    // Conversion to glm::vec3 for OpenGL compatibility
    glm::vec3 toGlmVec3() const {
        return glm::vec3(static_cast<float>(m_x), static_cast<float>(m_y), static_cast<float>(m_z));
    }
    
    // String representation for debugging
    std::string toString() const {
        return "(" + std::to_string(m_x) + ", " + std::to_string(m_y) + ", " + std::to_string(m_z) + ")";
    }
};

// Modern C++ Physics Object with RAII and encapsulation
template<typename T>
class PhysicsObject {
private:
    Vector3D<T> m_position;
    Vector3D<T> m_velocity;
    Vector3D<T> m_acceleration;
    T m_mass;
    T m_radius;
    std::string m_id;
    bool m_active;
    
public:
    PhysicsObject(const Vector3D<T>& position = Vector3D<T>{}, 
                  const Vector3D<T>& velocity = Vector3D<T>{}, 
                  T mass = T{1.0}, 
                  T radius = T{1.0},
                  const std::string& id = "")
        : m_position(position), m_velocity(velocity), m_acceleration{}, 
          m_mass(mass), m_radius(radius), m_id(id), m_active(true) {}
    
    // Getters
    const Vector3D<T>& getPosition() const { return m_position; }
    const Vector3D<T>& getVelocity() const { return m_velocity; }
    const Vector3D<T>& getAcceleration() const { return m_acceleration; }
    T getMass() const { return m_mass; }
    T getRadius() const { return m_radius; }
    const std::string& getId() const { return m_id; }
    bool isActive() const { return m_active; }
    
    // Setters
    void setPosition(const Vector3D<T>& position) { m_position = position; }
    void setVelocity(const Vector3D<T>& velocity) { m_velocity = velocity; }
    void setAcceleration(const Vector3D<T>& acceleration) { m_acceleration = acceleration; }
    void setMass(T mass) { m_mass = mass; }
    void setRadius(T radius) { m_radius = radius; }
    void setActive(bool active) { m_active = active; }
    
    // Physics calculations
    void applyForce(const Vector3D<T>& force) {
        if (m_mass > 0) {
            m_acceleration = force * (T{1.0} / m_mass);
        }
    }
    
    void updatePhysics(T deltaTime) {
        if (!m_active) return;
        
        // Update velocity using acceleration
        m_velocity += m_acceleration * deltaTime;
        
        // Update position using velocity
        m_position += m_velocity * deltaTime;
        
        // Reset acceleration for next frame
        m_acceleration = Vector3D<T>{};
    }
    
    T distanceTo(const PhysicsObject& other) const {
        return (m_position - other.m_position).magnitude();
    }
    
    Vector3D<T> calculateGravitationalForce(const PhysicsObject& other, T gravitationalConstant = T{6.674e-11}) const {
        Vector3D<T> direction = other.m_position - m_position;
        T distance = direction.magnitude();
        
        if (distance < m_radius + other.m_radius) {
            return Vector3D<T>{}; // Avoid division by zero
        }
        
        T forceMagnitude = gravitationalConstant * m_mass * other.m_mass / (distance * distance);
        return direction.normalized() * forceMagnitude;
    }
    
    T getKineticEnergy() const {
        T speed = m_velocity.magnitude();
        return T{0.5} * m_mass * speed * speed;
    }
    
    T getPotentialEnergy(const std::vector<PhysicsObject>& objects, T gravitationalConstant = T{6.674e-11}) const {
        T potentialEnergy = T{0};
        for (const auto& obj : objects) {
            if (obj.getId() != m_id) {
                T distance = distanceTo(obj);
                if (distance > m_radius + obj.m_radius) {
                    potentialEnergy -= gravitationalConstant * m_mass * obj.m_mass / distance;
                }
            }
        }
        return potentialEnergy;
    }
};

// Modern C++ Physics Engine with template-based calculations
template<typename T>
class PhysicsEngine {
private:
    std::vector<std::unique_ptr<PhysicsObject<T>>> m_objects;
    T m_gravitationalConstant;
    T m_timeScale;
    bool m_paused;
    std::string m_name;
    
    // Performance metrics
    std::chrono::high_resolution_clock::time_point m_lastUpdateTime;
    T m_averageUpdateTime;
    size_t m_updateCount;
    
public:
    PhysicsEngine(const std::string& name = "PhysicsEngine", T gravitationalConstant = T{6.674e-11})
        : m_gravitationalConstant(gravitationalConstant), m_timeScale(T{1.0}), m_paused(false),
          m_name(name), m_averageUpdateTime(T{0}), m_updateCount(0) {
        m_lastUpdateTime = std::chrono::high_resolution_clock::now();
    }
    
    // Object management
    PhysicsObject<T>& addObject(std::unique_ptr<PhysicsObject<T>> object) {
        PhysicsObject<T>& ref = *object;
        m_objects.push_back(std::move(object));
        return ref;
    }
    
    PhysicsObject<T>* getObject(const std::string& id) const {
        auto it = std::find_if(m_objects.begin(), m_objects.end(),
            [&id](const std::unique_ptr<PhysicsObject<T>>& obj) {
                return obj->getId() == id;
            });
        return (it != m_objects.end()) ? it->get() : nullptr;
    }
    
    void removeObject(const std::string& id) {
        m_objects.erase(
            std::remove_if(m_objects.begin(), m_objects.end(),
                [&id](const std::unique_ptr<PhysicsObject<T>>& obj) {
                    return obj->getId() == id;
                }),
            m_objects.end()
        );
    }
    
    void clearObjects() {
        m_objects.clear();
    }
    
    // Physics simulation
    void updatePhysics(T deltaTime) {
        if (m_paused) return;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        T scaledDeltaTime = deltaTime * m_timeScale;
        
        // Apply gravitational forces between all objects
        for (size_t i = 0; i < m_objects.size(); i++) {
            for (size_t j = i + 1; j < m_objects.size(); j++) {
                auto& obj1 = m_objects[i];
                auto& obj2 = m_objects[j];
                
                if (obj1->isActive() && obj2->isActive()) {
                    Vector3D<T> force = obj1->calculateGravitationalForce(*obj2, m_gravitationalConstant);
                    
                    obj1->applyForce(force);
                    obj2->applyForce(force * T{-1.0});
                }
            }
        }
        
        // Update all objects
        for (auto& obj : m_objects) {
            obj->updatePhysics(scaledDeltaTime);
        }
        
        // Update performance metrics
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        T updateTime = static_cast<T>(duration.count()) / T{1000.0}; // Convert to milliseconds
        
        m_averageUpdateTime = (m_averageUpdateTime * m_updateCount + updateTime) / (m_updateCount + 1);
        m_updateCount++;
    }
    
    // Getters and setters
    size_t getObjectCount() const { return m_objects.size(); }
    T getGravitationalConstant() const { return m_gravitationalConstant; }
    void setGravitationalConstant(T value) { m_gravitationalConstant = value; }
    T getTimeScale() const { return m_timeScale; }
    void setTimeScale(T value) { m_timeScale = value; }
    bool isPaused() const { return m_paused; }
    void setPaused(bool paused) { m_paused = paused; }
    const std::string& getName() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }
    
    // Performance metrics
    T getAverageUpdateTime() const { return m_averageUpdateTime; }
    size_t getUpdateCount() const { return m_updateCount; }
    
    // Energy calculations
    T getTotalKineticEnergy() const {
        return std::accumulate(m_objects.begin(), m_objects.end(), T{0},
            [](T total, const std::unique_ptr<PhysicsObject<T>>& obj) {
                return total + obj->getKineticEnergy();
            });
    }
    
    T getTotalPotentialEnergy() const {
        T totalPotential = T{0};
        for (const auto& obj : m_objects) {
            totalPotential += obj->getPotentialEnergy(
                std::vector<PhysicsObject<T>>(m_objects.begin(), m_objects.end()),
                m_gravitationalConstant
            );
        }
        return totalPotential / T{2}; // Divide by 2 to avoid double counting
    }
    
    T getTotalEnergy() const {
        return getTotalKineticEnergy() + getTotalPotentialEnergy();
    }
    
    // Statistics
    void printStatistics() const {
        std::cout << "\n=== " << m_name << " Statistics ===" << std::endl;
        std::cout << "Objects: " << getObjectCount() << std::endl;
        std::cout << "Gravitational Constant: " << m_gravitationalConstant << std::endl;
        std::cout << "Time Scale: " << m_timeScale << std::endl;
        std::cout << "Paused: " << (m_paused ? "Yes" : "No") << std::endl;
        std::cout << "Average Update Time: " << m_averageUpdateTime << " ms" << std::endl;
        std::cout << "Total Kinetic Energy: " << getTotalKineticEnergy() << std::endl;
        std::cout << "Total Potential Energy: " << getTotalPotentialEnergy() << std::endl;
        std::cout << "Total Energy: " << getTotalEnergy() << std::endl;
        std::cout << "===============================" << std::endl;
    }
    
    // Iterator support for modern C++ algorithms
    auto begin() { return m_objects.begin(); }
    auto end() { return m_objects.end(); }
    auto begin() const { return m_objects.begin(); }
    auto end() const { return m_objects.end(); }
    
    // Range-based for loop support
    const std::vector<std::unique_ptr<PhysicsObject<T>>>& getObjects() const { return m_objects; }
};

// Type aliases for common precision types
using PhysicsEngineF = PhysicsEngine<float>;
using PhysicsEngineD = PhysicsEngine<double>;
using PhysicsObjectF = PhysicsObject<float>;
using PhysicsObjectD = PhysicsObject<double>;
using Vector3DF = Vector3D<float>;
using Vector3DD = Vector3D<double>;

} // namespace Physics
} // namespace EASports

