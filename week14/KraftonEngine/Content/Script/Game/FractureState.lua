local State = {
    Act = 1,
    Score = 0,
    bEndingReached = false,
}

function State.Reset()
    State.Act = 1
    State.Score = 0
    State.bEndingReached = false
end

function State.AddScore(amount)
    State.Score = State.Score + (amount or 0)
    return State.Score
end

function State.SetAct(act)
    State.Act = act or State.Act
end

function State.MarkEndingReached()
    State.bEndingReached = true
end

return State
