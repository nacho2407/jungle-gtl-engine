local speed = 10

function BeginPlay()
end

function EndPlay()
end

function OnOverlap(OtherActor)
end

function Tick(dt)
    local dir = Vector.new(0, 0, 0)
 
    if Input.GetKey(Key.W) then
        dir.X = dir.X + 1
    end
    if Input.GetKey(Key.S) then
        dir.X = dir.X - 1
    end
    if Input.GetKey(Key.A) then
        dir.Y = dir.Y - 1
    end
    if Input.GetKey(Key.D) then
        dir.Y = dir.Y + 1
    end

    dir:Normalize()
    obj.Location = obj.Location + dir * speed * dt
end
