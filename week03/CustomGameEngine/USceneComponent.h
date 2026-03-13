#pragma once

#include "UObject.h"
#include "math/FVector4.h"
#include "FVector.h"
#include "FMatrix.h"
#include "FVertexSimple.h"
#include "FColor.h"

class USceneComponent : public UObject {
 public:
  FVector RelativeLocation;
  FVector RelativeRotation;
  FVector RelativeScale3D;

  public:
  USceneComponent();
   virtual ~USceneComponent();
};