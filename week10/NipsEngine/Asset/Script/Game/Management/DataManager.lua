local DataManager = {}
DataManager.__index = DataManager

function DataManager.new(context)
    return setmetatable({
        context = context,
        savePath = "GameData/scoreboard.json",
        bestScore = 0,
        scoreRecords = {}
    }, DataManager)
end

function DataManager:BeginPlay()
    self:Load()
end

local function Trim(value)
    value = tostring(value or "")
    return value:match("^%s*(.-)%s*$")
end

local function NormalizeUserName(userName)
    return string.lower(Trim(userName))
end

local function SortScoreRecords(records)
    table.sort(records, function(a, b)
        if (a.score or 0) == (b.score or 0) then
            return tostring(a.name or "") < tostring(b.name or "")
        end
        return (a.score or 0) > (b.score or 0)
    end)
end

function DataManager:Load()
    local text = Engine.API.Save.ReadText(self.savePath)
    if text == nil or text == "" then
        self.bestScore = 0
        self.scoreRecords = {}
        return
    end

    local data = Engine.API.Json.Decode(text)
    if data == nil then
        self.bestScore = 0
        self.scoreRecords = {}
        return
    end

    self.bestScore = math.floor(tonumber(data.bestScore) or 0)
    self.scoreRecords = {}

    local players = data.players or {}
    for _, record in ipairs(players) do
        local name = Trim(record.name)
        local key = NormalizeUserName(record.key or name)
        local score = math.floor(tonumber(record.score) or 0)
        if name ~= "" and key ~= "" then
            table.insert(self.scoreRecords, {
                name = name,
                key = key,
                score = score
            })
            self.bestScore = math.max(self.bestScore, score)
        end
    end

    SortScoreRecords(self.scoreRecords)
end

function DataManager:Save()
    local data = {
        version = 1,
        bestScore = self.bestScore,
        players = self.scoreRecords
    }

    return Engine.API.Save.WriteText(self.savePath, Engine.API.Json.Encode(data))
end

function DataManager:SaveBestScore(score)
    score = math.floor(score or 0)
    if score <= self.bestScore then
        return
    end

    self.bestScore = score
    self:Save()
end

function DataManager:FindPlayer(userName)
    local key = NormalizeUserName(userName)
    if key == "" then
        return nil
    end

    for _, record in ipairs(self.scoreRecords) do
        if record.key == key then
            return record
        end
    end

    return nil
end

function DataManager:IsUserNameTaken(userName)
    return self:FindPlayer(userName) ~= nil
end

function DataManager:RegisterScore(userName, score)
    local name = Trim(userName)
    local key = NormalizeUserName(name)
    score = math.floor(score or 0)

    if name == "" or key == "" then
        return false, "InvalidName"
    end

    local record = self:FindPlayer(key)
    if record == nil then
        record = {
            name = name,
            key = key,
            score = score
        }
        table.insert(self.scoreRecords, record)
    elseif score > (record.score or 0) then
        record.score = score
    end

    self.bestScore = math.max(self.bestScore, score)
    SortScoreRecords(self.scoreRecords)
    self:Save()
    return true, record
end

function DataManager:GetScoreRecords(limit)
    local result = {}
    local count = math.min(limit or #self.scoreRecords, #self.scoreRecords)
    for index = 1, count do
        local record = self.scoreRecords[index]
        result[index] = {
            name = record.name,
            key = record.key,
            score = record.score
        }
    end
    return result
end

function DataManager:GetBestScore()
    return self.bestScore
end

return DataManager
