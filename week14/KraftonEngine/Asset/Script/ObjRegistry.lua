local ObjRegistry = {}

ObjRegistry.car = nil
ObjRegistry.carCamera = nil
ObjRegistry.carGas = nil
ObjRegistry.manObj = nil
ObjRegistry.manCamera = nil
ObjRegistry.gasNozzle = nil
ObjRegistry.carWasher = nil
ObjRegistry.dirtyCar = nil
ObjRegistry.policeCars = {}

-- 등록된 액터와 파라미터 액터가 같은 인스턴스인지 안전하게 비교.
-- EndPlay 시점이라 양쪽 다 alive 가정이지만, dangling 방어로 IsValid 먼저 체크.
local function SameActor(a, b)
    if a == nil or b == nil then return false end
    if not a:IsValid() or not b:IsValid() then return false end
    return a.UUID == b.UUID
end

function ObjRegistry.RegisterCar(car)
    ObjRegistry.car = car
    ObjRegistry.carCamera = car:GetCamera()
    ObjRegistry.carGas = car:GetCarGas()
end

function ObjRegistry.UnregisterCar(car)
    if SameActor(ObjRegistry.car, car) then
        ObjRegistry.car = nil
        ObjRegistry.carCamera = nil
        ObjRegistry.carGas = nil
    end
end

function ObjRegistry.RegisterMan(man)
    ObjRegistry.manObj = man
    ObjRegistry.manCamera = man:GetCamera()
end

function ObjRegistry.UnregisterMan(man)
    if SameActor(ObjRegistry.manObj, man) then
        ObjRegistry.manObj = nil
        ObjRegistry.manCamera = nil
    end
end

function ObjRegistry.RegisterGasNozzle(gasNozzle)
    ObjRegistry.gasNozzle = gasNozzle
end

function ObjRegistry.UnregisterGasNozzle(gasNozzle)
    if SameActor(ObjRegistry.gasNozzle, gasNozzle) then
        ObjRegistry.gasNozzle = nil
    end
end

function ObjRegistry.RegisterDirtyCar(dirtyCar)
    ObjRegistry.dirtyCar = dirtyCar
end

function ObjRegistry.UnregisterDirtyCar(dirtyCar)
    if SameActor(ObjRegistry.dirtyCar, dirtyCar) then
        ObjRegistry.dirtyCar = nil
    end
end

function ObjRegistry.RegisterCarWasher(carWasher)
    ObjRegistry.carWasher = carWasher
end

function ObjRegistry.UnregisterCarWasher(carWasher)
    if SameActor(ObjRegistry.carWasher, carWasher) then
        ObjRegistry.carWasher = nil
    end
end

function ObjRegistry.RegisterPoliceCar(policeCar)
    if policeCar == nil then
        return
    end

    table.insert(ObjRegistry.policeCars, policeCar)
end

-- 배열에서 첫 매칭 entry 만 제거. dangling 인 entry 는 IsValid 로 걸러 deref 안 함.
function ObjRegistry.UnregisterPoliceCar(policeCar)
    if policeCar == nil or not policeCar:IsValid() then
        return
    end

    local targetUUID = policeCar.UUID
    for i = #ObjRegistry.policeCars, 1, -1 do
        local entry = ObjRegistry.policeCars[i]
        if entry ~= nil and entry:IsValid() and entry.UUID == targetUUID then
            table.remove(ObjRegistry.policeCars, i)
            return
        end
    end
end

function ObjRegistry.GetNearestPoliceDistance(location)
    if location == nil then
        return nil
    end

    local nearestDistance = nil
    local writeIndex = 1

    for i = 1, #ObjRegistry.policeCars do
        local policeCar = ObjRegistry.policeCars[i]
        if policeCar ~= nil and policeCar:IsValid() then
            ObjRegistry.policeCars[writeIndex] = policeCar
            writeIndex = writeIndex + 1

            local distance = location:Distance(policeCar.Location)
            if nearestDistance == nil or distance < nearestDistance then
                nearestDistance = distance
            end
        end
    end

    for i = writeIndex, #ObjRegistry.policeCars do
        ObjRegistry.policeCars[i] = nil
    end

    return nearestDistance
end

return ObjRegistry
