function BeginPlay()
    print("world spawn test begin")
end

function OnKeyPressed(key)
    if key ~= "Enter" then
        return
    end

    local spawned = World.SpawnActor("Actor")

    if not spawned:IsValid() then
        print("spawn failed")
        return
    end

    spawned.Location = obj.Location + Vector.new(100, 0, 0)
    print("spawned actor", spawned.UUID)
end