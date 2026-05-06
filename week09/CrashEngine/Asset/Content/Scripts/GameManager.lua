-- GameManager는 게임 전체 진행 상태와 시작 시 오브젝트 배치를 담당한다.
-- Player/Turret처럼 매 프레임 직접 움직이는 Actor가 아니라, 월드 규칙을 관리하는 스크립트다.
local isGameOver = false
local Timer = 0.0
local game_over_transition_elapsed = 0.0
local game_over_transition_duration = 3.0
local game_over_scene_requested = false
local game_over_slomo_scale = 0.3

-- Candidate가 실제 아이템/터렛으로 바뀔 때 종류별 최대 생성 수.
-- Scene에 Candidate가 더 많아도 여기 숫자까지만 랜덤으로 선택된다.
local SpawnMaxCounts = {
    Battery = 4,
    Document = 4,
    Turret = 12
}

-- Key는 Scene에서 찾을 Candidate Tag, Value는 바꿔치기할 실제 Actor 설정이다.
-- Mesh/Material/Collider/Script를 Lua에서 조립해서 Scene 배치 데이터를 gameplay Actor로 바꾼다.
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
        Rotation = Vector.new(0.0, 0.0, 0.0),
        Scale = Vector.new(0.3, 0.3, 0.3),
        -- Turret은 배치 방향이 gameplay에 의미가 있으므로 Candidate Actor의 회전을 그대로 받는다.
        UseCandidateRotation = true,
        Materials = {
            "Asset/Content/Materials/T_smooth.json",
            "Asset/Content/Materials/T_Black.json",
            "Asset/Content/Materials/T_yellow.json",
            "Asset/Content/Materials/T_D_yellow.json",
            "Asset/Content/Materials/T_Grey.json"
        },
        Script = "Turret.lua",
        Collider = {
            Type = "BoxComponent",
            BoxExtent = Vector.new(44.799999, 30.799999, 60.0),
            Location = Vector.new(0.0, 0.0, 0.0),
            Rotation = Vector.new(0.0, 0.0, 0.0),
            Scale = Vector.new(1.0, 1.0, 1.0)
        }
    }
}

local bRandomSeeded = false

-- Lua의 math.random은 seed가 같으면 같은 순서가 나오므로, 게임 시작 후 한 번만 seed를 바꾼다.
local function seedRandom()
    if bRandomSeeded then
        return
    end

    bRandomSeeded = true
    if os ~= nil and os.time ~= nil then
        math.randomseed(os.time())
    end
end

-- Fisher-Yates 방식으로 Actor 배열을 섞는다.
-- Candidate 중 일부만 랜덤으로 선택하기 위해 replaceCandidates에서 사용한다.
local function shuffleActors(actors)
    seedRandom()

    for i = #actors, 2, -1 do
        local j = math.random(i)
        actors[i], actors[j] = actors[j], actors[i]
    end
end

-- StaticMeshActor는 보통 RootComponent가 StaticMeshComponent지만,
-- Lua에서 새로 만든 Actor라 없을 수도 있어서 없으면 붙인다.
local function getOrAddStaticMeshComponent(actor)
    local mesh = actor:GetComponent("StaticMeshComponent", 0)

    if not mesh:IsValid() then
        mesh = actor:AddComponent("StaticMeshComponent")
    end

    return mesh
end

-- spawnInfo.Collider 테이블을 실제 엔진 Collider Component로 변환한다.
-- 지금은 Box/Capsule을 지원하고, 필요하면 Sphere 등도 여기에 추가하면 된다.
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

-- Tag, Mesh, Material, Collider처럼 Candidate 종류별 공통 생성 정보를 Actor에 적용한다.
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

-- Turret처럼 개별 LuaScriptComponent가 필요한 Actor에만 스크립트를 붙인다.
local function attachScript(actor, scriptPath)
    if scriptPath == nil then
        return true
    end

    local script = actor:AddComponent("LuaScriptComponent")
    if not script:IsValid() then
        return false
    end

    return script:SetScriptPath(scriptPath)
end

-- Candidate Actor 하나를 실제 gameplay Actor 하나로 교체한다.
-- 위치는 항상 Candidate에서 가져오고, 회전은 spawnInfo.UseCandidateRotation일 때만 가져온다.
local function replaceCandidate(candidate, spawnInfo)
    local spawned = World.SpawnActor("StaticMeshActor")

    if not spawned:IsValid() then
        return false
    end

    local spawnLocation = candidate.Location
    local spawnRotation = nil
    if spawnInfo.UseCandidateRotation then
        spawnRotation = candidate.Rotation
    end
    if not applySpawnInfo(spawned, spawnInfo) then
        World.DestroyActor(spawned)
        return false
    end

    spawned.Location = spawnLocation
    if spawnInfo.UseCandidateRotation then
        spawned.Rotation = spawnRotation
    end

    if not attachScript(spawned, spawnInfo.Script) then
        World.DestroyActor(spawned)
        return false
    end

    World.DestroyActor(candidate)
    return true
