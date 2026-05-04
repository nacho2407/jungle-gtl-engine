local isGameOver = false
local Timer = 0.0
local ScoreboardLimit = 5

local SpawnMaxCounts = {
    Battery = 3,
    Document = 3,
    Turret = 3
}

local CandidateSpawnInfos = {
    BatteryCandidate = {
        Tag = "Battery",
        MaxCount = SpawnMaxCounts.Battery,
        Mesh = "Asset/Content/Models/Battery/battery.obj",
        Rotation = Vector.new(90.0, 0.0, 0.0),
        Scale = Vector.new(10.0, 10.0, 10.0),
        Materials = {
            "Asset/Content/Materials/Battery/Material.002.json",
            "Asset/Content/Materials/Battery/Material.001.json",
            "None"
        },
        Collider = {
            Type = "BoxComponent",
            BoxExtent = Vector.new(1.5, 0.5, 0.5),
            Location = Vector.new(0.0, 0.0, 0.0),
            Rotation = Vector.new(0.0, 0.0, 0.0),
            Scale = Vector.new(1.0, 1.0, 1.0)
        }
    },
    DocumentCandidate = {
        Tag = "Document",
        MaxCount = SpawnMaxCounts.Document,
        Mesh = "Asset/Content/Models/Document/Document.obj",
        Rotation = Vector.new(0.0, -90.0, 0.0),
        Scale = Vector.new(3.0, 3.0, 3.0),
        Materials = {
            "Asset/Content/Materials/Document/A4_Page3Shape.json",
            "Asset/Content/Materials/Document/A4_Page2Shape.json",
            "Asset/Content/Materials/Document/A4_Page1Shape.json",
            "Asset/Content/Materials/Document/A4_Paper_StackShape.json",
            "Asset/Content/Materials/Document/Folder_1Shape.json"
        },
        Collider = {
            Type = "BoxComponent",
            BoxExtent = Vector.new(6.5, 2.7, 3.0),
            Location = Vector.new(0.0, 0.0, 0.0),
            Rotation = Vector.new(0.0, 0.0, 0.0),
            Scale = Vector.new(1.0, 1.0, 1.0)
        }
    },
    TurretCandidate = {
        Tag = "Turret",
        MaxCount = SpawnMaxCounts.Turret,
        Mesh = "Asset/Content/Models/Turret/Turret.obj",
        Rotation = Vector.new(90.0, 178.948853, 0.0),
        Scale = Vector.new(0.3, 0.3, 0.3),
        Materials = {
            "Asset/Content/Materials/T_smooth.json",
            "Asset/Content/Materials/T_Black.json",
            "Asset/Content/Materials/T_yellow.json",
            "Asset/Content/Materials/T_D_yellow.json",
            "Asset/Content/Materials/T_Grey.json"
        },
        Collider = {
            Type = "BoxComponent",
            BoxExtent = Vector.new(44.799999, 30.799999, 60.700001),
            Location = Vector.new(0.0, 0.0, 0.0),
            Rotation = Vector.new(0.0, 0.0, 0.0),
            Scale = Vector.new(1.0, 1.0, 1.0)
        }
    }
}

local bRandomSeeded = false

local function seedRandom()
    if bRandomSeeded then
        return
    end

    bRandomSeeded = true
    if os ~= nil and os.time ~= nil then
        math.randomseed(os.time())
    end
end

local function shuffleActors(actors)
    seedRandom()

    for i = #actors, 2, -1 do
        local j = math.random(i)
        actors[i], actors[j] = actors[j], actors[i]
    end
end

local function getOrAddStaticMeshComponent(actor)
    local mesh = actor:GetComponent("StaticMeshComponent", 0)

    if not mesh:IsValid() then
        mesh = actor:AddComponent("StaticMeshComponent")
    end

    return mesh
end

