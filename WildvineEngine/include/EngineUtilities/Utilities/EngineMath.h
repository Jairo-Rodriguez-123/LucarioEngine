/**
 * @file EngineMath.h
 * @brief Declara la API de EngineMath dentro del subsistema Utilities.
 * @ingroup utilities
 */
/*
 * MIT License
 *
 * Copyright (c) 2024 Roberto Charreton
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * In addition, any project or software that uses this library or class must include
 * the following acknowledgment in the credits:
 *
 * "This project uses software developed by Roberto Charreton and Attribute Overload."
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/
#pragma once
#include <cmath>
#include <limits>

namespace EU {
	constexpr float PI = 3.14159265358979323846f;
	constexpr float E  = 2.71828182845904523536f;

	inline float sqrt(float value) { return value < 0.0f ? 0.0f : std::sqrt(value); }
	inline float square(float value) { return value * value; }
	inline float cube(float value) { return value * value * value; }
	inline float power(float base, int exponent) {
		if (exponent == 0) return 1.0f;
		// Forma entre parentesis para evitar colisiones con las macros min/max de Windows.
		if (exponent == (std::numeric_limits<int>::min)())
			return 1.0f / (power(base, (std::numeric_limits<int>::max)()) * base);
		if (exponent < 0) return 1.0f / power(base, -exponent);
		float result = 1.0f;
		while (exponent > 0) {
			if (exponent & 1) result *= base;
			base *= base;
			exponent >>= 1;
		}
		return result;
	}
	inline float abs(float value) { return std::fabs(value); }
	inline float EMax(float a, float b) { return a > b ? a : b; }
	inline float EMin(float a, float b) { return a < b ? a : b; }
	inline float round(float value) { return std::round(value); }
	inline float floor(float value) { return std::floor(value); }
	inline float ceil(float value) { return std::ceil(value); }
	inline float fabs(float value) { return std::fabs(value); }
	inline float sin(float angle) { return std::sin(angle); }
	inline float cos(float angle) { return std::cos(angle); }
	inline float tan(float angle) { return std::tan(angle); }
	inline float asin(float value) { return std::asin(EMax(-1.0f, EMin(1.0f, value))); }
	inline float acos(float value) { return std::acos(EMax(-1.0f, EMin(1.0f, value))); }
	inline float atan(float value) { return std::atan(value); }
	inline float sinh(float value) { return std::sinh(value); }
	inline float cosh(float value) { return std::cosh(value); }
	inline float tanh(float value) { return std::tanh(value); }
	inline float radians(float degreesValue) { return degreesValue * PI / 180.0f; }
	inline float degrees(float radiansValue) { return radiansValue * 180.0f / PI; }
	inline float exp(float value) { return std::exp(value); }
	inline float log(float value) { return value > 0.0f ? std::log(value) : 0.0f; }
	inline float log10(float value) { return value > 0.0f ? std::log10(value) : 0.0f; }
	inline float mod(float a, float b) { return b == 0.0f ? 0.0f : std::fmod(a, b); }
	inline float circleArea(float radius) { return PI * radius * radius; }
	inline float circleCircumference(float radius) { return 2.0f * PI * radius; }
	inline float rectangleArea(float width, float height) { return width * height; }
	inline float rectanglePerimeter(float width, float height) { return 2.0f * (width + height); }
	inline float triangleArea(float base, float height) { return 0.5f * base * height; }
	inline float distance(float x1, float y1, float x2, float y2) {
		const float dx = x2 - x1;
		const float dy = y2 - y1;
		return std::sqrt(dx * dx + dy * dy);
	}
	inline float lerp(float a, float b, float t) { return a + t * (b - a); }
	inline int factorial(int n) {
		if (n < 0) return 0;
		int result = 1;
		for (int i = 2; i <= n; ++i) result *= i;
		return result;
	}
	inline bool approxEqual(float a, float b, float epsilon) { return std::fabs(a - b) <= std::fabs(epsilon); }
}
