#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <thread>
#include <limits>
#include <chrono>
#include <atomic>
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
    std::ofstream out(filename, std::ios::binary);
    if (!out) {
        std::cerr << "ça marche pas, je peux pas écrire";
        return;
    }

    // header P6 (binaire)
    out << "P6\n" << width << " " << height << "\n255\n";

    // buffer binaire : 3 bytes par pixel
    std::vector<unsigned char> buf;
    buf.reserve(width * height * 3);
    for (int i = 0; i < width * height; ++i) {
        auto clamp = [](float v) -> unsigned char {
            if (v <= 0.0f) return 0;
            if (v >= 255.0f) return 255;
            return static_cast<unsigned char>(v + 0.5f);
        };
        buf.push_back(clamp(p[i].r()));
        buf.push_back(clamp(p[i].g()));
        buf.push_back(clamp(p[i].b()));
    }

    out.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    out.close();
}

float intersect(const Rayon& r, const Sphere& s) {
    Vector oc = r.origin - s.center;
    float a = r.direction.dot(r.direction);
    float b = 2.0f * oc.dot(r.direction);
    float c = oc.dot(oc) - s.radius * s.radius;
    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f) return -1.0f;
    float sqrtD = std::sqrt(discriminant);
    float t0 = (-b - sqrtD) / (2.0f * a);
    float t1 = (-b + sqrtD) / (2.0f * a);
    if (t0 >= 0.0f) return t0;
    if (t1 >= 0.0f) return t1;
    return -1.0f;
}

std::pair<int, float> intersectMult(const Rayon& r, const std::vector<Object>& objects) {
    int hitIndex = -1;
    float closest = std::numeric_limits<float>::infinity();
    for (size_t i = 0; i < objects.size(); ++i) {
        float t = intersect(r, objects[i].sphere);
        if (t >= 0.0f && t < closest) {
            closest = t;
            hitIndex = static_cast<int>(i);
        }
    }
    if (hitIndex == -1) return std::make_pair(-1, -1.0f);
    return std::make_pair(hitIndex, closest);
}

Pixel radiance(const Rayon& r) {
    static const std::vector<Object> scene = []() {
        std::vector<Object> s;
        s.reserve(7);
        s.push_back(Object(Sphere(Vector({ 300.0f, 700.0f, 700.0f }), 80.0f), Material(Vector({ 255.0f,255.0f,255.0f }), MaterialBehaviour::Diffuse)));
        s.push_back(Object(Sphere(Vector({ 700.0f, 700.0f, 700.0f }), 80.0f), Material(Vector({ 255.0f,255.0f,255.0f }), MaterialBehaviour::Diffuse)));
        s.push_back(Object(Sphere(Vector({ +101000.0f, 500.0f, 250.0f }), 100000.0f), Material(Vector({ 255.0f,255.0f,0.0f }), MaterialBehaviour::Diffuse)));
        s.push_back(Object(Sphere(Vector({ -100000.0f, 500.0f, 250.0f }), 100000.0f), Material(Vector({ 0.0f,255.0f,255.0f }), MaterialBehaviour::Diffuse)));
        s.push_back(Object(Sphere(Vector({ 500.0f, 101000.0f, 250.0f }), 100000.0f), Material(Vector({ 255.0f,255.0f,255.0f }), MaterialBehaviour::Diffuse)));
        s.push_back(Object(Sphere(Vector({ 500.0f, -100000.0f, 250.0f }), 100000.0f), Material(Vector({ 255.0f,255.0f,255.0f }), MaterialBehaviour::Diffuse)));
        s.push_back(Object(Sphere(Vector({ 500.0f, 500.0f, 101000.0f }), 100000.0f), Material(Vector({ 255.0f,255.0f,255.0f }), MaterialBehaviour::Diffuse)));
        return s;
        }();

    Vector light_pos({ 500.0f, 500.0f, 500.0f });
	Vector light_emission({200000.0f, 200000.0f,200000.0f });

    auto hit = intersectMult(r, scene);
    if (hit.first != -1) {
        float t = hit.second;
        const Object& hitObject = scene[hit.first];
        Vector x = r.origin + (r.direction * t);
        Vector direction_to_light = light_pos - x;
        Vector normal = (x - hitObject.sphere.center).normalize();
        float light_distance = direction_to_light.dot(direction_to_light);
		Vector direction_to_light_normalized = direction_to_light.normalize();
        float coef = std::max(0.0f, normal.dot(direction_to_light_normalized))/light_distance;
		float epsilon = 0.1f;
		auto hit_Light = intersectMult(Rayon(x+ direction_to_light_normalized* epsilon, direction_to_light_normalized), scene);
		//Ma logique est opposée à celle de guibou sur ce qui suit pour une raison que j'ignore (Il avait tort, c'est pour ça gneheheh)
        bool canSeeLightSource = true;
        if (hit_Light.first != -1) {
            float t_block = hit_Light.second;
            if ((t_block * t_block) < light_distance) {
                canSeeLightSource = false;
            }
        }
        Vector visibility;
        if (canSeeLightSource) {
			visibility = Vector({ 1.0f,1.0f,1.0f });
        }
        else {

            visibility = Vector({ 0.0f,0.0f,0.0f });
        }
        Vector tonemap = visibility *(hitObject.material.color * coef) * light_emission;
        return Pixel(tonemap);
    }
    else {
        return Pixel::BLACK;
    }
}

