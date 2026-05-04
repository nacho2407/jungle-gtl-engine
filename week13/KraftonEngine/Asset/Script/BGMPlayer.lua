function BeginPlay()
    AudioManager.PlayBGM("CityBgm", 0.1)
end

function EndPlay()
    AudioManager.StopBGM()
end

function OnOverlap(OtherActor)
end

function Tick(dt)
end
