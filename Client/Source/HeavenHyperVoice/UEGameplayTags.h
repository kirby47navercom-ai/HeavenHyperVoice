#pragma once
#include "NativeGameplayTags.h"

namespace UEGameplayTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Action_Move);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Action_LookYaw);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Action_LookPitch);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Action_Run);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Action_Roll);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Action_Jump);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Action_SpawnPokemon);

	//UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Montage_Begin);
	//UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Montage_End);
	//UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Montage_Attack);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Character_Idle);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Character_Walk);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Character_Run);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Character_Roll);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Character_Jump);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Character_Fall);

	//UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack);
}
