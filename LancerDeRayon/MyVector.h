#pragma once
#include <vector>
#include <iostream>

class Vector {
private:
    std::vector<float> values;
public:
    Vector();
    Vector(const std::vector<float>& vals);
    std::vector<float> getValues() const;
    void setValues(const std::vector<float>& vals);

    Vector operator+(const Vector& other) const;
    Vector operator-(const Vector& other) const;
    Vector operator*(float scalar) const;
    Vector operator*(const Vector& other) const;
	float dot(const Vector& other) const;
	float length() const;

    static Vector translate(const Vector& v1, const Vector& v2);
	Vector normalize() const;

    friend std::ostream& operator<<(std::ostream& os, const Vector& v);
};