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
#include <omp.h>
#include <random>


thread_local std::mt19937 rng(std::random_device{}());
thread_local std::uniform_real_distribution<float> dist(0.0f, 1.0f);

inline float randomFloat() {
    return dist(rng);
}


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

class Triangle
{
public:
	Triangle(const Vector& v0, const Vector& v1, const Vector& v2);
	Vector v0;
	Vector v1;
	Vector v2;
};
Triangle::Triangle(const Vector& v0, const Vector& v1, const Vector& v2)
{
    this->v0 = v0;
    this->v1 = v1;
    this->v2 = v2;
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

enum class ObjectType {
    Sphere,
    Triangle
};


class Object {
public:
	ObjectType type;
    Sphere sphere;
	Triangle triangle;
    Material material;
    Object(const Sphere& s, const Material& m)
        : type(ObjectType::Sphere), sphere(s), triangle(Vector({ 0,0,0 }), Vector({ 0,0,0 }), Vector({ 0,0,0 })), material(m) {
    }
    Object(const Triangle& t, const Material& m)
        : type(ObjectType::Triangle), sphere(Vector({ 0,0,0 }), 0.0f), triangle(t), material(m) {
    }
};

struct Light {
    Vector position;
    Vector emission;
};

static const std::vector<Object> scene = []() {
    std::vector<Object> s;
    s.reserve(7);
    s.push_back(Object(Sphere(Vector({ 300.0f, 700.0f, 700.0f }), 80.0f), Material(Vector({ 255.0f,255.0f,255.0f }), MaterialBehaviour::Mirror)));
    s.push_back(Object(Sphere(Vector({ 700.0f, 700.0f, 700.0f }), 80.0f), Material(Vector({ 255.0f,255.0f,255.0f }), MaterialBehaviour::Glass)));
    s.push_back(Object(Triangle(Vector({ 500.0f,300.0f,600.0f }), Vector({ 300.0f,700.0f,600.0f }), Vector({ 700.0f,700.0f,600.0f })), Material(Vector({255.0f,255.0f,255.0f}), MaterialBehaviour::Mirror)));
	s.push_back(Object(Sphere(Vector({ +101000.0f, 500.0f, 250.0f }), 100000.0f), Material(Vector({ 255.0f,255.0f,0.0f }), MaterialBehaviour::Diffuse))); //Mur droit
	s.push_back(Object(Sphere(Vector({ -100000.0f, 500.0f, 250.0f }), 100000.0f), Material(Vector({ 0.0f,255.0f,255.0f }), MaterialBehaviour::Diffuse)));  //Mur gauche
	s.push_back(Object(Sphere(Vector({ 500.0f, 101000.0f, 250.0f }), 100000.0f), Material(Vector({ 255.0f,255.0f,255.0f }), MaterialBehaviour::Diffuse)));  //Mur bas
	s.push_back(Object(Sphere(Vector({ 500.0f, -100000.0f, 250.0f }), 100000.0f), Material(Vector({ 255.0f,255.0f,255.0f }), MaterialBehaviour::Diffuse))); //Mur haut
	s.push_back(Object(Sphere(Vector({ 500.0f, 500.0f, 101000.0f }), 100000.0f), Material(Vector({ 255.0f,255.0f,255.0f }), MaterialBehaviour::Diffuse)));  //Mur arrière
	s.push_back(Object(Sphere(Vector({ 500.0f, 500.0f, -100100.0f }), 100000.0f), Material(Vector({ 255.0f,0.0f,0.0f }), MaterialBehaviour::Diffuse))); //Mur precaméra
    return s;
    }();

std::vector<Light> lights = {
    {Vector({500.0f, 500.0f, 500.0f}), Vector({200000.0f,200000.0f,200000.0f})},
    {Vector({500.0f, 800.0f, 500.0f}), Vector({100000.0f,100000.0f,200000.0f})}
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

float intersectTriangle(Rayon r,
    Triangle tr)
{
    const float EPSILON = 0.1;
    Vector vertex0 = tr.v0;
    Vector vertex1 = tr.v1;
    Vector vertex2 = tr.v2;
    Vector edge1, edge2, h, s, q;
    float a, f, u, v;
    edge1 = vertex1 - vertex0;
    edge2 = vertex2 - vertex0;
    h = r.direction.cross(edge2);
    a = edge1.dot(h);
    if (a > -EPSILON && a < EPSILON)
        return -1.0f;
    f = 1.0 / a;
    s = r.origin - vertex0;
    u = f * (s.dot(h));
    if (u < 0.0 || u > 1.0)
        return -1.0f;
    q = s.cross(edge1);
    v = f * r.direction.dot(q);
    if (v < 0.0 || u + v > 1.0)
        return -1.0f;
    float t = f * edge2.dot(q);
    if (t > EPSILON)
    {
        return t;
    }
    else
        return -1;
}

std::pair<int, float> intersectMult(const Rayon& r, const std::vector<Object>& scene1) {
    int hitIndex = -1;
    float closest = std::numeric_limits<float>::infinity();
    for (size_t i = 0; i < scene1.size(); ++i) {
        float t = -1.0f;

        if (scene1[i].type == ObjectType::Sphere)
            t = intersect(r, scene1[i].sphere);

        else if (scene1[i].type == ObjectType::Triangle)
            t = intersectTriangle(r, scene1[i].triangle);
        if (t >= 0.0f && t < closest) {
            closest = t;
            hitIndex = static_cast<int>(i);
        }
    }
    if (hitIndex == -1) return std::make_pair(-1, -1.0f);
    return std::make_pair(hitIndex, closest);
}

Vector reflect(Vector n, Vector wi) {
    float proj = -n.dot(wi);
    return (n * (2 * proj) + wi);
}

std::pair<float,Vector>* refract(float iorp, Vector nl, Vector direction, bool outside) {
	float ior = iorp;
    if (outside)
    {
		ior = 1.0f / iorp;
    }
	float cos1 = direction.dot(nl);
	float cos2 = 1 - ior * ior * (1 - cos1 * cos1);
    if (cos2 < 0) {
		return NULL;
    }
    else {
		Vector tdir = ((direction * ior) - (nl * ((cos1 * ior + sqrt(cos2))))).normalize();
		float r0 = pow((ior - 1) / (ior + 1),2);
		float cosTheta = outside ? -cos1 : tdir.dot(nl);
        float reflectCoef = r0 + (1 - r0) * pow(cosTheta, 5);
		return new std::pair<float, Vector>(1-reflectCoef, tdir);
    }
}

Pixel radiance(const Rayon& r, int Maxbounce) {
    Vector total_light({ 0.0f,0.0f,0.0f });

    auto hit = intersectMult(r, scene);
    if (hit.first != -1) {
        float t = hit.second;
        const Object& hitObject = scene[hit.first];
        Vector x = r.origin + (r.direction * t);
        Vector normal;

        if (hitObject.type == ObjectType::Sphere) {
            normal = (x - hitObject.sphere.center).normalize();
        }
        else { // Triangle
            Vector edge1 = hitObject.triangle.v1 - hitObject.triangle.v0;
            Vector edge2 = hitObject.triangle.v2 - hitObject.triangle.v0;
            normal = edge1.cross(edge2).normalize();
            if (normal.dot(r.direction) > 0)
                normal = normal * -1.0f;
        }
        float epsilon = 0.1f;
        if (hitObject.material.behaviour == MaterialBehaviour::Diffuse) {
            for (const auto& light : lights) {
                Vector direction_to_light = light.position - x;
                float light_distance = direction_to_light.dot(direction_to_light);
                Vector direction_to_light_normalized = direction_to_light.normalize();
                float coef = std::max(0.0f, normal.dot(direction_to_light_normalized)) / light_distance;                
                auto hit_Light = intersectMult(Rayon(x + direction_to_light_normalized * epsilon, direction_to_light_normalized), scene);
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
                total_light = total_light + visibility * (hitObject.material.color * coef) * light.emission;
            }

            return Pixel(total_light);
        }
        else if (Maxbounce>=10)
        {
            return Pixel::BLACK;
        }
        else if (hitObject.material.behaviour==MaterialBehaviour::Glass)
        {
			bool outside = (r.direction.dot(normal) < 0);
			Vector normal2 = normal;
            if (!outside) {
				normal2 = normal * -1;
            }
            std::pair<float,Vector>* transmittedRay = refract(1.5,normal2, r.direction, outside);
            if (transmittedRay == NULL) {
                Vector reflectedDirection = reflect(normal, r.direction);
                Rayon reflectedRay = Rayon(x + (reflectedDirection * epsilon), reflectedDirection);
                return radiance(reflectedRay, Maxbounce+1);
            }
            else {
				Vector refractedDirection = transmittedRay->second;
                Rayon reflectedRay = Rayon(x + (refractedDirection * epsilon), refractedDirection);
                return radiance(reflectedRay, Maxbounce+1);
            }
        }
        else if (hitObject.material.behaviour == MaterialBehaviour::Mirror) {
            Vector reflectedDirection = reflect(normal*-1, r.direction);
            Rayon reflectedRay = Rayon(x + (reflectedDirection * epsilon), reflectedDirection);
            return radiance(reflectedRay, Maxbounce+1);
        }
        
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
    return radiance(r,0);
}

int main() {
    // Désactiver la synchronisation iostream -> gain léger au démarrage
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int width = 1000;
    int height = 1000;
    std::vector<Pixel> pixels(width * height);

    using clock = std::chrono::high_resolution_clock;
    auto t0 = clock::now();

    std::atomic<int> rowsDone(0);
    const int progressInterval = 50;

    #pragma omp parallel for schedule(static)
    for (int y = 0; y < height; ++y) {
        // Thread-local random generator for antialiasing
        thread_local std::mt19937 rng(std::random_device{}());
        thread_local std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        auto randomFloat = [&]() { return dist(rng); };

        for (int x = 0; x < width; ++x) {
            Vector colorSum({ 0, 0, 0 });
            const int samples = 32;
            for (int s = 0; s < samples; ++s) {
                float u = x + randomFloat();
                float v = y + randomFloat();
                Pixel p = raytrace(u, v);
                colorSum = colorSum + Vector({ p.r(), p.g(), p.b() });
            }
            colorSum = colorSum * (1.0f / samples);
            pixels[y * width + x] = Pixel(colorSum);
        }

        int done = ++rowsDone;
        if (done % progressInterval == 0) {
            #pragma omp critical
            std::cout << "." << std::flush;
        }
    }

    auto t_after_render = clock::now();

    // Écrire l'image sur disque
    write_image("first_image.ppm", width, height, pixels);

    auto t_after_write = clock::now();

    std::chrono::duration<double> total = t_after_write - t0;
    std::chrono::duration<double> render_time = t_after_render - t0;
    std::chrono::duration<double> write_time = t_after_write - t_after_render;

    std::cout << "\nTimings (seconds):\n";
    std::cout << "  total: " << total.count() << "\n";
    std::cout << "  render time: " << render_time.count() << "\n";
    std::cout << "  write image: " << write_time.count() << "\n";

    return 0;
}
