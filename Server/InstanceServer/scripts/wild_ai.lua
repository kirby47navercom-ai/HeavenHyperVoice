-- 야생 포켓몬 Lua actions.
--
-- 서버(C++)가 야생 포켓몬 FSM을 소유한다. 이 파일은 FSM이 새 행동 결정을
-- 필요로 할 때만 호출되는 action 함수들을 제공한다. 매 서버 틱마다 Lua를
-- 호출하지 않는다.
--
-- 현재는 wander 하나만 있다. 새 행동을 넣을 때는 wild_actions.<name> 함수를
-- 추가하고, C++ FSM 쪽에서 그 action을 호출하면 된다.
--
-- 돌아다닐 구역은 ctx 로 받는다. 여기 상수로 두면 맵이 바뀔 때마다 스크립트를
-- 같이 고쳐야 하고, 빠뜨리면 야생이 맵 밖 경계에 몰려 선다.

local WANDER_RADIUS = 3000.0   -- 한 번에 배회하는 최대 거리.
                               -- 구역 반폭만큼 크게 두면 고른 목표가 매번
                               -- 구역 밖이라 전부 경계로 클램프되고, 야생이
                               -- 테두리에 몰려 선다. 반폭보다 넉넉히 작게 둘 것.
local ARRIVE_RADIUS = 80.0     -- 이보다 가까우면 도착으로 본다
local REST_MIN      = 1.5      -- 목표 사이 쉬는 시간(초) 범위
local REST_MAX      = 4.0

wild_actions = wild_actions or {}

local function clamp_area(v, center, half_extent)
    local lo = center - half_extent
    local hi = center + half_extent
    if v < lo then return lo end
    if v > hi then return hi end
    return v
end

local function random_range(min_value, max_value)
    return min_value + math.random() * (max_value - min_value)
end

-- wander action: 지금 위치 주변에서 다음 산책 목표 하나를 고른다.
--
-- ctx:
--   id, species, x, y, home_x, home_y
--   area_center_x, area_center_y, area_half_extent   <- 돌아다닐 상자
--
-- return:
--   target_x / target_y: 이번 산책 목표
--   acceptance_radius: 이 거리 안에 들면 C++ FSM 이 목표 달성으로 본다
--   rest_seconds: 목표 달성 뒤 C++ FSM 이 Lua 호출 없이 쉬게 할 시간
function wild_actions.wander(ctx)
    local center_x = ctx.area_center_x or ctx.home_x or 0.0
    local center_y = ctx.area_center_y or ctx.home_y or 0.0
    local half_extent = ctx.area_half_extent or 4000.0

    -- 구역이 좁으면 배회 거리도 같이 줄인다. 안 그러면 목표가 매번 밖으로
    -- 나가 전부 경계로 클램프된다.
    local radius = math.min(WANDER_RADIUS, half_extent * 0.4)

    local angle = math.random() * math.pi * 2.0
    local distance = math.random() * radius

    local x = ctx.x or center_x
    local y = ctx.y or center_y
    return {
        target_x = clamp_area(x + math.cos(angle) * distance, center_x, half_extent),
        target_y = clamp_area(y + math.sin(angle) * distance, center_y, half_extent),
        acceptance_radius = ARRIVE_RADIUS,
        rest_seconds = random_range(REST_MIN, REST_MAX)
    }
end
