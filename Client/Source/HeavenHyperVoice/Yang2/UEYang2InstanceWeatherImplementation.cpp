// YANG2_CLIENT_AUTHORITY_ONLY
//
// 복사본을 만들지 않고 InstanceServer의 실제 구현을 이 클라이언트 모듈에서도
// 한 번 컴파일한다. main -> Yang2 로 서버 계산을 가져오면 이 로컬 실행도 같은
// 수식으로 갱신된다. 이 파일은 main에 합치면 안 된다.

#if !defined(HHV_YANG2_CLIENT_AUTHORITY_ONLY) || !HHV_YANG2_CLIENT_AUTHORITY_ONLY
#error "Yang2 client authority source must never compile without the Yang2-only build flag."
#endif

#include "../../../../Server/InstanceServer/src/InstanceWeather.cpp"
