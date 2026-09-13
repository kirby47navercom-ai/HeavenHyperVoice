local bt = require("wild_pokemon_ai.bt")
local perception = require("wild_pokemon_ai.perception")

local LOSE_TARGET_RADIUS = 1800.0
local ATTACK_RADIUS = 180.0
local REPATH_DISTANCE = 180.0

local root = bt.selector({
    bt.sequence({
        bt.condition(function(ctx)
            ctx.target = perception.find_target(ctx)
            return not ctx.target or
                perception.distance_squared(ctx, ctx.target) > LOSE_TARGET_RADIUS * LOSE_TARGET_RADIUS
        end),
        bt.action(function()
            return { next_state = "wander", action = "wait" }
        end)
    }),
    bt.sequence({
        bt.condition(function(ctx)
            return perception.distance_squared(ctx, ctx.target) <= ATTACK_RADIUS * ATTACK_RADIUS
        end),
        bt.action(function(ctx)
            if ctx.attack_cooldown > 0 then
                return { action = "wait" }
            end
            return {
                action = "attack",
                target_id = ctx.target.id,
                attack_range = ATTACK_RADIUS,
                reconsider_seconds = 1.2
            }
        end)
    }),
    bt.sequence({
        bt.condition(function(ctx)
            return ctx.rest_remaining > 0
        end),
        bt.action(function()
            return { action = "wait" }
        end)
    }),
    bt.sequence({
        bt.condition(function(ctx)
            if not ctx.moving then
                return false
            end
            local dx = ctx.target.x - ctx.path_target_x
            local dy = ctx.target.y - ctx.path_target_y
            return ctx.replan_remaining > 0 or
                dx * dx + dy * dy <= REPATH_DISTANCE * REPATH_DISTANCE
        end),
        bt.action(function()
            return { action = "continue" }
        end)
    }),
    bt.action(function(ctx)
        return {
            action = "chase",
            target_id = ctx.target.id,
            acceptance_radius = 140.0,
            reconsider_seconds = 0.5
        }
    end)
})

return function(ctx)
    return bt.evaluate(root, ctx)
end
