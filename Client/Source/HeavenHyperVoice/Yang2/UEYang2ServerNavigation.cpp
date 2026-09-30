// YANG2_CLIENT_AUTHORITY_ONLY: 공통 충돌과 서버의 실제 Recast/Detour 경로를 사용해요.
#if !defined(HHV_YANG2_CLIENT_AUTHORITY_ONLY) || !HHV_YANG2_CLIENT_AUTHORITY_ONLY
#error "Offline navigation must not compile on main."
#endif
#include "../../../../Server/Movement/src/Map.cpp"
#include "../../../../Server/Movement/src/Path.cpp"
#include "../../../../Server/Movement/src/NavigationMesh.cpp"
