#pragma once
#include "EngineAPI.h"

class FEngine;
ENGINE_API extern FEngine* GEngine;

/**
 * 자주 바뀌지 않는 헤더 파일만 넣어둘 것 (빌드 시간을 줄이기 위해).
 */
#include "Types/CoreTypes.h"
#include "Types/Array.h"
#include "Types/LinkedList.h"
#include "Types/Map.h"
#include "Types/Pair.h"
#include "Types/Queue.h"
#include "Types/Set.h"
#include "Types/StaticArray.h"
#include "Types/String.h"
#include "Types/ObjectPtr.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "Math/Transform.h"
#include "Math/Vector2.h"

