function BeginPlay()
    print("[Lua] BeginPlay", obj.UUID)
    obj:PrintLocation()
end

function Tick(dt)
    obj.Location = obj.Location + obj.Velocity * dt
end

function EndPlay()
    print("[Lua] EndPlay", obj.UUID)
    obj:PrintLocation()
end

function OnKeyPressed(key)
    print("[Lua] KeyPressed", key, obj.UUID)

    if key == "D" then
        obj.Velocity = Vector.new(0.0, 10.0, 0.0)
    elseif key == "A" then
        obj.Velocity = Vector.new(0.0, -10.0, 0.0)
    elseif key == "W" then
        obj.Velocity = Vector.new(10.0, 0.0, 0.0)
    elseif key == "S" then
        obj.Velocity = Vector.new(-10.0, 0.0, 0.0)
    end
end
