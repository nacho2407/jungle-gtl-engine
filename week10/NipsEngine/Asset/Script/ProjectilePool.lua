local ProjectilePool = {
    Inactive = {},
    Active = {},
    Registered = {},
}
ProjectilePool.__index = ProjectilePool

ProjectilePool = ProjectilePool or {
    Inactive = {},
    Active = {},
    Registered = {},
}

function ProjectilePool:Register(projectile)
    if projectile == nil then
        return
    end

    if self.Registered[projectile] then
        return
    end

    self.Registered[projectile] = true
    projectile.bActive = false

    table.insert(self.Inactive, projectile)
end

function ProjectilePool:Acquire()
    while #self.Inactive > 0 do
        local projectile = table.remove(self.Inactive)

        if projectile ~= nil and not projectile.bDestroyed then
            self.Active[projectile] = true
            return projectile
        end
    end

    LogWarning("[ProjectilePool] Pool is empty")
    return nil
end

function ProjectilePool:Spawn(spawnLocation, targetActor, prefabPath)
    local projectile = self:Acquire()

    -- 풀이 비어있으면 새로 생성
    if projectile == nil then
        if prefabPath == nil then
            LogWarning("[ProjectilePool] Pool is empty and no prefabPath provided")
            return nil
        end

        Log("[ProjectilePool] Pool empty, creating new projectile")

        local newActor = Engine.API.World.SpawnActorFromPrefab(prefabPath)

        if newActor == nil then
            LogWarning("[ProjectilePool] SpawnActorFromPrefab failed: " .. prefabPath)
            return nil
        end

        -- BeginPlay에서 Register → Inactive에 자동 등록됨
        -- 바로 Acquire로 꺼냄
        projectile = self:Acquire()

        if projectile == nil then
            LogWarning("[ProjectilePool] Acquire failed after spawn")
            return nil
        end
    end

    projectile:Activate(spawnLocation, targetActor)
    return projectile
end

function ProjectilePool:Release(projectile)
    if projectile == nil then return end
    if not projectile.bActive then return end

    projectile.bActive = false
    projectile.bDestroyed = false  -- ← 이 줄 추가
    projectile.Elapsed = 0.0

    self.Active[projectile] = nil
    table.insert(self.Inactive, projectile)

    projectile:SetPooledActive(false)
end

function ProjectilePool:GetActiveCount()
    local count = 0

    for _ in pairs(self.Active) do
        count = count + 1
    end

    return count
end

function ProjectilePool:GetInactiveCount()
    return #self.Inactive
end
return ProjectilePool