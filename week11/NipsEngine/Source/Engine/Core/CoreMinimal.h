#pragma once

//=============================================================================
// Platform Types
// Basic integer and platform-dependent types.
// Example: int32, uint32, TCHAR.
//=============================================================================
#include "CoreTypes.h"

//=============================================================================
// Containers
// Core containers and string types.
//=============================================================================
#include "Containers/Array.h"
#include "Containers/LinkedList.h"
#include "Containers/Map.h"
#include "Containers/Pair.h"
#include "Containers/Queue.h"
#include "Containers/Set.h"
#include "Containers/StaticArray.h"
#include "Containers/String.h"

//=============================================================================
// Math
// Core math types and utilities.
// Vector, matrix, rotation, transform, color, etc.
//=============================================================================
#include "Math/Color.h"
#include "Math/Utils.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "Math/Vector2.h"

//=============================================================================
// Geometry
// 기하 프리미티브 및 기하 유틸리티
// AABB, Ray, Segment, Triangle, 교차 판정, Bounds 보조 함수 등
//=============================================================================
#include "Engine/Geometry/Transform.h"
#include "Engine/Geometry/AABB.h"
#include "Engine/Geometry/Ray.h"
#include "Engine/Geometry/Plane.h"
#include "Engine/Geometry/Triangle.h"
#include "Engine/Geometry/Frustum.h"
#include "Engine/Geometry/Edge.h"