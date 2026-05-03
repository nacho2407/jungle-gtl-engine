local ObjRegistry = require("ObjRegistry")

local car = nil
local movement = nil
local startLocationActor = nil   -- Tag = "StartLocation" 인 액터 — R 키 리스폰 기준점.

function BeginPlay()
    car = obj:AsCarPawn()
    if car == nil then
        return
    end

    ObjRegistry.RegisterCar(car)
    movement = car:GetCarMovement()

    -- 시작 위치 액터를 1회 lookup. 매 frame 재검색하지 않도록 캐시 — World.FindFirstActorByTag
    -- 가 actors 선형 스캔이라 비싸진 않지만, 주기적 호출은 피한다.
    startLocationActor = World.FindFirstActorByTag("StartLocation")
end

local function ResetCarToStart()
    if car == nil or movement == nil then return end
    if startLocationActor == nil or not startLocationActor:IsValid() then
        -- scene 에 StartLocation 태그 액터가 없거나 dangling — 무음 무시.
        return
    end

    -- transform 먼저 set, 그 다음 velocity/angular 0 클리어. PhysX pre-simulate teleport
    -- 임계 (1m²) 를 넘는 큰 위치 변화면 setGlobalPose 로 강제 동기화되므로 안전.
    car.Location = startLocationActor.Location
    car.Rotation = startLocationActor.Rotation
    movement:StopImmediately()
end

function EndPlay()
end

function OnOverlap(OtherActor)
end

function Tick(dt)
    if car == nil or movement == nil then
        return
    end

    local gs = GetGameState()
    if gs == nil then return false end

    if gs:GetPhase() == ECarGamePhase.CarWash or gs:GetPhase() == ECarGamePhase.CarGas then
        movement:StopImmediately()
        movement:SetThrottleInput(0)
        movement:SetSteeringInput(0)
        return
    end

    -- R → 시작 위치/회전으로 리스폰 (차량이 뒤집혔을 때 복구용).
    if Input.GetKeyDown(Key.R) then
        ResetCarToStart()
    end

    local throttle = 0
    if Input.GetKey(Key.W) then throttle = throttle + 1 end
    if Input.GetKey(Key.S) then throttle = throttle - 1 end

    local steering = 0
    if Input.GetKey(Key.A) then steering = steering - 1 end
    if Input.GetKey(Key.D) then steering = steering + 1 end

    movement:SetThrottleInput(throttle)
    movement:SetSteeringInput(steering)
end

