local soundMgr = GetSoundManager()

function BeginPlay()
    print("[System] BeginPlay")
    
    -- 다중 인자 코루틴 테스트
    StartCoroutine("PatrolRoutine", obj, 15, "순찰 테스트", 3)
    StartCoroutine("GreetingRoutine", "Hello World", 2.5)
end

function PatrolRoutine(target, step, msg, maxLoop)
    print("[Patrol] Start. Msg: " .. msg)
    
    for i = 1, maxLoop do
        local nextX = step * i
        print("[Patrol] MoveTo X: " .. nextX)
        
        MoveTo(target, nextX, 0)
        WaitUntilMoveDone(target)
        
        -- 이동 후 1초 대기
        Wait(1.0)
    end
    
    print("[Patrol] Done")
end

function GreetingRoutine(msg, delay)
    print("[Greeting] Wait " .. delay .. " sec...")
    Wait(delay)
    print("[Greeting] " .. msg)
end