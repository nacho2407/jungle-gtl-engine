local Coroutine = {}
Coroutine.coroutines = {}

function StartCoroutine(func)
    return Coroutine:Create(func)
end

function StopCoroutine(handle)
    return Coroutine:Stop(handle)
end

function Wait(seconds)
    Coroutine:Wait(seconds)
end

function WaitFrame()
    return Coroutine:WaitFrame()
end

function WaitUntil(predicate)
    Coroutine:WaitUntil(predicate)
end

function UpdateCoroutines(dt)
    Coroutine:Update(dt)
end

function Coroutine:Create(func)
    local routine = {
        co = coroutine.create(func),
        wait = nil,
        dead = false
    }

    table.insert(self.coroutines, routine)
    self:Resume(routine, 0)

    return routine
end

function Coroutine:Stop(handle)
    if handle == nil then
        return false
    end

    for i = #self.coroutines, 1, -1 do
        local routine = self.coroutines[i]
        if routine == handle or routine.co == handle then
            routine.dead = true
            table.remove(self.coroutines, i)
            return true
        end
    end

    return false
end

function Coroutine:Resume(routine, dt)
    local success, waitInfo = coroutine.resume(routine.co, dt or 0)

    if not success then
        print("Error in coroutine: " .. tostring(waitInfo))
        routine.dead = true
        return
    end

    routine.wait = waitInfo
end

function Coroutine:Wait(seconds)
    coroutine.yield({
        type = "wait",
        time = seconds
    })
end

function Coroutine:WaitFrame()
    return coroutine.yield({
        type = "frame"
    }) or 0
end

function Coroutine:WaitUntil(predicate)
    coroutine.yield({
        type = "wait_until",
        predicate = predicate
    })
end

function Coroutine:Update(dt)
    for i = #self.coroutines, 1, -1 do
        local routine = self.coroutines[i]

        if coroutine.status(routine.co) == "dead" or routine.dead then
            table.remove(self.coroutines, i)
        else
            local wait = routine.wait
            local shouldResume = false

            if wait == nil then
                shouldResume = true
            elseif wait.type == "wait" then
                wait.time = wait.time - dt;
                shouldResume = wait.time <= 0
            elseif wait.type == "frame" then
                shouldResume = true
            elseif wait.type == "wait_until" then
                local ok, result = pcall(wait.predicate)
                if not ok then
                    print("Error in wait_until predicate: " .. result)
                    routine.dead = true
                else
                    shouldResume = result == true
                end
            else
                shouldResume = true
            end

            if shouldResume and not routine.dead then
                self:Resume(routine, dt)
            end
        end
    end
end

return Coroutine
