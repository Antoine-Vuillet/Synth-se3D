#ifndef PIXEL_H
#define PIXEL_H

#include "MyVector.h"

class Pixel {
public:
    Vector color;

    static const Pixel WHITE;
    static const Pixel BLACK;
    static const Pixel RED;

    Pixel(float r, float g, float b);
    Pixel(const Vector& rgb);
	Pixel() : color({ 0.0f, 0.0f, 0.0f }) {}

    float r() const;
    float g() const;
    float b() const;
};
#endif 
