local soundMgr = GetSoundManager()

function BeginPlay()
    print("스크립트 시작됨!")
    
    StartCoroutine("MyFirstRoutine")
    
    StartCoroutine("WeatherRoutine")

    soundMgr:LoadSoundsFromDirectory("Sounds", false)
    soundMgr:PlaySFX("bubble")
end

function MyFirstRoutine()
    print("1. 이동 전 대기...")
    Wait(2.0) 
    
    print("2. 이동 시작!")
    MoveTo(obj, 10, 0);
    WaitUntilMoveDone(obj)
    
    print("3. 코루틴 완료!")
end

function WeatherRoutine()
    Wait(10.0)
    print("10초 경과: 날씨가 흐려집니다.")
end