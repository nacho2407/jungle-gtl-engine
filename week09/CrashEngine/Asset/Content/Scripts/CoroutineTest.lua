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
    wait(2.0) 
    
    print("2. 이동 시작!")
    move_to(obj, 10, 0);
    wait_until_move_done(obj)
    
    print("3. 코루틴 완료!")
end

function WeatherRoutine()
    wait(10.0)
    print("10초 경과: 날씨가 흐려집니다.")
end