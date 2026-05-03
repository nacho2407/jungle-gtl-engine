function BeginPlay()

end

function EndPlay()

end

function OnOverlapBegin(other)
    if other:HasTag("Player") then
        obj:AddWorldOffset(Vector.new(100, 0, 0))
    end
end

function Tick(dt)

end
