# Lua Prefs and Scoreboard

Lua에서 간단한 로컬 저장 기능을 사용할 수 있다.

- `Prefs`: Unity `PlayerPrefs`처럼 단일 값을 key-value로 저장한다.
- `Scoreboard`: 점수 기록을 누적 저장하고 랭킹으로 불러온다.

저장 파일은 프로젝트의 `Saved` 폴더 아래에 생성된다.

```text
Saved/PlayerPrefs.json
Saved/Scoreboard.json
```

## Prefs

`Prefs`는 옵션, 마지막 점수, 최고 점수 같은 단일 값을 저장할 때 사용한다.

### Number

```lua
Prefs.SetNumber("LastScore", 1200)

local lastScore = Prefs.GetNumber("LastScore", 0)
print("Last Score : ", lastScore)
```

### String

```lua
Prefs.SetString("PlayerName", "Player")

local name = Prefs.GetString("PlayerName", "Unknown")
print("Player Name : ", name)
```

### Bool

```lua
Prefs.SetBool("HasClearedTutorial", true)

local cleared = Prefs.GetBool("HasClearedTutorial", false)
print("Tutorial Cleared : ", cleared)
```

### HasKey / Clear

```lua
if Prefs.HasKey("LastScore") then
    print("LastScore exists")
end

-- 모든 Prefs 값을 지운다.
-- Prefs.Clear()
```

## Scoreboard

`Scoreboard`는 게임 결과를 누적 저장하고, 높은 점수 순으로 불러올 때 사용한다.

### AddScore

```lua
local hp = 80
local documentCount = 3
local timer = 125.0
local score = hp * documentCount

Scoreboard.AddScore("Player", score, hp, documentCount, timer)
```

저장되는 값:

- `name`
- `score`
- `hp`
- `documentCount`
- `timer`
- `timestamp`

### GetTopScores

```lua
local scores = Scoreboard.GetTopScores(5)

for i, entry in ipairs(scores) do
    print(
        entry.rank,
        entry.name,
        entry.score,
        entry.hp,
        entry.documentCount,
        entry.timer,
        entry.timestamp
    )
end
```

`GetTopScores(limit)`는 점수가 높은 순서대로 최대 `limit`개를 반환한다.

### Clear

```lua
-- 모든 스코어 기록을 지운다.
-- Scoreboard.Clear()
```

## Example: GameOver에서 점수 저장

```lua
local isGameOver = false

function GameOver(finalHP, finalDocumentCount)
    if isGameOver then
        return
    end

    isGameOver = true

    finalHP = finalHP or 0
    finalDocumentCount = finalDocumentCount or 0

    local score = finalHP * finalDocumentCount

    Prefs.SetNumber("LastScore", score)
    Scoreboard.AddScore("Player", score, finalHP, finalDocumentCount)

    print("GAME OVER")
    print("Score : ", score)
end
```

## Example: 스코어보드 출력

```lua
function ShowScoreboard()
    local scores = Scoreboard.GetTopScores(5)

    print("SCOREBOARD")

    for i, entry in ipairs(scores) do
        print(entry.rank .. ". " .. entry.name .. " / " .. entry.score)
    end
end
```

## Notes

- `Prefs`는 단일 값 저장용이다. 랭킹처럼 여러 기록이 필요한 경우 `Scoreboard`를 사용한다.
- `Scoreboard.AddScore`는 새 기록을 추가하고 저장 파일을 갱신한다.
- `Scoreboard.GetTopScores`는 파일에서 기록을 읽어 점수 내림차순으로 정렬해서 반환한다.
- 현재 스코어 계산은 게임 쪽 Lua에서 직접 한다. 예: `score = HP * DocumentCount`.
