#pragma once

#include "MovementCore.h"
#include <cstddef>

namespace hhv::movement
{
// 입력 하나는 대기 입력을 포함해 항상 1/60초다. 좌표는 검증 자료로만 쓴다.
struct PredictedInput
{
	Input input;
	Vec3 position;
};

constexpr std::size_t MaxPendingInputs = 600;
constexpr std::size_t MaxInputBatch = 15;
}