Pixel raytrace(float x, float y) {
    float coefOpening = 1.001f;
    Vector n({ x, y, 0.0f });
    Vector n2 = n-Vector({500.0f, 500.0f, 0.0f});
    Vector f = Vector({ coefOpening * n2.getValues()[0], coefOpening * n2.getValues()[1], 1.0f }) + Vector({500,500,0});
    Vector dir = (f - n).normalize();
    Rayon r(n, dir);
    return radiance(r);
}

int main() {
    // Désactiver la synchronisation iostream -> gain léger au démarrage
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int width = 1000;
    int height = 1000;
    std::vector<Pixel> pixels;
    pixels.resize(width * height); // allocation unique

    using clock = std::chrono::high_resolution_clock;
    auto t0 = clock::now();

    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4; // fallback

    std::atomic<int> rowsDone(0);
    const int progressInterval = 50; // n'affiche la progression que toutes les n lignes (réduit le cout IO)

    auto worker = [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            for (int x = 0; x < width; ++x) {
                pixels[y * width + x] = raytrace(static_cast<float>(x), static_cast<float>(y));
            }
            int done = ++rowsDone;
            if (done % progressInterval == 0) {
                // affichage léger et non bloquant
                std::cout << "." << std::flush;
            }
        }
        };

    auto t_before_threads = clock::now();

    std::vector<std::thread> threads;
    threads.reserve(numThreads);
    int rowsPerThread = height / static_cast<int>(numThreads);
    int y = 0;
    for (unsigned int i = 0; i < numThreads; ++i) {
        int y1 = (i + 1 == numThreads) ? height : (y + rowsPerThread);
        threads.emplace_back(worker, y, y1);
        y = y1;
    }

    auto t_after_spawn = clock::now();

    for (auto& t : threads) t.join();

    auto t_after_join = clock::now();

    // écrire l'image
    write_image("first_image.ppm", width, height, pixels);

    auto t_after_write = clock::now();

    std::chrono::duration<double> total = t_after_write - t0;
    std::chrono::duration<double> spawn_cost = t_after_spawn - t_before_threads;
    std::chrono::duration<double> threads_runtime = t_after_join - t_after_spawn;
    std::chrono::duration<double> write_cost = t_after_write - t_after_join;

    std::cout << "\nTimings (seconds):\n";
    std::cout << "  total: " << total.count() << "\n";
    std::cout << "  spawn threads: " << spawn_cost.count() << "\n";
    std::cout << "  thread runtime: " << threads_runtime.count() << "\n";
    std::cout << "  write image: " << write_cost.count() << "\n";
    return 0;
}