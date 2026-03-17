#include "Color.h"

FColor FColor::operator + (float num) const {
  return {r + num, g + num, b + num, a};
}

FColor FColor::operator+(const FColor& c) const {
  return {r + c.r, g + c.g, b + c.b, a};
}

FColor FColor::operator-(float num) const {
  return {r - num, g - num, b - num, a};
}

FColor FColor::operator-(const FColor& c) const {
  return {r - c.r, g - c.g, b - c.b, a};
}

FColor FColor::operator*(float num) const {
  return {r * num, g * num, b * num, a};
}

FColor FColor::operator*(const FColor& c) const {
  return {r * c.r, g * c.g, b * c.b, a};
}
