#pragma once

// FlatBuffers adapter shared by the Unreal transport and both server processes.
#include "MovementPrediction.h"
#include "field_generated.h"

namespace hhv::movement::wire
{
inline flatbuffers::Offset<HeavenField::CoreState> encodeState(flatbuffers::FlatBufferBuilder &builder,
                                                               const State &state)
{
	std::vector<flatbuffers::Offset<HeavenField::WaterRegion>> regions;
	for(const auto& r:state.environment.water)
		regions.push_back(HeavenField::CreateWaterRegion(builder,r.minX,r.minY,r.maxX,r.maxY,r.seaLevelCm,r.swimDepthCm,r.canSwim));
	const auto environment=HeavenField::CreateMovementEnvironment(builder,state.environment.speedMultiplier,
		state.environment.traction,state.environment.tideOffsetCm,state.environment.swimSpeed,builder.CreateVector(regions));
	HeavenField::CoreStateBuilder value(builder);
	value.add_px(state.position.x);
	value.add_py(state.position.y);
	value.add_pz(state.position.z);
	value.add_vx(state.velocity.x);
	value.add_vy(state.velocity.y);
	value.add_vz(state.velocity.z);
	value.add_ax(state.acceleration.x);
	value.add_ay(state.acceleration.y);
	value.add_az(state.acceleration.z);
	value.add_nx(state.floorNormal.x);
	value.add_ny(state.floorNormal.y);
	value.add_nz(state.floorNormal.z);
	value.add_facing(state.facing);
	value.add_mode(static_cast<std::uint8_t>(state.mode));
	value.add_roll_ticks(state.rollRemaining);
	value.add_roll_x(state.rollDirection.x);
	value.add_roll_y(state.rollDirection.y);
	value.add_roll_z(state.rollDirection.z);
	value.add_wall_sliding(state.wallSliding);
	value.add_environment(environment);
	return value.Finish();
}

inline State decodeState(const HeavenField::CoreState &value)
{
	State state;
	state.position.x = value.px();
	state.position.y = value.py();
	state.position.z = value.pz();
	state.velocity.x = value.vx();
	state.velocity.y = value.vy();
	state.velocity.z = value.vz();
	state.acceleration.x = value.ax();
	state.acceleration.y = value.ay();
	state.acceleration.z = value.az();
	state.floorNormal.x = value.nx();
	state.floorNormal.y = value.ny();
	state.floorNormal.z = value.nz();
	state.facing = value.facing();
	state.mode = static_cast<Mode>(value.mode());
	state.rollRemaining = value.roll_ticks();
	state.rollDirection.x = value.roll_x();
	state.rollDirection.y = value.roll_y();
	state.rollDirection.z = value.roll_z();
	state.wallSliding = value.wall_sliding();
	if(const auto* env=value.environment()) {
		state.environment.speedMultiplier=env->speed_multiplier();
		state.environment.traction=env->traction();
		state.environment.tideOffsetCm=env->tide_offset_cm();
		state.environment.swimSpeed=env->swim_speed();
		if(env->water() && env->water()->size()<=64) for(const auto* r:*env->water())
			state.environment.water.push_back({r->min_x(),r->min_y(),r->max_x(),r->max_y(),r->sea_level_cm(),r->swim_depth_cm(),r->can_swim()});
	}
	return state;
}

inline auto encodeInputs(flatbuffers::FlatBufferBuilder &builder, const std::vector<PredictedInput> &batch)
{
	std::vector<flatbuffers::Offset<HeavenField::CoreInput>> entries;
	entries.reserve(batch.size());
	for (const auto &frame : batch)
	{
		entries.push_back(HeavenField::CreateCoreInput(builder, frame.input.sequence, frame.input.x,
		                                               frame.input.y, frame.input.buttons, frame.position.x,
		                                               frame.position.y, frame.position.z));
	}
	return builder.CreateVector(entries);
}

inline std::vector<PredictedInput> decodeInputs(const HeavenField::Move &move)
{
	std::vector<PredictedInput> result;
	if (!move.inputs() || move.inputs()->size() > MaxInputBatch)
	{
		return result;
	}

	for (const auto *frame : *move.inputs())
	{
		result.push_back({{frame->sequence(), frame->x(), frame->y(), frame->buttons()},
		                  {frame->predicted_x(), frame->predicted_y(), frame->predicted_z()}});
	}
	return result;
}
} // namespace hhv::movement::wire
