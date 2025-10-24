#include "MyVector.h"


Vector::Vector() : values() {}

Vector::Vector(const std::vector<float>& vals) : values(vals) {}

std::vector<float> Vector::getValues() const {
    return values;
}

void Vector::setValues(const std::vector<float>& vals) {
    values = vals;
}

Vector Vector::operator+(const Vector& other) const {
    if (values.size() != other.values.size())
        throw std::runtime_error("Vector sizes must match");

    std::vector<float> result(values.size());
    for (size_t i = 0; i < values.size(); ++i)
        result[i] = values[i] + other.values[i];

    return Vector(result);
}

Vector Vector::operator-(const Vector& other) const {
    if (values.size() != other.values.size())
        throw std::runtime_error("Vector sizes must match");

    std::vector<float> result(values.size());
    for (size_t i = 0; i < values.size(); ++i)
        result[i] = values[i] - other.values[i];

    return Vector(result);
}

Vector Vector::operator*(float scalar) const {
    std::vector<float> result(values.size());
    for (size_t i = 0; i < values.size(); ++i)
        result[i] = values[i] * scalar;

    return Vector(result);
}

Vector Vector::operator*(const Vector& other) const {
    if (values.size() != other.values.size())
        throw std::runtime_error("Vector sizes must match");

    std::vector<float> result(values.size());
    for (size_t i = 0; i < values.size(); ++i)
        result[i] = values[i] * other.values[i];

    return Vector(result);
}

float Vector::dot(const Vector& other) const {
    if (values.size() != other.values.size())
        throw std::runtime_error("Vector sizes must match");
    float result = 0.0;
    for (size_t i = 0; i < values.size(); ++i)
        result += values[i] * other.values[i];
    return result;
}

float Vector::length() const {
	return std::sqrt(this->dot(*this));
}

Vector Vector::translate(const Vector& v1, const Vector& v2) {
    return v1 + v2;
}

std::ostream& operator<<(std::ostream& os, const Vector& v) {
    os << "[";
    for (size_t i = 0; i < v.values.size(); ++i) {
        os << v.values[i];
        if (i != v.values.size() - 1)
            os << ", ";
    }
    os << "]";
    return os;
}

Vector Vector::normalize() const {
    float len = length();
    if (len == 0.0f)
        throw std::runtime_error("Cannot normalize a zero-length vector.");
    return (*this) * (1.0f / len);
}

