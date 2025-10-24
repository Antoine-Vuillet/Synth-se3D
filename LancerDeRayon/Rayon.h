#ifndef RAYON_H
#define RAYON_H

#include "MyVector.h" 

class Rayon {
public:
    Vector origin;
    Vector direction;

    Rayon(const Vector& origin, const Vector& direction);
};

#endif // RAYON_H
#pragma once
