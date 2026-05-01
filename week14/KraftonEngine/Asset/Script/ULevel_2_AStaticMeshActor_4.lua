local test = false

function BeginPlay()
    print("[BeginPlay] " .. obj.UUID)

    StartCoroutine(function()
        print("Coroutine started")
        Wait(2)
        test = true
    end)

    print("BeginPlay finished")
end

function EndPlay()
    print("[EndPlay] " .. obj.UUID)
end

function OnOverlap(OtherActor)
end

function Tick(dt)
    if test then
        obj.Location = obj.Location + Vector.new(10, 0, 0) * dt
    end
end
