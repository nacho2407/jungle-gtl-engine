function BeginPlay()
    print("[Lua] BeginPlay", obj.UUID)
    obj.Velocity = Vector.new(10.0, 0.0, 0.0)
    obj:PrintLocation()
end

function Tick(dt)
    obj.Location = obj.Location + obj.Velocity * dt
end

function EndPlay()
    print("[Lua] EndPlay", obj.UUID)
    obj:PrintLocation()
end
