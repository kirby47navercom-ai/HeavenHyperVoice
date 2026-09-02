-- Wild pokemon behaviour tree.
--
-- C++ owns movement, navmesh checks, collision checks, and the final validation.
-- Lua only receives the data this first tree needs and picks the next action.

wild_ai = wild_ai or {}

local SUCCESS = "success"
local FAILURE = "failure"

local function selector(children)
    return function(ctx)
        for _, child in ipairs(children) do
            local status, intent = child(ctx)
            if status == SUCCESS then
                return SUCCESS, intent
            end
        end
        return FAILURE, nil
    end
end

local function sequence(children)
    return function(ctx)
        local last_intent = nil
        for _, child in ipairs(children) do
            local status, intent = child(ctx)
            if status ~= SUCCESS then
                return FAILURE, nil
            end
            last_intent = intent or last_intent
        end
        return SUCCESS, last_intent
    end
end

local function condition(predicate)
    return function(ctx)
        return predicate(ctx) and SUCCESS or FAILURE, nil
    end
end

local function action(fn)
    return function(ctx)
        return SUCCESS, fn(ctx)
    end
end

local function in_range(player, radius)
    return player ~= nil
        and player.id ~= nil
        and player.distance ~= nil
        and radius ~= nil
        and player.distance <= radius
end

local function has_current_target(ctx)
    return in_range(ctx.current_target, ctx.lose_target_radius)
end

local function player_near(ctx)
    return in_range(ctx.nearest_player, ctx.aggro_radius)
end

local function chase_current_target(ctx)
    return {
        action = "chase",
        target_id = ctx.current_target.id,
        acceptance_radius = 140.0,
        reconsider_seconds = 0.5
    }
end

local function chase_nearest_player(ctx)
    return {
        action = "chase",
        target_id = ctx.nearest_player.id,
        acceptance_radius = 140.0,
        reconsider_seconds = 0.5
    }
end

local function wander()
    return {
        action = "wander",
        wander_radius = 1500.0,
        acceptance_radius = 80.0,
        rest_seconds = 2.0
    }
end

local wild_tree = selector({
    sequence({
        condition(has_current_target),
        action(chase_current_target)
    }),
    sequence({
        condition(player_near),
        action(chase_nearest_player)
    }),
    action(wander)
})

function wild_ai.decide(ctx)
    local _, intent = wild_tree(ctx or {})
    return intent or wander()
end
