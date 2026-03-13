#pragma once

struct FMatrix4x4 {
  float m[4][4];

  FMatrix4x4() {
    for (int i{0}; i < 4; i++)
      for (int j{0}; j < 4; j++) m[i][j] = (i == j) ? 1.0f : 0.0f;
  }

  FMatrix4x4 operator*(const FMatrix4x4& other) const {
    FMatrix4x4 result;
    for (int i{0}; i < 4; i++) {
      for (int j{0}; j < 4; j++) {
        result.m[i][j] = m[i][0] * other.m[0][j] + m[i][1] * other.m[1][j] +
                         m[i][2] * other.m[2][j] + m[i][3] * other.m[3][j];
      }
    }
    return result;
  }
};