local function addCollider(actor, colliderInfo)
    if colliderInfo == nil then
        return true
    end

    local collider = actor:AddComponent(colliderInfo.Type)
    if not collider:IsValid() then
        return false
    end

    collider:SetRelativeLocation(colliderInfo.Location)
    collider:SetRelativeRotation(colliderInfo.Rotation)
    collider:SetRelativeScale(colliderInfo.Scale)
    collider:SetVisible(true)
    collider:SetVisibleInEditor(true)
    collider:SetVisibleInGame(true)

    if colliderInfo.Type == "BoxComponent" then
        collider:SetBoxExtent(colliderInfo.BoxExtent)
    elseif colliderInfo.Type == "CapsuleComponent" then
        collider:SetCapsuleSize(colliderInfo.CapsuleRadius, colliderInfo.CapsuleHalfHeight)
    end

    return true
end

local function applySpawnInfo(actor, spawnInfo)
    actor:AddTag(spawnInfo.Tag)

    local mesh = getOrAddStaticMeshComponent(actor)
    if not mesh:IsValid() then
        return false
    end

    mesh:SetStaticMesh(spawnInfo.Mesh)
    mesh:SetRelativeRotation(spawnInfo.Rotation)
    mesh:SetRelativeScale(spawnInfo.Scale)

    for i, material in ipairs(spawnInfo.Materials) do
        mesh:SetMaterial(i - 1, material)
    end

    if not addCollider(actor, spawnInfo.Collider) then
        return false
    end

    return true
end

local function replaceCandidate(candidate, spawnInfo)
    local spawned = World.SpawnActor("StaticMeshActor")

    if not spawned:IsValid() then
        return false
    end

    local spawnLocation = candidate.Location
    if not applySpawnInfo(spawned, spawnInfo) then
        World.DestroyActor(spawned)
        return false
    end

    spawned.Location = spawnLocation
    World.DestroyActor(candidate)
    return true
end

local function replaceCandidates()
    if World.FindActorsByTag == nil then
        print("World.FindActorsByTag is not registered")
        return
    end

    for candidateTag, spawnInfo in pairs(CandidateSpawnInfos) do
        local candidates = World.FindActorsByTag(candidateTag)
        shuffleActors(candidates)

        local spawnCount = math.min(#candidates, spawnInfo.MaxCount)
        for i = 1, spawnCount do
            replaceCandidate(candidates[i], spawnInfo)
        end
    end
end

local function getPlayerHP()
    if _G.PlayerState == nil then
        return 0
    end

    return _G.PlayerState.HP or 0
end

local function getDocumentCount()
    if _G.PlayerState == nil then
        return 0
    end

    return _G.PlayerState.DocumentCount or 0
end

function GameOver(finalHP, finalDocumentCount)
    if isGameOver then
        return
    end

    isGameOver = true
    finalHP = finalHP or getPlayerHP()
    finalDocumentCount = finalDocumentCount or getDocumentCount()
    local score = finalHP * finalDocumentCount

    Prefs.SetNumber("LastScore", score)
    Scoreboard.AddScore("Player", score, finalHP, finalDocumentCount)

    print("GAME OVER")
    print("Score : ", score)
    print("Final HP : ", finalHP)
    print("Final Document Count : ", finalDocumentCount)
    print("Timer : ", Timer)
    ShowScoreboard(ScoreboardLimit)
end

function ShowScoreboard(limit)
    limit = limit or ScoreboardLimit
    local scores = Scoreboard.GetTopScores(limit)

    print("SCOREBOARD")
    for i, entry in ipairs(scores) do
        print(entry.rank, entry.name, entry.score, entry.hp, entry.documentCount, entry.timestamp)
    end
end

function BeginPlay()
    _G.GameManager = {
        GameOver = GameOver,
        GetPlayerHP = getPlayerHP,
        GetDocumentCount = getDocumentCount
    }

    print("[BeginPlay] " .. obj.UUID)
    obj:PrintLocation()
    replaceCandidates()
end

function EndPlay()
    if _G.GameManager ~= nil and _G.GameManager.GameOver == GameOver then
        _G.GameManager = nil
    end

    print("[EndPlay] " .. obj.UUID)
    obj:PrintLocation()
end

function OnOverlap(OtherActor)
    OtherActor:PrintLocation();
end

function Tick(dt)
    Timer = Timer + dt
end
