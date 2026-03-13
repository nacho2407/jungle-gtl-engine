#pragma once



class USceneComponent : public UObject {
 public:
  FVector RelativeLocation;
  FVector RelativeRotation;
  FVector RelativeScale3D;
};