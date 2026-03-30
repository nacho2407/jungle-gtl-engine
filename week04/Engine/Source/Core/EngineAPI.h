#pragma once

#pragma warning(disable : 4251)

#if defined(ENGINECORE_EXPORTS) || defined(ENGINE_EXPORTS)
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif
