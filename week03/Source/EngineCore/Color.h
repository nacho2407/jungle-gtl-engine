#pragma once

struct FColor {
  float r, g, b, a;
  FColor(float _r = 0.f, float _g = 0.f, float _b = 0.f, float _a = 1.0f)
      : r(_r), g(_g), b(_b), a(_a) {}

  FColor operator+(float num) const;
  FColor operator+(const FColor& c) const;
  FColor operator-(float num) const;
  FColor operator-(const FColor& c) const;
  FColor operator*(float num) const;
  FColor operator*(const FColor& c) const;
};
