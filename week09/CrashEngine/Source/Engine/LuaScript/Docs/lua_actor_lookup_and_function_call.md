# Lua Actor Lookup and Function Call

Lua 스크립트에서 다른 Actor를 태그로 찾고, 그 Actor에 붙은 `LuaScriptComponent`의 함수를 호출할 수 있다.

## API

### `World.FindActorByTag(tag)`

월드에서 `tag`를 가진 첫 번째 Actor를 찾아 `GameObject`로 반환한다.

```lua
local actor = World.FindActorByTag("GameManager")

if actor:IsValid() then
    print("found actor", actor.UUID)
end
```

찾지 못하면 invalid `GameObject`가 반환되므로 반드시 `IsValid()`를 확인한다.

### `Component:CallFunction(functionName, ...)`

`LuaScriptComponent`에 로드된 Lua 함수 이름을 찾아 호출한다. 추가 인자는 그대로 대상 Lua 함수에 전달된다.

```lua
local actor = World.FindActorByTag("GameManager")

if actor:IsValid() then
    local lua = actor:GetComponent("LuaScriptComponent", 0)

    if lua:IsValid() then
        lua:CallFunction("GameOver")
    end
end
```

`CallFunction`은 호출 성공 여부를 `true`/`false`로 반환한다.

```lua
local ok = lua:CallFunction("GameOver", 35.0, 2)

if not ok then
    print("failed to call GameOver")
end
```

## Example: Player Calls GameManager

`GameManager` Actor에 `GameManager` 태그를 붙이고, `GameManager.lua`가 붙은 `LuaScriptComponent`가 있다고 가정한다.

### `GameManager.lua`

```lua
local isGameOver = false

function GameOver(finalHP, finalDocumentCount)
    if isGameOver then
        return
    end

    isGameOver = true
    print("GAME OVER")
    print("Final HP : ", finalHP)
    print("Final Document Count : ", finalDocumentCount)
end
```

### `PlayerController.lua`

```lua
local HP = 100.0
local DocumentCount = 0

local function callGameOver()
    local gameManager = World.FindActorByTag("GameManager")

    if not gameManager:IsValid() then
        print("GameManager actor not found")
        return
    end

    local lua = gameManager:GetComponent("LuaScriptComponent", 0)

    if not lua:IsValid() then
        print("GameManager LuaScriptComponent not found")
        return
    end

    if not lua:CallFunction("GameOver", HP, DocumentCount) then
        print("GameOver call failed")
    end
end

function OnOverlapBegin(other)
    if other:HasTag("Destination") then
        callGameOver()
    end
end
```

## Notes

- `FindActorByTag`는 첫 번째로 발견한 Actor만 반환한다.
- 같은 태그를 여러 Actor가 공유하면 호출 대상이 명확하지 않을 수 있다.
- `CallFunction`은 `LuaScriptComponent` 전용이다. 다른 컴포넌트에서 호출하면 `false`를 반환한다.
- 호출할 함수는 대상 Lua 파일의 전역 함수로 정의되어 있어야 한다. 예: `function GameOver(...)`.
