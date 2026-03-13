#pragma once

#include "UPrimitiveComponent.h"

class UAxisComp : public UPrimitiveComponent
{
 public:
  FColor XAxisColor{FColor(1.0f, 0.0f, 0.0f)};
  FColor YAxisColor{FColor(0.0f, 1.0f, 0.0f)};
  FColor ZAxisColor{FColor(0.0f, 0.0f, 1.0f)};

 public:
  UAxisComp();
  // UAxisComp(float _XAxisColor, float _YAxisColor, float _ZAxisColor);
  virtual ~UAxisComp();


};
