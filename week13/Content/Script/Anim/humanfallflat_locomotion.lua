local IDLE_PATH = "Content/Data/HumanFallFlat/Idle_mixamo_com.uasset"
local WALK_PATH = "Content/Data/HumanFallFlat/HumanWalking_mixamo_com.uasset"

function init(self)
    self.Speed = 0.0
    self.SpeedThreshold = 0.2

    local locomotion = Anim.create_state_machine("HumanFallFlatLocomotion")
    Anim.sm_add_state(locomotion, "Idle", Anim.create_sequence_player(IDLE_PATH, 1.0, true))
    Anim.sm_add_state(locomotion, "Walk", Anim.create_sequence_player(WALK_PATH, 1.0, true))
    Anim.sm_add_transition(locomotion, "Idle", "Walk",
        function() return self.Speed > self.SpeedThreshold end, 0.2)
    Anim.sm_add_transition(locomotion, "Walk", "Idle",
        function() return self.Speed <= self.SpeedThreshold end, 0.2)
    Anim.sm_set_initial_state(locomotion, "Idle")

    Anim.set_root_node(locomotion)
end

function update(self, dt)
    self.Speed = Anim.get_owner_speed()
end
