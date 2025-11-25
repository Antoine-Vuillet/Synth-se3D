#define TINYOBJLOADER_IMPLEMENTATION
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
#include "tiny_obj_loader.h"
#include <omp.h>
#include <random>
#include <optional>


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


struct AABB {
    Vector min;
    Vector max;

    AABB() :
        min(Vector({ FLT_MAX, FLT_MAX, FLT_MAX })),
        max(Vector({ -FLT_MAX, -FLT_MAX, -FLT_MAX })) {
    }

    void expand(const Vector& p) {
        min.x = std::min(min.x, p.x);
        min.y = std::min(min.y, p.y);
        min.z = std::min(min.z, p.z);
        max.x = std::max(max.x, p.x);
        max.y = std::max(max.y, p.y);
        max.z = std::max(max.z, p.z);
    }

    void expand(const AABB& b) {
        expand(b.min);
        expand(b.max);
    }
};

AABB computeBounds(const Object& obj) {
    AABB box;

    if (obj.type == ObjectType::Sphere) {
        Vector c = obj.sphere.center;
        float r = obj.sphere.radius;

        box.expand(Vector({ c.x - r, c.y - r, c.z - r }));
        box.expand(Vector({ c.x + r, c.y + r, c.z + r }));
    }
    else {
        box.expand(obj.triangle.v0);
        box.expand(obj.triangle.v1);
        box.expand(obj.triangle.v2);
    }

    return box;
}


struct BVHNode
{
    Vector aabbMin, aabbMax;     // 24 bytes
    int leftChild, rightChild;  // 8 bytes
    bool isLeaf;                 // 4 bytes
    int firstPrim, primCount;   // 8 bytes; total: 44 bytes
};

int buildBVH(
    std::vector<BVHNode>& nodes,
    std::vector<int>& primIdx,
    const std::vector<AABB>& bounds,
    int start, int end)
{
    int nodeIndex = nodes.size();
    nodes.push_back(BVHNode());

    // Build node bounding box
    AABB nodeBox;
    for (int i = start; i < end; ++i)
        nodeBox.expand(bounds[primIdx[i]]);

    BVHNode& node = nodes[nodeIndex];
    node.aabbMin = nodeBox.min;
    node.aabbMax = nodeBox.max;

    int count = end - start;

    // Leaf condition
    if (count <= 2) {
        node.isLeaf = true;
        node.firstPrim = start;
        node.primCount = count;
        node.leftChild = node.rightChild = -1;
        return nodeIndex;
    }

    // Determine split axis (longest dimension)
    Vector size = nodeBox.max - nodeBox.min;
    int axis = 0;
    if (size.y > size.x && size.y > size.z) axis = 1;
    else if (size.z > size.x) axis = 2;

    // Sort primitives by centroid
    std::sort(primIdx.begin() + start, primIdx.begin() + end,
        [&](int a, int b) {
            Vector ca = (bounds[a].min + bounds[a].max) * 0.5f;
            Vector cb = (bounds[b].min + bounds[b].max) * 0.5f;
            return (axis == 0 ? ca.x : axis == 1 ? ca.y : ca.z) <
                (axis == 0 ? cb.x : axis == 1 ? cb.y : cb.z);
        });

    int mid = (start + end) / 2;

    node.isLeaf = false;
    node.firstPrim = -1;

    node.leftChild = buildBVH(nodes, primIdx, bounds, start, mid);
    node.rightChild = buildBVH(nodes, primIdx, bounds, mid, end);

    return nodeIndex;
}



struct Light {
    Vector position;
    Vector emission;
};

