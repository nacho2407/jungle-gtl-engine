#pragma once

#ifndef SOL_ALL_SAFETIES_ON
#define SOL_ALL_SAFETIES_ON 1
#endif

#ifndef SOL_LUAJIT
#define SOL_LUAJIT 1
#endif

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <luajit.h>
}

#include <sol/sol.hpp>
