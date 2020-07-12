#pragma once

#include <cstdint>

namespace cw::simplex {
	extern float frequency;
	extern float amplitude;
	extern float lacunarity;
	extern float persistence;
	float noise(const float &x);
	float noise(const float &x, const float &y);
	float noise(const float &x, const float &y, const float &z);
	float fractal(const size_t &octaves, const float &x);
	float fractal(const size_t &octaves, const float &x, const float &y);
	float fractal(const size_t &octaves, const float &x, const float &y, const float &z);
}
