-- 서버는 좌표를 제공하고, 거리/대상 선택 정책은 Lua에서 계산한다.
local perception = {}

function perception.distance_squared(ctx, player)
    local dx = player.x - ctx.x
    local dy = player.y - ctx.y
    return dx * dx + dy * dy
end

function perception.find_target(ctx)
    for _, player in ipairs(ctx.players) do
        if player.id == ctx.target_id and player.map_id == ctx.map_id then
            return player
        end
    end
    return nil
end

function perception.nearest(ctx, radius)
    local best
    local best_distance = radius * radius

    for _, player in ipairs(ctx.players) do
        if player.map_id == ctx.map_id then
            local distance = perception.distance_squared(ctx, player)
            if distance < best_distance or
                (distance == best_distance and (not best or player.id < best.id)) then
                best = player
                best_distance = distance
            end
        end
    end

    return best
end

return perception
