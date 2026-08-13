#include "UEGameplayTags.h"
namespace UEGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(Input_Action_MoveForward, "Input.Action.MoveForward");
	UE_DEFINE_GAMEPLAY_TAG(Input_Action_MoveBackward, "Input.Action.MoveBackward");
	UE_DEFINE_GAMEPLAY_TAG(Input_Action_MoveRight, "Input.Action.MoveRight");
	UE_DEFINE_GAMEPLAY_TAG(Input_Action_MoveLeft, "Input.Action.MoveLeft");
	UE_DEFINE_GAMEPLAY_TAG(Input_Action_LookYaw, "Input.Action.LookYaw");
	UE_DEFINE_GAMEPLAY_TAG(Input_Action_LookPitch, "Input.Action.LookPitch");
	UE_DEFINE_GAMEPLAY_TAG(Input_Action_Run, "Input.Action.Run");
	UE_DEFINE_GAMEPLAY_TAG(Input_Action_Roll, "Input.Action.Roll");
	UE_DEFINE_GAMEPLAY_TAG(Input_Action_Jump, "Input.Action.Jump");
	UE_DEFINE_GAMEPLAY_TAG(Input_Action_SpawnPokemon, "Input.Action.SpawnPokemon");

	//UE_DEFINE_GAMEPLAY_TAG(Event_Montage_Begin, "Event.Montage.Begin");
	//UE_DEFINE_GAMEPLAY_TAG(Event_Montage_End, "Event.Montage.End");
	//UE_DEFINE_GAMEPLAY_TAG(Event_Montage_Attack, "Event.Montage.Attack");

	UE_DEFINE_GAMEPLAY_TAG(State_Character_Idle, "State.Character.Idle");
	UE_DEFINE_GAMEPLAY_TAG(State_Character_Walk, "State.Character.Walk");
	UE_DEFINE_GAMEPLAY_TAG(State_Character_Run, "State.Character.Run");
	UE_DEFINE_GAMEPLAY_TAG(State_Character_Roll, "State.Character.Roll");
	UE_DEFINE_GAMEPLAY_TAG(State_Character_Jump, "State.Character.Jump");
	UE_DEFINE_GAMEPLAY_TAG(State_Character_Fall, "State.Character.Fall");

	//UE_DEFINE_GAMEPLAY_TAG(Ability_Attack, "Ability.Attack");
}
