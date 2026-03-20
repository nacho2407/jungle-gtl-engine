#pragma once
#define M_PI 3.14159265358979323846f
#define DEG_TO_RAD (M_PI / 180.0f)
#define RAD_TO_DEG (180.0f / M_PI)
#define EPSILON 1e-4

float Clamp(float val, float lo, float hi);