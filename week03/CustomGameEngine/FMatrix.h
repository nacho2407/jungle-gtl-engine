#pragma once

struct FMatrix {
  float m[4][4];

  FMatrix();

  FMatrix operator*(const FMatrix& other) const;
};
