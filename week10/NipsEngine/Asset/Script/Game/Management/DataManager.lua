local DataManager = {}
DataManager.__index = DataManager

function DataManager.new(context)
    return setmetatable({
        context = context,
        savePath = "samurai_save.txt",
        bestScore = 0
    }, DataManager)
end

function DataManager:BeginPlay()
    self:Load()
end

function DataManager:Load()
    local text = Engine.API.Save.ReadText(self.savePath)
    self.bestScore = tonumber(text) or 0
end

function DataManager:SaveBestScore(score)
    score = math.floor(score or 0)
    if score <= self.bestScore then
        return
    end

    self.bestScore = score
    Engine.API.Save.WriteText(self.savePath, tostring(self.bestScore))
end

function DataManager:GetBestScore()
    return self.bestScore
end

return DataManager
