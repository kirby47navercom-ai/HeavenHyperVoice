#pragma once

// YANG2_CLIENT_AUTHORITY_ONLY
//
// Yang2 브랜치에서만 서버의 순수 C++ 날씨 계산기를 클라이언트에 연결한다.
// 이 파일이나 이를 포함하는 브릿지 변경을 main 으로 올리면 서버 권위가 깨진다.
// 자세한 금지 범위는 저장소 루트의 YANG2_ONLY.md 를 먼저 읽을 것.

#include "../../../../Server/InstanceServer/src/InstanceWeather.h"
