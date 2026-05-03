function OnKeyPressed(key)
    if key ~= "Enter" then
        return
    end

    local actor = World.SpawnActor("Actor")

    if not actor:IsValid() then
        print("spawn failed")
        return
    end

    actor.Location = obj.Location + Vector.new(150, 0, 0)

    local mesh = actor:AddComponent("StaticMeshComponent")

    if mesh:IsValid() then
        mesh:SetStaticMesh("Asset/Content/Models/_Basic/Sphere.obj")
        mesh:SetRelativeScale(Vector.new(2, 2, 2))
        print("spawned actor with mesh", actor.UUID, mesh.UUID)
    end
end