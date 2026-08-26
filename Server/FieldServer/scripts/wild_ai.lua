-- 야생 포켓몬 행동 트리.
--
-- 서버(C++)가 20Hz 로 wild_tick(id, species, x, y, dt) 을 부른다.
-- 목표점으로 걷고 싶으면 (target_x, target_y) 를 반환하고, 제자리에 있고
-- 싶으면 아무것도 반환하지 않는다. 서버가 속도 상한과 벽 충돌을 마지막에
-- 강제하므로, 여기서는 "어디로 가고 싶은가" 만 정하면 된다.
--
-- 트리는 매 틱 root 를 평가한다. 노드는 running/success 를 돌려주고, 이동을
-- 원하는 노드만 intent 에 목표를 채운다. 새 행동을 넣으려면 노드 함수를
-- 하나 만들어 selector 에 끼우면 되고 C++ 은 건드리지 않는다.

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

local RUNNING, SUCCESS = "running", "success"

-- 포켓몬별 상태(블랙보드). id 로 찾는다.
local boards = {}

local function board_of(id, x, y)
    local b = boards[id]
    if not b then
        b = { rest = 0.0, tx = x, ty = y, has_target = false }
        boards[id] = b
    end
    return b
end

local function clamp_area(v)
    local lo = AREA_CENTER - AREA_HALF_EXTENT
    local hi = AREA_CENTER + AREA_HALF_EXTENT
    if v < lo then return lo end
    if v > hi then return hi end
    return v
end

local function dist2(ax, ay, bx, by)
    local dx, dy = ax - bx, ay - by
    return dx * dx + dy * dy
end

-- --- 노드 -----------------------------------------------------------------

-- 쉬는 중이면 타이머를 깎고 제자리에 선다.
local function node_rest(bb, dt)
    if bb.rest <= 0.0 then
        return SUCCESS
    end
    bb.rest = bb.rest - dt
    return RUNNING  -- intent 를 채우지 않음 = 제자리
end

-- 목표가 없으면 현재 위치 주변에서 하나 고른다.
local function node_pick_target(bb, x, y)
    if bb.has_target then
        return SUCCESS
    end
    local angle = math.random() * math.pi * 2.0
    local radius = math.random() * WANDER_RADIUS
    bb.tx = clamp_area(x + math.cos(angle) * radius)
    bb.ty = clamp_area(y + math.sin(angle) * radius)
    bb.has_target = true
    return SUCCESS
end

-- 목표에 도착했으면 잠시 쉬기로 하고, 아니면 그쪽으로 걷는다.
local function node_move_to_target(bb, x, y, intent)
    if dist2(x, y, bb.tx, bb.ty) <= ARRIVE_RADIUS * ARRIVE_RADIUS then
        bb.has_target = false
        bb.rest = REST_MIN + math.random() * (REST_MAX - REST_MIN)
        return SUCCESS
    end
    intent.x, intent.y = bb.tx, bb.ty
    return RUNNING
end

-- selector: 쉬는 중이면 거기서 멈추고, 아니면 목표를 골라 걷는다.
local function tick_tree(bb, x, y, dt, intent)
    if node_rest(bb, dt) == RUNNING then
        return
    end
    node_pick_target(bb, x, y)
    node_move_to_target(bb, x, y, intent)
end

-- --- C++ 진입점 -----------------------------------------------------------

function wild_tick(id, species, x, y, dt)
    local bb = board_of(id, x, y)
    local intent = {}
    tick_tree(bb, x, y, dt, intent)
    if intent.x then
        return intent.x, intent.y
    end
    -- 반환 없음 = 제자리
end

