#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <iostream>

class Vector {
public:
    float x, y, z;

    Vector() noexcept : x(0.0f), y(0.0f), z(0.0f) {}
    Vector(float xx, float yy, float zz) noexcept : x(xx), y(yy), z(zz) {}
    Vector(const std::vector<float>& vals) noexcept {
        x = vals.size() > 0 ? vals[0] : 0.0f;
        y = vals.size() > 1 ? vals[1] : 0.0f;
        z = vals.size() > 2 ? vals[2] : 0.0f;
    }

    std::vector<float> getValues() const {
        return std::vector<float>{x, y, z};
    }

    void setValues(const std::vector<float>& vals) {
        x = vals.size() > 0 ? vals[0] : 0.0f;
        y = vals.size() > 1 ? vals[1] : 0.0f;
        z = vals.size() > 2 ? vals[2] : 0.0f;
    }

    // opérateurs inlines rapides
    Vector operator+(const Vector& o) const noexcept { return Vector(x + o.x, y + o.y, z + o.z); }
    Vector operator-(const Vector& o) const noexcept { return Vector(x - o.x, y - o.y, z - o.z); }
    Vector operator*(float s) const noexcept { return Vector(x * s, y * s, z * s); }
    Vector operator*(const Vector& o) const noexcept { return Vector(x * o.x, y * o.y, z * o.z); }

    float dot(const Vector& o) const noexcept { return x * o.x + y * o.y + z * o.z; }
    float length() const noexcept { return std::sqrt(dot(*this)); }

    static Vector translate(const Vector& v1, const Vector& v2) noexcept { return v1 + v2; }

    Vector normalize() const {
        float len = length();
        if (len == 0.0f) return Vector(0.0f, 0.0f, 0.0f);
        float inv = 1.0f / len;
        return Vector(x * inv, y * inv, z * inv);
    }

    friend std::ostream& operator<<(std::ostream& os, const Vector& v) {
        os << "[" << v.x << ", " << v.y << ", " << v.z << "]";
        return os;
    }
};