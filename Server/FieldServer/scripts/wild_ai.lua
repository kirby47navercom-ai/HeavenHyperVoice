-- 야생 포켓몬 Lua actions.
--
-- 서버(C++)가 야생 포켓몬 FSM을 소유한다. 이 파일은 FSM이 새 행동 결정을
-- 필요로 할 때만 호출되는 action 함수들을 제공한다. 매 서버 틱마다 Lua를
-- 호출하지 않는다.
--
-- 현재는 wander 하나만 있다. 새 행동을 넣을 때는 wild_actions.<name> 함수를
-- 추가하고, C++ FSM 쪽에서 그 action을 호출하면 된다.

-- 야생이 돌아다니는 구역. 월드 전체가 아니라 중앙의 정사각형으로 묶어 둔다.
-- 클라이언트는 원점이 월드 중앙이므로(WorldOriginOffset), 언리얼 좌표로는
-- -4000 ~ +4000 인 8000x8000 상자다.
local AREA_CENTER      = 25600.0   -- FieldGeometry.h 의 kSpawnX / kSpawnY
local AREA_HALF_EXTENT = 4000.0
local WANDER_RADIUS    = 1500.0    -- 한 번에 배회하는 최대 거리.
                                   -- 구역 반폭(4000)만큼 크게 두면 고른 목표가
                                   -- 매번 구역 밖이라 전부 경계로 클램프되고,
                                   -- 야생이 테두리에 몰려 선다.
local ARRIVE_RADIUS    = 80.0      -- 이보다 가까우면 도착으로 본다
local REST_MIN         = 1.5       -- 목표 사이 쉬는 시간(초) 범위
local REST_MAX         = 4.0

wild_actions = wild_actions or {}

local function clamp_area(v)
    local lo = AREA_CENTER - AREA_HALF_EXTENT
    local hi = AREA_CENTER + AREA_HALF_EXTENT
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
--
-- return:
--   target_x / target_y: 이번 산책 목표
--   acceptance_radius: 이 거리 안에 들면 C++ FSM 이 목표 달성으로 본다
--   rest_seconds: 목표 달성 뒤 C++ FSM 이 Lua 호출 없이 쉬게 할 시간
function wild_actions.wander(ctx)
    local angle = math.random() * math.pi * 2.0
    local radius = math.random() * WANDER_RADIUS

    local x = ctx.x or AREA_CENTER
    local y = ctx.y or AREA_CENTER
    return {
        target_x = clamp_area(x + math.cos(angle) * radius),
        target_y = clamp_area(y + math.sin(angle) * radius),
        acceptance_radius = ARRIVE_RADIUS,
        rest_seconds = random_range(REST_MIN, REST_MAX)
    }
end