end

-- BeginPlay에서 한 번 호출되어 Scene의 Candidate들을 실제 아이템/터렛으로 바꾼다.
local function replaceCandidates()
    if World.FindActorsByTag == nil then
        return
    end

    for candidateTag, spawnInfo in pairs(CandidateSpawnInfos) do
        local candidates = World.FindActorsByTag(candidateTag)
        shuffleActors(candidates)

        local spawnCount = math.min(#candidates, spawnInfo.MaxCount)
        for i, candidate in ipairs(candidates) do
            if i <= spawnCount then
                if not replaceCandidate(candidate, spawnInfo) and candidate:IsValid() then
                    World.DestroyActor(candidate)
                end
            elseif candidate:IsValid() then
                World.DestroyActor(candidate)
            end
        end
    end
end

-- PlayerController.lua가 _G.PlayerState에 올려둔 HP를 읽는다.
local function getPlayerHP()
    if _G.PlayerState == nil then
        return 0
    end

    return _G.PlayerState.HP or 0
end

-- PlayerController.lua가 _G.PlayerState에 올려둔 문서 획득 수를 읽는다.
local function getDocumentCount()
    if _G.PlayerState == nil then
        return 0
    end

    return _G.PlayerState.DocumentCount or 0
end

local function getBatteryCount()
    if _G.PlayerState == nil then
        return 0
    end

    return _G.PlayerState.BatteryCount or 0
end

local function getEnemyKillCount()
    if _G.PlayerState == nil then
        return 0
    end

    return _G.PlayerState.EnemyKillCount or 0
end

local function getTimer()
    return Timer
end


-- PlayerController 또는 다른 시스템에서 호출할 수 있는 게임 종료 진입점.
local function calculateScore(finalHP, enemyKillCount, documentCount, elapsedTime, batteryCount)
    if (finalHP or 0) <= 0 then
        return 0
    end

    local enemy_score = math.max(0, enemyKillCount or 0) * 1000
    local document_score = math.max(0, documentCount or 0) * 3000
    local time_penalty = math.floor((elapsedTime or 0) * 10)
    local battery_score = math.max(0, batteryCount or 0) * 300

    return math.max(0, math.floor(enemy_score + document_score - time_penalty + battery_score))
end

function GameOver(finalHP, finalDocumentCount)
    if isGameOver then
        return
    end

    isGameOver = true
    finalHP = finalHP or getPlayerHP()
    finalDocumentCount = finalDocumentCount or getDocumentCount()
    local score = calculateScore(finalHP, getEnemyKillCount(), finalDocumentCount, Timer, getBatteryCount())

    Prefs.SetNumber("LastScore", score)
    Prefs.SetNumber("LastHP", finalHP)
    Prefs.SetNumber("LastDocumentCount", finalDocumentCount)
    Prefs.SetNumber("LastTimer", Timer)

    game_over_transition_elapsed = 0.0
    game_over_scene_requested = false
    World.RequestSlomo(game_over_slomo_scale, game_over_transition_duration)
end

function BeginPlay()
    -- _G는 Lua 전역 테이블이다. 다른 Lua 파일에서 _G.GameManager로 이 함수들에 접근할 수 있다.
    _G.GameManager = {
        GameOver = GameOver,
        GetPlayerHP = getPlayerHP,
        GetDocumentCount = getDocumentCount,
        GetBatteryCount = getBatteryCount,
        GetEnemyKillCount = getEnemyKillCount,
        GetTimer = getTimer
    }

    World.PlayCameraEffectAsset("Asset/Content/CameraEffects/FadeIn.ceffect")
    local sound_mgr = GetSoundManager()
    replaceCandidates()
end

function EndPlay()
    local sound_mgr = GetSoundManager()
    if sound_mgr ~= nil and sound_mgr:IsInitialized() then
        sound_mgr:StopBGM()
        sound_mgr:StopAllSFX()
    end

    -- PIE 종료/월드 종료 시 전역 참조가 오래 살아남지 않도록 정리한다.
    if _G.GameManager ~= nil and _G.GameManager.GameOver == GameOver then
        _G.GameManager = nil
    end

end

function OnOverlap(OtherActor)
end

function Tick(dt)
    -- dt는 이전 프레임부터 현재 프레임까지 걸린 시간이다.
    local unscaled_dt = World.GetUnscaledDeltatime()

    if isGameOver then
        if not game_over_scene_requested then
            game_over_transition_elapsed = game_over_transition_elapsed + unscaled_dt
            if game_over_transition_elapsed >= game_over_transition_duration then
                game_over_scene_requested = true
                LoadScene("GameOver.Scene")
            end
        end

        return
    end

    Timer = Timer + unscaled_dt
end
