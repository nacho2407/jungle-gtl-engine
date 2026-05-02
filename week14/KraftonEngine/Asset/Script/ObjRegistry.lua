local ObjRegistry = {}

ObjRegistry.car = nil
ObjRegistry.manCamera = nil

function ObjRegistry.RegisterCar(car)
    ObjRegistry.car = car
end

function ObjRegistry.RegisterManCamera(manCamera)
    ObjRegistry.manCamera = manCamera
end

return ObjRegistry
