#include "Pixel.h"

Pixel::Pixel(float r, float g, float b)
    : color({ r, g, b }) {
}

Pixel::Pixel(const Vector& rgb)
    : color(rgb) {
}

float Pixel::r() const { return color.getValues()[0]; }
float Pixel::g() const { return color.getValues()[1]; }
float Pixel::b() const { return color.getValues()[2]; }

// Static constants
const Pixel Pixel::WHITE(255.0f, 255.0f, 255.0f);
const Pixel Pixel::BLACK(0.0f, 0.0f, 0.0f);
const Pixel Pixel::RED(255.0f, 0.0f, 0.0f);
