-- InputSmokeTest.lua

function Tick(DeltaTime)
    if Input:IsKeyDown("W") then
        print("W down")
    end

    if Input:WasKeyPressed("Space") then
        print("Space pressed")
    end

    if Input:WasKeyReleased("Space") then
        print("Space released")
    end
end