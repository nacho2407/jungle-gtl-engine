local soundMgr = GetSoundManager()

function BeginPlay()
    print("[BeginPlay] " .. obj.UUID)
    obj:PrintLocation()
    obj.Velocity = Vector.new(10.0, 0.0, 0.0)
    soundMgr:LoadSoundsFromDirectory("Sounds", false)
    soundMgr:PlaySFX("bubble")
end

function EndPlay()
    print("[EndPlay] " .. obj.UUID)
    obj:PrintLocation()
end

function OnOverlap(OtherActor)
    OtherActor:PrintLocation();
    

end

function Tick(dt)
    obj.Location = obj.Location + obj.Velocity * dt

end