#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <algorithm>
#include "MyVector.h"
#include "Pixel.h"
#include "Rayon.h"

class Sphere
{
public:
    Sphere(const Vector& center, float radius);
    Vector center;
    float radius;
};

Sphere::Sphere(const Vector& center, float radius)
{
	this->center = center;
	this->radius = radius;
}
enum class MaterialBehaviour {
    Diffuse,
    Glass,
    Mirror
};

class Material {
public:
    Vector color;
    MaterialBehaviour behaviour;

    Material(const Vector& color = Vector({ 0.0f, 0.0f, 0.0f }),
        MaterialBehaviour behaviour = MaterialBehaviour::Diffuse)
        : color(color), behaviour(behaviour) {
    }

    static std::string behaviourToString(MaterialBehaviour b) {
        switch (b) {
        case MaterialBehaviour::Diffuse: return "Diffuse";
        case MaterialBehaviour::Glass:   return "Glass";
        case MaterialBehaviour::Mirror:  return "Mirror";
        default:                         return "Unknown";
        }
    }

    friend std::ostream& operator<<(std::ostream& os, const Material& m) {
        os << "Material(" << m.color << ", " << behaviourToString(m.behaviour) << ")";
        return os;
    }
};

class Object {
    public:
    Sphere sphere;
    Material material;
    Object(const Sphere& sphere, const Material& material)
        : sphere(sphere), material(material) {
    }
    friend std::ostream& operator<<(std::ostream& os, const Object& obj) {
        os << "Object(" << obj.sphere.center << ", " << obj.sphere.radius << ", " << obj.material << ")";
        return os;
	}
};

void write_image(const std::string& filename, int width, int height, const std::vector<Pixel>& p) {
    std::ofstream out(filename);
    if (!out) {
        std::cerr << "ça marche pas, je peux pas écrire";
        return;
    }

    out << "P3\n" << width << " " << height << "\n255\n";

    for (int i = 0; i < width * height; i++) {
        out << static_cast<int>(p[i].r()) << " " << static_cast<int>(p[i].g()) << " " << static_cast<int>(p[i].b()) << " ";
        if ((i + 1) % width == 0) out << "\n";
    }
    out.close();
}

float* intersect(const Rayon& r, const Sphere& s) {
    Vector oc = r.origin - s.center;
    float carrayon = s.radius * s.radius;
    float a = r.direction.dot(r.direction);
    float b = 2.0f * oc.dot(r.direction);
    float c = oc.dot(oc) - carrayon;
    float discriminant = b * b - 4.0f * a * c;

    if (discriminant < 0.0f) {
        return nullptr;
    }

    float sqrtD = std::sqrt(discriminant);
    float t0 = (-b - sqrtD) / (2.0f * a);
    float t1 = (-b + sqrtD) / (2.0f * a);

    if (t0 >= 0.0f) {
        return new float(t0);
    } else if (t1 >= 0.0f) {
        return new float(t1);
    } else {
        return nullptr;
    }
}


std::pair<float, Object>* intersectMult(const Rayon& r, const std::vector<Object>& objects) {
    std::pair<float, Object>* closest = nullptr;

    for (const Object& obj : objects) {
        float* tptr = intersect(r, obj.sphere);
        if (tptr != nullptr) {
            float t = *tptr;
            if (closest == nullptr || t < closest->first) {
                delete closest;
                closest = new std::pair<float, Object>(t, obj);
            }
            delete tptr;
        }
    }

    return closest;
}

Pixel radiance(const Rayon& r) {
    std::vector<Object> scene;
    scene.push_back(Object(Sphere(Vector({ 500.0f, 500.0f, 500.0f }), 200.0f), Material(Vector({ 0.0f,0.0f,0.0f}), MaterialBehaviour::Diffuse)));
    scene.push_back(Object(Sphere(Vector({ 255.0f, 500.0f, 255.0f }), 100.0f), Material(Vector({ 0.0f,0.0f,0.0f }), MaterialBehaviour::Diffuse)));
    scene.push_back(Object(Sphere(Vector({ +100900.0f, 500.0f, 255.0f }), 100000.0f), Material(Vector({ 255.0f,0.0f,0.0f }), MaterialBehaviour::Diffuse)));//Mur droite
    scene.push_back(Object(Sphere(Vector({ -99900.0f, 500.0f, 255.0f }), 100000.0f), Material(Vector({ 0.0f,0.0f,255.0f }), MaterialBehaviour::Diffuse)));//Mur gauche
    scene.push_back(Object(Sphere(Vector({ 500.0f, 100900.0f, 255.0f }), 100000.0f), Material(Vector({ 255.0f,255.0f,255.0f }), MaterialBehaviour::Diffuse)));//Mur bas
    scene.push_back(Object(Sphere(Vector({ 500.0f, -99900.0f, 255.0f }), 100000.0f), Material(Vector({ 255.0f,255.0f,255.0f }), MaterialBehaviour::Diffuse)));//Mur haut
    scene.push_back(Object(Sphere(Vector({ 500.0f, 500.0f, 100900.0f }), 100000.0f), Material(Vector({ 255.0f,255.0f,255.0f }), MaterialBehaviour::Diffuse)));//Mur fond


    Vector light_pos({ 500.0f, -1000.0f, 255.0f });

    std::pair<float, Object>* hit = intersectMult(r, scene);
    if (hit != nullptr) {
        float t = hit->first;
        Object hitObject = hit->second;
        delete hit;

        Vector x = r.origin + (r.direction * t);
        Vector direction_to_light = (light_pos - x).normalize(); 
        Vector normal = (x - hitObject.sphere.center).normalize();
        float coef = normal.dot(direction_to_light);
		Vector tonemap = hitObject.material.color*coef;
        return Pixel(tonemap);
    } else {
        return Pixel::RED;
    }
}

Pixel raytrace(float x, float y) {
    Rayon r(Vector({ x, y, 0.0f }), Vector({ 0.0f, 0.0f, 1.0f }));
    return radiance(r);
}

int main() {
    int width = 1000;
    int height = 1000;
    std::vector<Pixel> pixels;
    pixels.reserve(width * height);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            pixels.push_back(raytrace(static_cast<float>(x), static_cast<float>(y)));
        }
    }

    write_image("first_image.ppm", width, height, pixels);
    return 0;
}

