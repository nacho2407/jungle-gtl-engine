local ObjRegistry = {}

ObjRegistry.car = nil
ObjRegistry.carCamera = nil
ObjRegistry.carGas = nil
ObjRegistry.manObj = nil
ObjRegistry.manCamera = nil
ObjRegistry.gasNozzle = nil
ObjRegistry.carWasher = nil
ObjRegistry.dirtyCar = nil

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

return ObjRegistry
