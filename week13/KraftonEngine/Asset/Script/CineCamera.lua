local ObjRegistry = require("ObjRegistry")

function BeginPlay()
    ObjRegistry.RegisterPoliceCineCamera(obj)
end

function EndPlay()
    ObjRegistry.UnregisterPoliceCineCamera(obj)
end

function OnOverlap(OtherActor)
end

function Tick(dt)
end
