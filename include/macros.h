#pragma once
#include "nvVector.h"
#include <cassert>

using namespace nv;

#define EPSILON (1e-5f)
#define EPS_RAY (1e-5f)
#define COS_TERM_MIN (1e-6f)

#define max2(a, b) a > b ? a : b
#define min2(a, b) a < b ? a : b

#if defined(_WIN32)
	#define isnan _isnan
	#define M_PI 3.14159265358979323846f
#endif

typedef vec2<double> vec2d;
typedef vec3<double> vec3d;

// false: use shading normal, true: use geometric normal
const bool flatNormals = false;

const bool useRussian = false;