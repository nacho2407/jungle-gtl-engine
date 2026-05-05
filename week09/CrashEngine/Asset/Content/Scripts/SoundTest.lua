local soundMgr = GetSoundManager()

function BeginPlay()
    obj.Velocity = Vector.new(10.0, 0.0, 0.0)
    soundMgr:LoadSoundsFromDirectory("Sounds", false)
    soundMgr:PlaySFX("bubble")
end

function EndPlay()
end

function OnOverlap(OtherActor)
    

end

function Tick(dt)
    obj.Location = obj.Location + obj.Velocity * dt

end