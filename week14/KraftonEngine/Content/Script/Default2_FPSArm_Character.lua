local arms   = nil
local camera = nil
local weaponHudWidget = nil

-- 본 기반 머즐은 Arm_R 이 어깨 관절이라 카메라랑 거의 같은 위치 → 의미 없음.
-- 대부분 FPS 는 카메라에서 forward offset 으로 머즐을 근사함.
local MUZZLE_FWD_OFFSET = 0.4   -- 카메라 앞 0.4m 지점을 머즐로 간주

local MAX_RANGE        = 200.0
local FIRE_INTERVAL    = 0.12
local MAGAZINE_SIZE    = 50
local RELOAD_DURATION  = 1.1

local MAX_HITS_FOR_FULL_RED = 8

local fireCooldown = 0.0
local reloadTimer  = 0.0
local currentAmmo  = MAGAZINE_SIZE
local isReloading  = false
local hitCounts    = {}

local function get_anim_requests()
    if obj == nil then return nil end

    _G.FPSArmAnimRequests = _G.FPSArmAnimRequests or {}

    local ownerId = obj.UUID
    local requests = _G.FPSArmAnimRequests[ownerId]
    if requests == nil then
        requests = {}
        _G.FPSArmAnimRequests[ownerId] = requests
    end

    return requests
end

local function request_arm_animation(name)
    local requests = get_anim_requests()
    if requests == nil then return end

    requests[name] = true
end

local function update_weapon_hud()
    if weaponHudWidget == nil then return end

    weaponHudWidget:SetText("weapon-hud-current-ammo", tostring(currentAmmo))
    weaponHudWidget:SetText("weapon-hud-magazine-ammo", tostring(MAGAZINE_SIZE))
end

local function finish_reload()
    currentAmmo = MAGAZINE_SIZE
    reloadTimer = 0.0
    isReloading = false
    update_weapon_hud()
end

local function start_reload()
    if arms == nil or isReloading or currentAmmo >= MAGAZINE_SIZE then return end

    isReloading = true
    reloadTimer = RELOAD_DURATION
    request_arm_animation("reload")
end

local function lerp_red(t)
    if t > 1.0 then t = 1.0 end
    -- 옅은 빨강 (255,180,180) → 진한 빨강 (180,0,0)
    local r = math.floor(255 - (255 - 180) * t)
    local g = math.floor(180 * (1 - t))
    local b = math.floor(180 * (1 - t))
    return r, g, b
end

local function register_hit(hitActor, hitLoc)
    local id = hitActor.UUID
    local n  = (hitCounts[id] or 0) + 1
    hitCounts[id] = n
    local r, g, b = lerp_red(n / MAX_HITS_FOR_FULL_RED)
    Debug.DrawSphere(hitLoc, 0.08, r, g, b, 9999.0, 10)
end

local function try_shoot()
    if camera == nil or arms == nil then return end
    if isReloading then return end
    if currentAmmo <= 0 then
        start_reload()
        return
    end

    currentAmmo = currentAmmo - 1
    update_weapon_hud()

    request_arm_animation("shoot")

    local camPos = camera:GetWorldLocation()
    local camFwd = camera:GetForwardVector()

    -- 카메라 + forward offset 을 머즐로 간주 (시안색 시각화).
    local muzzleWorld = Vector.new(
        camPos.X + camFwd.X * MUZZLE_FWD_OFFSET,
        camPos.Y + camFwd.Y * MUZZLE_FWD_OFFSET,
        camPos.Z + camFwd.Z * MUZZLE_FWD_OFFSET)
    Debug.DrawSphere(muzzleWorld, 0.04, 0, 200, 255, 1.0, 8)

    -- 카메라 → 머즐 사이에 벽이 있으면 (= 총구가 벽 안으로 박힘) 시작점 클램프.
    local rayStart = muzzleWorld
    local obstrHit = World.RaycastWorldStatic(camPos, camFwd, MUZZLE_FWD_OFFSET, obj)
    if obstrHit ~= nil then
        rayStart = obstrHit.WorldHitLocation
        Debug.DrawSphere(rayStart, 0.05, 255, 230, 0, 1.0, 8)
    end

    -- 벽 우선: 카메라 forward 방향으로 WorldStatic 트레이스 → 닿는 곳까지만 SkeletalMesh 트레이스.
    local effRange = MAX_RANGE
    local wallHit  = World.RaycastWorldStatic(rayStart, camFwd, MAX_RANGE, obj)
    if wallHit ~= nil then
        effRange = wallHit.Distance
    end

    local hit = World.RaycastSkeletalMesh(rayStart, camFwd, effRange, obj)

    local endPos
    if hit ~= nil then
        endPos = hit.WorldHitLocation
    elseif wallHit ~= nil then
        endPos = wallHit.WorldHitLocation
    else
        endPos = Vector.new(
            rayStart.X + camFwd.X * MAX_RANGE,
            rayStart.Y + camFwd.Y * MAX_RANGE,
            rayStart.Z + camFwd.Z * MAX_RANGE)
    end
    Debug.DrawLine(rayStart, endPos, 0, 255, 0, 1.5)

    if hit ~= nil and hit.HitActor ~= nil then
        register_hit(hit.HitActor, hit.WorldHitLocation)
    end
end

function BeginPlay()
    arms   = obj:GetSkeletalMesh()
    camera = obj:GetCamera()
    currentAmmo = MAGAZINE_SIZE
    reloadTimer = 0.0
    isReloading = false
    fireCooldown = 0.0
    local requests = get_anim_requests()
    if requests ~= nil then
        requests.shoot = false
        requests.reload = false
    end

    weaponHudWidget = UI.CreateWidget("Content/UI/HUD/WeaponHUD.rml")
    if weaponHudWidget ~= nil then
        weaponHudWidget:AddToViewportZ(80)
        update_weapon_hud()
    end
end

function EndPlay()
    if obj ~= nil and _G.FPSArmAnimRequests ~= nil then
        _G.FPSArmAnimRequests[obj.UUID] = nil
    end

    if weaponHudWidget ~= nil and weaponHudWidget:IsInViewport() then
        weaponHudWidget:RemoveFromParent()
    end
    weaponHudWidget = nil
end

function Tick(dt)
    if arms == nil then return end

    if fireCooldown > 0 then
        fireCooldown = fireCooldown - dt
    end

    if isReloading then
        reloadTimer = reloadTimer - dt
        if reloadTimer <= 0 then
            finish_reload()
        end
    end

    if Input.GetKeyDown(Key.R) then
        start_reload()
    end

    if Input.GetKey(Key.MouseLeft) and fireCooldown <= 0 then
        try_shoot()
        fireCooldown = FIRE_INTERVAL
    end
end
