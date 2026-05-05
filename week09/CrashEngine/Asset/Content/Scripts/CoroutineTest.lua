local soundMgr = GetSoundManager()

function BeginPlay()
    
    StartCoroutine("MyFirstRoutine")
    
    StartCoroutine("WeatherRoutine")

    soundMgr:LoadSoundsFromDirectory("Sounds", false)
    soundMgr:PlaySFX("bubble")
end

function MyFirstRoutine()
    Wait(2.0) 
    
    MoveTo(obj, 10, 0);
    WaitUntilMoveDone(obj)
    
end

function WeatherRoutine()
    Wait(10.0)
end