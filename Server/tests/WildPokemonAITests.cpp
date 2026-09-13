#include "WildAi.h"
#include "WildBt.h"
#include "WildPokemonAIWanderAction.h"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace heaven::instance;

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
    std::cout << "PASS: " << message << '\n';
}

struct ScriptFixture {
    std::filesystem::path path;

    explicit ScriptFixture(const char* script) {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() /
               ("hhv-wild-ai-" + std::to_string(stamp) + ".lua");
        std::ofstream output(path);
        output << script;
    }

    ~ScriptFixture() {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
};

void testLuaDecisions(const std::string& script) {
    WildBt behavior(script);
    WildPokemonAIActionMemory memory;
    std::vector<ObservedPlayer> players;
    WildBtContext context;
    context.entityId = 1;
    context.mapId = 1;
    context.memory = &memory;
    context.players = &players;

    auto decision = behavior.decide(context);
    require(decision.valid && decision.action == WildDecision::Action::Wander,
            "Wander BT selects wandering with no players");
    const auto firstDecision = decision;
    bool variedDuration = false;
    for (int sample = 0; sample < 12; ++sample) {
        decision = behavior.decide(context);
        if (decision.moveSeconds < 1.f || decision.moveSeconds > 3.f ||
            decision.restSeconds < 1.f || decision.restSeconds > 3.f) {
            throw std::runtime_error("Wander durations must stay in their Lua-configured ranges");
        }
        variedDuration = variedDuration || decision.moveSeconds != firstDecision.moveSeconds ||
                        decision.restSeconds != firstDecision.restSeconds;
    }
    require(variedDuration, "Lua samples random movement and rest durations in the configured ranges");

    memory.movement.moving = true;
    decision = behavior.decide(context);
    require(decision.action == WildDecision::Action::Continue,
            "Wander BT continues the current directional movement");

    memory.movement.restRemaining = 2.f;
    players = {{7, 2, 1, 0, 0}, {9, 1, 700, 0, 0}, {8, 1, 300, 400, 0}};
    decision = behavior.decide(context);
    require(decision.valid && decision.nextState == WildPokemonAIState::Battle && decision.targetId == 8,
            "Lua computes distance from raw positions, ignores other maps and interrupts rest");

    context.state = WildPokemonAIState::Battle;
    memory.targetId = 8;
    memory.movement.restRemaining = 0.f;
    memory.movement.moving = false;
    decision = behavior.decide(context);
    require(decision.action == WildDecision::Action::Chase && decision.targetId == 8,
            "Battle BT selects chase outside attack range");

    memory.movement.moving = true;
    memory.movement.requestedGoal = {300, 400, 0};
    decision = behavior.decide(context);
    require(decision.action == WildDecision::Action::Continue,
            "Battle BT keeps a valid chase path instead of searching every tick");
    players[2].x = 600.f;
    decision = behavior.decide(context);
    require(decision.action == WildDecision::Action::Chase,
            "Lua requests a new path when the target moves beyond the threshold");
    memory.movement.replanRemaining = 0.3f;
    decision = behavior.decide(context);
    require(decision.action == WildDecision::Action::Continue,
            "Lua preserves the repath cooldown while the target moves");

    players[2].x = 100.f;
    players[2].y = 0.f;
    decision = behavior.decide(context);
    require(decision.action == WildDecision::Action::Attack,
            "Battle BT selects attack inside attack range");
    memory.attackCooldown = 1.f;
    decision = behavior.decide(context);
    require(decision.action == WildDecision::Action::Wait,
            "Battle BT waits during attack cooldown");

    players[2].x = 2000.f;
    decision = behavior.decide(context);
    require(decision.nextState == WildPokemonAIState::Wander,
            "Battle BT requests Wander when the target is too far away");
    players.clear();
    decision = behavior.decide(context);
    require(decision.nextState == WildPokemonAIState::Wander,
            "Battle BT handles a missing target");
}

void testLuaOwnsTransitions() {
    ScriptFixture fixture(R"(
        wild_ai = {
            wander = function(ctx)
                return { next_state = "battle", action = "wait", target_id = ctx.players[1].id }
            end,
            battle = function(ctx)
                return { next_state = "wander", action = "wait" }
            end
        }
    )");
    WildAi ai(std::make_unique<WildBt>(fixture.path.string()));
    const std::vector<ObservedPlayer> players{{99, 1, 5000, 0, 0}};
    ai.decide(1, 1, 1, 0, 0, 0, .05f, players);
    require(ai.stateOf(1) == WildPokemonAIState::Battle,
            "C++ applies Lua transition even outside the old native aggro radius");
    ai.decide(1, 1, 1, 0, 0, 0, .05f, players);
    require(ai.stateOf(1) == WildPokemonAIState::Wander,
            "C++ applies Lua return transition without native battle policy");
    require(ai.stateOf(2) == WildPokemonAIState::Wander,
            "FSM state is isolated per Pokemon");
}

void testBadScriptResults() {
    ScriptFixture fixture(R"(
        wild_ai = {
            wander = function(ctx)
                if ctx.entity_id == 1 then error("test runtime error") end
                return { next_state = "unknown", action = "wait" }
            end,
            battle = function(ctx) return { action = "wait" } end
        }
    )");
    WildAi ai(std::make_unique<WildBt>(fixture.path.string()));
    for (std::uint64_t id : {1, 2}) {
        const auto intent = ai.decide(id, 1, 1, 0, 0, 0, .05f, {});
        require(!intent.moving && !intent.attacking && ai.stateOf(id) == WildPokemonAIState::Wander,
                "Lua runtime errors and unknown states cannot change the FSM");
    }
}

void testTimedWandering() {
    ScriptFixture fixture(R"(
        wild_ai = {
            wander = function(ctx)
                if ctx.rest_remaining > 0 then return { action = "wait" } end
                if ctx.moving then return { action = "continue" } end
                return { action = "wander", move_seconds = 0.2, rest_seconds = 0.3 }
            end,
            battle = function(ctx) return { action = "wait" } end
        }
    )");
    WildAi ai(std::make_unique<WildBt>(fixture.path.string()));
    ai.seed(17);

    // 맵 없이도 방향 명령이 나온다. 배회 판단은 경로 탐색이나 지형 조회에 의존하지 않는다.
    const auto first = ai.decide(1, 1, 1, 0, 0, 0, .05f, {});
    require(first.moving, "Wander produces a direction without a navigation map");
    const auto continued = ai.decide(1, 1, 1, 10, 20, 0, .05f, {});
    require(continued.moving && std::abs(continued.targetX - 10.f - first.targetX) < .001f &&
            std::abs(continued.targetY - 20.f - first.targetY) < .001f,
            "Wander keeps the sampled direction as the entity moves");

    require(!ai.decide(1, 1, 1, 0, 0, 0, .2f, {}).moving,
            "Movement duration expiry starts the rest period");
    require(!ai.decide(1, 1, 1, 0, 0, 0, .1f, {}).moving,
            "Wander stays idle until its rest duration expires");
    const auto restarted = ai.decide(1, 1, 1, 0, 0, 0, .25f, {});
    require(restarted.moving && (std::abs(restarted.targetX - first.targetX) > .01f ||
                                 std::abs(restarted.targetY - first.targetY) > .01f),
            "Rest expiry starts another movement with a newly sampled direction");
    ai.notifyMoveBlocked(1);
    require(!ai.decide(1, 1, 1, 0, 0, 0, .05f, {}).moving,
            "Blocked wandering waits instead of searching for a detour");

    WildPokemonAIActionMemory memory;
    memory.movement.moving = true;
    memory.wanderDirection = {1, 0, 0};
    memory.wanderMoveRemaining = 1.f;
    memory.wanderRestSeconds = .7f;
    const std::vector<ObservedPlayer> players;
    const WildPokemonAIActionContext context{{4000, 0, 0}, nullptr, {0, 0, 4000}, players, 1};
    WildDecision decision;
    decision.action = WildDecision::Action::Continue;
    std::mt19937 random(17);
    const auto boundary = WildPokemonAIWanderAction{}.execute(decision, context, memory, random);
    require(!boundary.moving && memory.movement.restRemaining == .7f,
            "An outward direction at the area boundary starts the sampled rest period");
}

void testSharedCoreMovement(const std::string& script, const std::string& collisionFile) {
    heaven::Map map(0);
    std::string error;
    require(map.loadFromFile(collisionFile, error), "Load the actual Filed collision map");
    hhv::movement::State state;
    require(map.canStandAt(0, 0, map.agent(), &state.position), "Resolve a real grounded spawn");
    state.mode = hhv::movement::Mode::Grounded;

    WildAi ai(std::make_unique<WildBt>(script));
    ai.setMap(&map);
    ai.setArea({0, 0, 4000});
    ai.seed(17);
    const auto initial = state.position;
    float accumulator = 0.f;
    bool moved = false;
    bool grounded = true;
    for (int tick = 0; tick < 240; ++tick) {
        const auto p = state.position;
        const auto intent = ai.decide(1, 1, 1, p.x, p.y, p.z, .05f, {});
        map.advance(state, accumulator, .05f, {intent.targetX, intent.targetY, p.z}, intent.moving, 100.f);
        moved = moved || hhv::movement::length(state.position - initial) > 5.f;
        grounded = grounded && state.mode == hhv::movement::Mode::Grounded;
        if (intent.moving && hhv::movement::length(state.position - p) < .01f) {
            ai.notifyMoveBlocked(1);
        }
    }
    require(moved && grounded, "Wander action moves on Filed using the shared movement core");

    const auto p = state.position;
    std::vector<ObservedPlayer> players{{55, 1, p.x + 100, p.y, p.z}};
    auto intent = ai.decide(1, 1, 1, p.x, p.y, p.z, .05f, players);
    require(ai.stateOf(1) == WildPokemonAIState::Battle && !intent.moving,
            "Player detection cancels the timed wandering movement on transition");
    intent = ai.decide(1, 1, 1, p.x, p.y, p.z, .05f, players);
    require(intent.attacking && intent.attackTargetId == 55, "Battle action executes the Lua-selected attack");
    intent = ai.decide(1, 1, 1, p.x, p.y, p.z, .05f, players);
    require(!intent.attacking, "Attack action cannot repeat every server tick");
    ai.decide(1, 1, 1, p.x, p.y, p.z, .05f, {});
    require(ai.stateOf(1) == WildPokemonAIState::Wander, "Lost target returns to Wander through Lua");

    // 추격 회귀 검사는 고정된 출발점에서 실행한다. 랜덤 배회의 종료 지형에 의존하지 않는다.
    state = {};
    state.position = initial;
    state.mode = hhv::movement::Mode::Grounded;
    accumulator = 0.f;
    const auto chaseStart = state.position;
    heaven::nav::Vec3 chaseGoal;
    bool foundGoal = false;
    for (int direction = 0; direction < 8 && !foundGoal; ++direction) {
        const float angle = direction * 6.2831853f / 8.f;
        foundGoal = map.canStandAt(chaseStart.x + std::cos(angle) * 320.f,
                                  chaseStart.y + std::sin(angle) * 320.f, map.agent(), &chaseGoal, chaseStart.z);
        foundGoal = foundGoal && !map.blockedAlong(chaseStart, chaseGoal, map.agent());
    }
    require(foundGoal, "Find a reachable chase destination on Filed");
    players = {{55, 1, chaseGoal.x, chaseGoal.y, chaseGoal.z}};
    bool chased = false;
    bool attacked = false;
    for (int tick = 0; tick < 120 && !attacked; ++tick) {
        const auto before = state.position;
        intent = ai.decide(1, 1, 1, before.x, before.y, before.z, .05f, players);
        map.advance(state, accumulator, .05f, {intent.targetX, intent.targetY, before.z},
                    intent.moving, 100.f);
        chased = chased || (intent.moving && hhv::movement::length(state.position - before) > .1f);
        if (intent.moving && hhv::movement::length(state.position - before) < .01f) {
            ai.notifyMoveBlocked(1);
        }
        attacked = intent.attacking;
    }
    if (!chased || !attacked) {
        std::cerr << "Chase diagnostic: moved=" << chased << ", attacked=" << attacked
                  << ", start=(" << chaseStart.x << ',' << chaseStart.y << ',' << chaseStart.z << ")"
                  << ", goal=(" << chaseGoal.x << ',' << chaseGoal.y << ',' << chaseGoal.z << ")"
                  << ", final=(" << state.position.x << ',' << state.position.y << ',' << state.position.z << ")\n";
    }
    require(chased && attacked, "Battle action follows a shared-core path and attacks after approaching");
}

} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 3, "AI test arguments are present");
        WildPokemonAIFSM fsm;
        require(!fsm.transitionTo(WildPokemonAIState::Wander), "Same-state requests are not transitions");
        testLuaDecisions(argv[1]);
        testLuaOwnsTransitions();
        testBadScriptResults();
        testTimedWandering();
        testSharedCoreMovement(argv[1], argv[2]);
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
