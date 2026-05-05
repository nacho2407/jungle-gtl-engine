function BeginPlay()
end

function OnKeyPressed(key)
    if key ~= "Enter" then
        return
    end

    local spawned = World.SpawnActor("Actor")

    if not spawned:IsValid() then
        return
    end

    spawned.Location = obj.Location + Vector.new(100, 0, 0)
end