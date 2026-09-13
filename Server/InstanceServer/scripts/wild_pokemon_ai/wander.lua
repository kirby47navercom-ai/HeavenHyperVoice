local bt = require("wild_pokemon_ai.bt")
local perception = require("wild_pokemon_ai.perception")

local AGGRO_RADIUS = 900.0
local MIN_MOVE_SECONDS = 1.0
local MAX_MOVE_SECONDS = 3.0
local MIN_REST_SECONDS = 1.0
local MAX_REST_SECONDS = 3.0

local function random_seconds(minimum, maximum)
    return minimum + math.random() * (maximum - minimum)
end

local root = bt.selector({
    bt.sequence({
        bt.condition(function(ctx)
            ctx.detected_player = perception.nearest(ctx, AGGRO_RADIUS)
            return ctx.detected_player ~= nil
        end),
        bt.action(function(ctx)
            return {
                next_state = "battle",
                action = "wait",
                target_id = ctx.detected_player.id
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
            return ctx.moving
        end),
        bt.action(function()
            return { action = "continue" }
        end)
    }),
    bt.action(function()
        return {
            action = "wander",
            move_seconds = random_seconds(MIN_MOVE_SECONDS, MAX_MOVE_SECONDS),
            rest_seconds = random_seconds(MIN_REST_SECONDS, MAX_REST_SECONDS)
        }
    end)
})

return function(ctx)
    return bt.evaluate(root, ctx)
end