std::vector<Object> loadOBJ(const std::string& filename)
{
    tinyobj::ObjReaderConfig config;
    config.mtl_search_path = "./";

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(filename, config)) {
        if (!reader.Error().empty())
            std::cerr << "TinyObjReader Error: " << reader.Error() << "\n";
        exit(1);
    }

    if (!reader.Warning().empty())
        std::cout << "TinyObjReader Warning: " << reader.Warning() << "\n";

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();
    const auto& materials = reader.GetMaterials();

    // First, compute bounding box
    Vector minV({ FLT_MAX, FLT_MAX, FLT_MAX });
    Vector maxV({ -FLT_MAX, -FLT_MAX, -FLT_MAX });

    for (size_t v = 0; v < attrib.vertices.size() / 3; v++) {
        float x = attrib.vertices[3 * v + 0];
        float y = attrib.vertices[3 * v + 1];
        float z = attrib.vertices[3 * v + 2];

        minV.x = std::min(minV.x, x);
        minV.y = std::min(minV.y, y);
        minV.z = std::min(minV.z, z);

        maxV.x = std::max(maxV.x, x);
        maxV.y = std::max(maxV.y, y);
        maxV.z = std::max(maxV.z, z);
    }

    float maxRange = std::max({ maxV.x - minV.x, maxV.y - minV.y, maxV.z - minV.z });
    float scale = 800.0f / maxRange;

    std::vector<Object> objects;

    for (const auto& shape : shapes) {
        size_t index_offset = 0;

        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            size_t fv = shape.mesh.num_face_vertices[f];

            if (fv != 3) {
                std::cerr << "Non-triangle face found, FV=" << fv << "\n";
                index_offset += fv;
                continue;
            }

            tinyobj::index_t i0 = shape.mesh.indices[index_offset + 0];
            tinyobj::index_t i1 = shape.mesh.indices[index_offset + 1];
            tinyobj::index_t i2 = shape.mesh.indices[index_offset + 2];

            auto transformVertex = [&](const tinyobj::index_t& idx) {
                return Vector({
                    (attrib.vertices[3 * idx.vertex_index + 0] - minV.x) * scale+100,       // X stays the same
                    (maxV.y - attrib.vertices[3 * idx.vertex_index + 1]) * scale+100,       // Flip Y and translate
                    (attrib.vertices[3 * idx.vertex_index + 2] - minV.z) * scale        // Z stays the same
                    });
                };


            Vector v0 = transformVertex(i0);
            Vector v1 = transformVertex(i1);
            Vector v2 = transformVertex(i2);

            int mat_id = shape.mesh.material_ids[f];

            Material mat_obj({ 255,255,255 }, MaterialBehaviour::Diffuse);

            if (mat_id >= 0 && mat_id < (int)materials.size()) {
                const auto& mat = materials[mat_id];
                mat_obj = Material(
                    Vector({
                        mat.diffuse[0] * 255.0f,
                        mat.diffuse[1] * 255.0f,
                        mat.diffuse[2] * 255.0f
                        }),
                    MaterialBehaviour::Diffuse
                );
            }

            objects.push_back(Object(Triangle(v0, v1, v2), mat_obj));

            index_offset += fv;
        }
    }

    return objects;
}


std::vector<Light> lights = {
    {Vector({100.0f, 500.0f, 100.0f}), Vector({100000.0f,100000.0f,100000.0f})}/*
    {Vector({250.0f, 400.0f, 250.0f}), Vector({1.0f,1.0f,1.0f})}*/
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

float intersectPrimitive(const Rayon& r, const Object& obj)
{
    if (obj.type == ObjectType::Sphere)
        return intersect(r, obj.sphere);

    else // triangle
        return intersectTriangle(r, obj.triangle);
}


bool intersectAABB(const Rayon& r, const Vector aabbMin, const Vector aabbMax) {
    float tmin = (aabbMin.x - r.origin.x) * r.invDir.x;
    float tmax = (aabbMax.x - r.origin.x) * r.invDir.x;
    if (tmin > tmax) std::swap(tmin, tmax);

    float tymin = (aabbMin.y - r.origin.y) * r.invDir.y;
    float tymax = (aabbMax.y - r.origin.y) * r.invDir.y;
    if (tymin > tymax) std::swap(tymin, tymax);
    if ((tmin > tymax) || (tymin > tmax)) return false;
    tmin = std::max(tmin, tymin);
    tmax = std::min(tmax, tymax);

    float tzmin = (aabbMin.z - r.origin.z) * r.invDir.z;
    float tzmax = (aabbMax.z - r.origin.z) * r.invDir.z;
    if (tzmin > tzmax) std::swap(tzmin, tzmax);
    if ((tmin > tzmax) || (tzmin > tmax)) return false;

    return true;
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



std::pair<int, float> intersectBVH(
    const Rayon& r,
    const std::vector<BVHNode>& nodes,
    const std::vector<int>& primIdx,
    const std::vector<Object>& scene)
{
    int stack[64];
    int sp = 0;
    stack[sp++] = 0;

    float closest = std::numeric_limits<float>::infinity();
    int hitIndex = -1;

    while (sp > 0) {
        int idx = stack[--sp];
        const BVHNode& node = nodes[idx];

        if (!intersectAABB(r, node.aabbMin, node.aabbMax)) continue;

        if (node.isLeaf) {
            for (int i = 0; i < node.primCount; ++i) {
                int prim = primIdx[node.firstPrim + i];
                float t = intersectPrimitive(r, scene[prim]);
                if (t > 0 && t < closest) {
                    closest = t;
                    hitIndex = prim;
                }
            }
        }
        else {
            stack[sp++] = node.leftChild;
            stack[sp++] = node.rightChild;
        }
    }

    if (hitIndex == -1) return { -1, -1.0f };
    return { hitIndex, closest };
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

Pixel radiance(const Rayon& r,const std::vector<Object>& scene,const std::vector<BVHNode>& nodes,const std::vector<int>& primIdx,int Maxbounce)
{
    Vector total_light({ 0.0f,0.0f,0.0f });

    auto hit = intersectBVH(r, nodes, primIdx, scene);
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
                auto hit_Light = intersectBVH(
                    Rayon(x + direction_to_light_normalized * epsilon, direction_to_light_normalized),nodes,primIdx,scene);
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
                return radiance(reflectedRay,scene, nodes, primIdx, Maxbounce+1);
            }
            else {
				Vector refractedDirection = transmittedRay->second;
                Rayon reflectedRay = Rayon(x + (refractedDirection * epsilon), refractedDirection);
                return radiance(reflectedRay,scene, nodes, primIdx, Maxbounce+1);
            }
        }
        else if (hitObject.material.behaviour == MaterialBehaviour::Mirror) {
            Vector reflectedDirection = reflect(normal*-1, r.direction);
            Rayon reflectedRay = Rayon(x + (reflectedDirection * epsilon), reflectedDirection);
            return radiance(reflectedRay,scene, nodes, primIdx, Maxbounce+1);
        }
        
    }
    else {
        return Pixel::RED;
    }
}



