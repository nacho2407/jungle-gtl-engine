#pragma once

#include <algorithm>

#include "FVector.h"
#include "FMatrix.h"

const FVector UpVec(0.0f, 1.0f, 0.0f);
const float Epsilon = 1e-6f;

class UCamera {
 public:
  FVector Position{FVector(0.0f, 0.0f, 0.0f)};
  float Pitch{0.0f};
  float Yaw{0.0f};
  float Roll{0.0f};
  bool IsChanged{true};

  float FOV{60.0f};
  float AspectRatio{16.0f / 9.0f};
  float NearPlane{0.1f};
  float FarPlane{1000.0f};

  FVector Direction{FVector(0.0f, 0.0f, 1.0f)};
  FVector SideDirection{FVector(1.0f, 0.0f, 0.0f)};
  FVector UpDirection{FVector(0.0f, 1.0f, 0.0f)};

  FMatrix View;
  FMatrix Projection;
  FMatrix ViewProjection;

  public:
    UCamera() {};
   ~UCamera() {};

   void SetPosition(const FVector& _input);
   void SetDirection(const const float InputPitch, const float InputYaw,
                     const float InputRoll);
   FMatrix GetViewProjectionMatrix() const;

   void OnResize(int NewWidth, int NewHeight);
   void update();

   private:
   FMatrix GetViewMatrix() const;
   FMatrix GetProjectionMatrix() const;
};

