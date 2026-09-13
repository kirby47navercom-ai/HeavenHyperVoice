-- 상태별 BT 진입점. C++는 현재 FSM 상태에 해당하는 함수 하나만 호출한다.
wild_ai = {
    wander = require("wild_pokemon_ai.wander"),
    battle = require("wild_pokemon_ai.battle")
}
