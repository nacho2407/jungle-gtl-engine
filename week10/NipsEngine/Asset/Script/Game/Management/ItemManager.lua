local ItemManager = {}
ItemManager.__index = ItemManager

local ItemSpawnInfos = {
    {
        name = "HillPack",
        prefab = "Asset/Prefab/Item/HillPack"
    },
    {
        name = "TimeSlow",
        prefab = "Asset/Prefab/Item/TimeSlow"
    }
}

function ItemManager.new(context)
    return setmetatable({
        context = context,
        spawnInterval = 10.0,
        elapsed = 0.0,
        itemPoints = {},
        activeItems = {}
    }, ItemManager)
end

function ItemManager:BeginPlay()
    self:CacheItemPoints()

    self.context.eventBus:Subscribe("Game.Started", self, function()
        self.elapsed = 0.0
        self.activeItems = {}
        self:CacheItemPoints()
    end)

    self.context.eventBus:Subscribe("Game.Finished", self, function()
        self:ClearSpawnedItems()
    end)

    self.context.eventBus:Subscribe("Game.Canceled", self, function()
        self:ClearSpawnedItems()
    end)
end

function ItemManager:EndPlay()
    self.context.eventBus:ClearOwner(self)
    self:ClearSpawnedItems()
end

function ItemManager:CacheItemPoints()
    self.itemPoints = {}

    local actors = Engine.API.World.FindActorsByTag("ItemPoint")
    if actors == nil then
        Log("[ItemManager] ItemPoint lookup failed")
        return
    end

    for _, actor in ipairs(actors) do
        if actor ~= nil then
            table.insert(self.itemPoints, actor)
        end
    end

    Log("[ItemManager] Cached ItemPoint Count: " .. tostring(#self.itemPoints))
end

function ItemManager:GetRandomPoint(excludedIndex)
    local count = #self.itemPoints
    if count <= 0 then
        return nil, nil
    end

    if count == 1 then
        return self.itemPoints[1], 1
    end

    local index = Engine.API.Random.RandomInt(1, count)
    if excludedIndex ~= nil and index == excludedIndex then
        index = (index % count) + 1
    end

    return self.itemPoints[index], index
end

function ItemManager:SpawnItem(info, point)
    if info == nil or point == nil then
        return nil
    end

    local actor = Engine.API.World.SpawnActorFromPrefab(info.prefab)
    if actor == nil then
        Log("[ItemManager] Spawn failed: " .. tostring(info.prefab))
        return nil
    end

    actor.Location = point.Location + Vector(0.0, 0.0, 1.0)
    actor.Rotation = Vector(0.0, 0.0, Engine.API.Random.RandomFloat(0.0, 360.0))

    table.insert(self.activeItems, actor)
    Log("[ItemManager] Spawned " .. tostring(info.name))
    return actor
end

function ItemManager:SpawnWave()
    if #self.itemPoints <= 0 then
        self:CacheItemPoints()
    end

    if #self.itemPoints <= 0 then
        Log("[ItemManager] No ItemPoint")
        return
    end

    local itemCount = #ItemSpawnInfos
    if itemCount <= 0 then
        Log("[ItemManager] No item prefab info")
        return
    end

    local itemIndex = Engine.API.Random.RandomInt(1, itemCount)
    local point = self:GetRandomPoint(nil)
    self:SpawnItem(ItemSpawnInfos[itemIndex], point)
end

function ItemManager:PruneInvalidItems()
    local nextItems = {}
    for _, actor in ipairs(self.activeItems) do
        if actor ~= nil and Engine.API.World.IsValidActor(actor) then
            table.insert(nextItems, actor)
        end
    end
    self.activeItems = nextItems
end

function ItemManager:ClearSpawnedItems()
    for _, actor in ipairs(self.activeItems) do
        if actor ~= nil and Engine.API.World.IsValidActor(actor) then
            Engine.API.World.DestroyActor(actor)
        end
    end

    self.activeItems = {}
end

function ItemManager:Tick(dt)
    local game = self.context.managers.Game
    if game == nil or not game.isRunning or game.isPaused then
        return
    end

    local realDt = Engine.API.World.GetUnscaledDeltaTime()
    if realDt <= 0.0 then
        realDt = dt or 0.0
    end

    self.elapsed = self.elapsed + realDt
    if self.elapsed >= self.spawnInterval then
        self.elapsed = 0.0
        self:SpawnWave()
        self:PruneInvalidItems()
    end
end

return ItemManager
