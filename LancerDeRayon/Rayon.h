#ifndef RAYON_H
#define RAYON_H

#include "MyVector.h" 

class Rayon {
public:
    Vector origin;
    Vector direction;
    Vector invDir = Vector({ 1.0f / direction.x, 1.0f / direction.y, 1.0f / direction.z });

    Rayon(const Vector& origin, const Vector& direction);
};

#endif // RAYON_H
#pragma once
