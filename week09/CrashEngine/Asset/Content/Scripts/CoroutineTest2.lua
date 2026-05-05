local soundMgr = GetSoundManager()

function BeginPlay()
    
    -- 다중 인자 코루틴 테스트
    StartCoroutine("PatrolRoutine", obj, 15, "순찰 테스트", 3)
    StartCoroutine("GreetingRoutine", "Hello World", 2.5)
end

function PatrolRoutine(target, step, msg, maxLoop)
    
    for i = 1, maxLoop do
        local nextX = step * i
        
        MoveTo(target, nextX, 0)
        WaitUntilMoveDone(target)
        
        -- 이동 후 1초 대기
        Wait(1.0)
    end
    
end

function GreetingRoutine(msg, delay)
    Wait(delay)
end