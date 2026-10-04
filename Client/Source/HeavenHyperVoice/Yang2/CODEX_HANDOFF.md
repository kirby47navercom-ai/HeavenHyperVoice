# Yang2 로컬 플레이 인계

YANG2_CLIENT_AUTHORITY_ONLY — main 병합 금지예요. main → Yang2 방향만 허용해요.

- 상태: 로컬 파트너 추종/공격 슬롯 수정, Editor 빌드 성공. 실제 플레이 확인은 사용자 담당이에요.
- 원인: `AddLocalPartner`의 액터 부착은 속도를 생성하지 않아 포켓몬 이동 AnimBP가 대기 상태에 남았어요.
- 흐름: 공통 충돌 → 백그라운드 Recast 준비 → 서버 PartnerFollower 20Hz → 기존 이동 보간 → 기존 AnimBP예요.
- 소유자: `Server/UEFieldPartnerSyncComponent.*`, `Yang2/UEYang2PartnerNavigation.cpp`예요.
- 재사용: `UEYang2ServerNavigation.cpp`에서 실제 서버 PartnerFollower.cpp를 포함해요.
  야생 시뮬레이션도 파트너의 맵을 공유하고 공통 길찾기 준비 후 시작해요.
- 입력: 기존 공격 슬롯 네 개와 브릿지 BP의 `PlayYang2PartnerFieldAnimation`을 사용해요.
- 확인: C++/UHT/링크 성공, 저장된 실제 종족 73개의 이동 애니메이션 참조와
  플레이어 남녀 AnimBP/DA 참조를 읽어 확인했어요. 에셋과 게임 레벨은 수정하지 않았어요.
- 검사: 기존 `Heaven.Weather.Yang2WildAI`에 공유 맵/추종 검사만 추가했고 실행하지 않았어요.
- 한계: 실제 이동 모션/장애물 경로/레벨 전환은 PIE에서 검증하지 않았어요.
  길찾기 준비 전 파트너는 숨겨져 있고, 없는 행동 시퀀스는 재생하지 않아요.
- 다음 확인: 게임에서 소환 후 이동·정지·장애물 우회·파트너 교체·포탈 이동을 확인해요.
  캐릭터 자체의 특정 모션 이상은 사용자가 동작을 알려주면 그 경로에서 진단해요.
- DEBUG_HANDOFF.md: 반복 실패한 게임 수정 가설이 없어 만들지 않았어요.
- 무관한 사용자 작업: 물대포 제작 스크립트 수정과 Jubilife 미추적 에셋/스크립트는 커밋하지 않아요.
