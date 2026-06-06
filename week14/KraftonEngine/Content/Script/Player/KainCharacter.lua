local runtimeReadyToken = nil

function BeginPlay()
    Game.Log("KainCharacter BeginPlay", obj.Name)

    runtimeReadyToken = Game.On("RuntimeReady", function(context)
        Game.Log("KainCharacter RuntimeReady")
    end)
end

function EndPlay()
    if runtimeReadyToken ~= nil then
        Game.Off(runtimeReadyToken)
        runtimeReadyToken = nil
    end
end

function Tick(dt)
    -- 이동과 mouse look은 ACharacter 기본 입력 경로가 처리합니다.
end
