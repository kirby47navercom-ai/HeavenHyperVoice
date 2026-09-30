// YANG2_CLIENT_AUTHORITY_ONLY
#if !defined(HHV_YANG2_CLIENT_AUTHORITY_ONLY) || !HHV_YANG2_CLIENT_AUTHORITY_ONLY
#error "Offline Lua BT must not compile on main."
#endif
#include "../../../../Server/InstanceServer/src/WildBt.cpp"
