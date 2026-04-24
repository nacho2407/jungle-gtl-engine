// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"

// Windows API
#define NOMINMAX
#include <Windows.h>
#include <windowsx.h>

// D3D API
#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "dxgi")

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_5.h>