Pixel raytrace(float x, float y,const std::vector<Object>& scene,const std::vector<BVHNode>& nodes,const std::vector<int>& primIdx)
{
    float coefOpening = 1.001f;
    Vector n({ x, y, 0.0f });
    Vector n2 = n-Vector({500.0f, 500.0f, 0.0f});
    Vector f = Vector({ coefOpening * n2.getValues()[0], coefOpening * n2.getValues()[1], 1.0f }) + Vector({500,500,0});
    Vector dir = (f - n).normalize();
    Rayon r(n, dir);
    return radiance(r, scene, nodes, primIdx, 0);
}

int main() {

    /*static const std::vector<Object> scene = []() {
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
    }();*/

    std::vector<Object> scene= loadOBJ("C:/Users/avuillet/Documents/Synthese_image_3d/Synth-se3D/LancerDeRayon/dragon_vrip.obj");

    scene.reserve(scene.size() + 8);
    scene.push_back(Object(Sphere(Vector({ +101000.0f, 500.0f, 250.0f }), 100000.0f), Material(Vector({ 255.0f,255.0f,0.0f }), MaterialBehaviour::Diffuse))); //Mur droit
    scene.push_back(Object(Sphere(Vector({ -100000.0f, 500.0f, 250.0f }), 100000.0f), Material(Vector({ 0.0f,255.0f,255.0f }), MaterialBehaviour::Diffuse)));  //Mur gauche
    scene.push_back(Object(Sphere(Vector({ 500.0f, 101000.0f, 250.0f }), 100000.0f), Material(Vector({ 255.0f,255.0f,255.0f }), MaterialBehaviour::Diffuse)));  //Mur bas
    scene.push_back(Object(Sphere(Vector({ 500.0f, -100000.0f, 250.0f }), 100000.0f), Material(Vector({ 255.0f,255.0f,255.0f }), MaterialBehaviour::Diffuse))); //Mur haut
    scene.push_back(Object(Sphere(Vector({ 500.0f, 500.0f, 101000.0f }), 100000.0f), Material(Vector({ 255.0f,255.0f,255.0f }), MaterialBehaviour::Diffuse)));  //Mur arrière
    scene.push_back(Object(Sphere(Vector({ 500.0f, 500.0f, -100100.0f }), 100000.0f), Material(Vector({ 255.0f,0.0f,0.0f }), MaterialBehaviour::Diffuse))); //Mur precaméra

    std::vector<AABB> bounds(scene.size());
    for (int i = 0; i < scene.size(); ++i)
        bounds[i] = computeBounds(scene[i]);

    std::vector<int> primIdx(scene.size());
    for (int i = 0;i < scene.size();++i) primIdx[i] = i;

    std::vector<BVHNode> nodes;
    nodes.reserve(scene.size() * 2);

    int root = buildBVH(nodes, primIdx, bounds, 0, scene.size());

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
                Pixel p = raytrace(u, v, scene, nodes, primIdx);
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
