function BeginPlay()
end

function EndPlay()
end

function Tick(dt)
    if Input.GetKeyDown(Key.F2) then
        CameraManager.ToggleOwnerCamera(obj)
    end
end
