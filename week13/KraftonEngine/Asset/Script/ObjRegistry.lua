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

function ObjRegistry.RegisterCar(car)
    ObjRegistry.car = car
    ObjRegistry.carCamera = car:GetCamera() 
    ObjRegistry.carGas = car:GetCarGas()
end

function ObjRegistry.RegisterMan(man)
    ObjRegistry.manObj = man
    ObjRegistry.manCamera = man:GetCamera()
end

function ObjRegistry.RegisterGasNozzle(gasNozzle)
    ObjRegistry.gasNozzle = gasNozzle
end

function ObjRegistry.RegisterDirtyCar(dirtyCar)
    ObjRegistry.dirtyCar = dirtyCar
end

function ObjRegistry.RegisterCarWasher(carWasher)
    ObjRegistry.carWasher = carWasher
end

function ObjRegistry.RegisterPoliceCar(policeCar)
    if policeCar == nil then
        return
    end

    table.insert(ObjRegistry.policeCars, policeCar)
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
