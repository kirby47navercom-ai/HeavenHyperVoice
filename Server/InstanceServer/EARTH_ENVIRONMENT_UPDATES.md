# 환경 계산 보완과 실제 게임 연결

기존 평균 기단 모델을 유지하면서 시간, 물 수지, 지표 에너지와 게임 규칙을 보완했다.
main은 InstanceServer가 계산하고 클라이언트는 표시한다. Yang2는 같은 순수 C++ 계산을
로컬에서 실행하는 별도 어댑터를 사용한다. 이 문서의 변경은 지구과학 기능에 한정한다.
날씨 판단에 추가한 퍼지 규칙은 [퍼지 날씨 안내](FUZZY_WEATHER_GUIDE.md)를 참고해요.
기존 시간·저장소는 유지하고 강수·눈 비중·안개·먼지 목표를 규칙으로 정해요.

## 계산이 돌아가는 순서

`RoomManager::tickShard`는 방마다 현실 약 1초에 한 번 `InstanceWeather::advance`를 호출한다.
계산기는 그 시간을 10초짜리 게임 시간 단계로 나눠 아래 순서로 처리한다.

1. 공용 세계 시간으로 낮밤, 계절, 태양 방향을 갱신한다.
2. 지역 기단의 기온·기압·풍속을 갱신한다.
3. 일사 흡수·장파 복사·대기 열 교환으로 지표 온도를 계산한다.
4. 외부 기단과 교환하는 수증기를 질량 장부에 기록한다.
5. 표면 물막/지표수/토양수 증발 → 응결 → 비·눈 → 융해/동결 → 침투/배수를 계산한다.
6. 실제 증발/융해량에 사용된 잠열을 지표 온도에 반영한다.
7. 사리·조금, 밀물·썰물, 바람 파고, 모래폭풍을 계산한다.
8. 최종 젖음·얼음·폭풍으로 야생 이동/탐지 배율을 정하고 패킷을 보낸다.

## 수정하거나 추가한 계산 파일

| 파일 | 역할과 의미 |
|---|---|
| `WorldEnvironmentClock.h` | `steady_clock`으로 서버 세션 경과 시간을 읽는다. 방을 새로 만들거나 재입장해도 낮밤은 처음부터 시작하지 않는다. |
| `EnvironmentClock.cpp` | 공용 시간에서 낮/연중 진행률과 위도별 태양 방향을 얻는다. 방별 날씨의 나이는 별도로 유지한다. |
| `SurfaceEnergyBalance.cpp` | 햇빛 반사·구름 차광·장파 복사·대기 열 교환·증발/융해 잠열을 지표 온도에 반영한다. |
| `AtmosphericBoundary.cpp` | 주변 기단의 습도와 방의 수증기 차이를 천천히 줄인다. 들어온 물과 나간 물을 각각 기록한다. |
| `SurfaceWaterCycle.cpp` | 얇은 표면 물막을 토양과 분리하고, 실제 옮긴 물만 증발·융해 열 계산에 사용한다. |
| `CoastalEnvironment.cpp` | 짧은 밀물/썰물 주기에 긴 사리/조금 진폭 주기를 겹친다. 조석 진폭이 0이어도 파도는 가능하다. |
| `WeatherSnapshot.cpp` | 화면 젖음은 물막/지표수로, 토양 수분은 토양 용량 비율로 따로 전송한다. 개방된 물 수지를 검사한다. |
| `EnvironmentGameplayRules.h/.cpp` | 비·눈·밤의 생성 가중치, 젖음/얼음의 야생 이동 배율, 폭풍의 탐지 배율을 계산한다. |
| `EnvironmentConfig.cpp` | 기존 수치와 `spawn.도감번호=기본,비,눈,밤` 설정을 읽고 오류/중복/없는 종족을 거절한다. |

물 수지 오차는 `현재 저장량 - 초기 저장량 + 토양 배수 + 외부 유출 - 외부 유입`이다.
바다는 이 방 평균 대기/지표 저장소와 다른 외부 저장소이며 바다 전체 질량을 여기에 합산하지 않는다.
`SurfaceHeatFluxWm2`는 복사/현열의 순 유입이다. 잠열은 별도 단계에서 반영한다.

공용 시계 설정 다섯 가지인 `GameSecondsPerRealSecond`, `DaySeconds`, `YearDays`,
`StartHour`, `StartYearFraction`은 서버에 등록된 모든 인스턴스 종류에서 같아야 한다.
서버 시작 시 서로 다른 설정을 거절한다. 위도·고도·기온·풍속은 지역마다 달라도 된다.
서버 재시작을 넘어 시간을 보존하는 DB/파일 저장은 아직 없다.

## 데이터 에셋에서 조절할 새 값

기존 `/Game/VFX/Weather/DA_Environment_*`에 추가했다. 새 콘텐츠 최상위 폴더는 만들지 않는다.

| 속성 | 조절할 것 |
|---|---|
| `SurfaceFilmCapacityKgM2` | 젖음 100%에 해당하는 얇은 물막 용량. 기본 0.2 mm. 흙 속 물만으로 바닥이 반짝이지 않는다. |
| `MoistureExchangeSeconds` | 외부 수분 교환 시간. 기본 게임 6시간. 작으면 지역 기준 습도로 빨리 돌아간다. |
| `SolarPeakWm2` / `SurfaceAlbedo` | 최대 일사 / 반사율. 밝은 표면은 열 흡수가 줄어든다. |
| `SurfaceEmissivity` / `ClearSkyCoolingWm2` | 장파 복사 방출 / 맑은 하늘 냉각. 구름은 냉각을 줄인다. |
| `AirHeatTransferWm2K` / `ThermalResponseSeconds` | 공기와의 열 교환 / 지표 열용량의 반응 시간. 물은 완만하고 모래는 빠르게 설정한다. |
| `SpringNeapPeriodDays` / `NeapTideFraction` / `SpringNeapPhaseDegrees` | 사리 주기 / 조금의 상대 진폭 / 시작 위상. 기본 14.765일, 0.5, 0도. |
| `ShoreHeightM` | 기준 해수면 대비 관찰할 해변의 높이. `ShoreWaterDepthM=max(0,조석-해변높이)`. 지형 전체의 침수 지도가 아니다. |
| `WetMovementMultiplier` / `IceMovementMultiplier` | 최대 젖음/얼음에서 야생 이동 배율. 1이면 끈다. |
| `SandVisibilityMultiplier` | 최대 모래폭풍에서 야생 탐지 범위 배율. 1이면 끈다. |
| `SpawnRules` | 도감번호, 기본 가중치, 비/눈/밤 배율. 기존 서버 종족 목록 안에서만 확률이 바뀐다. |

예시 DA에는 팽도리의 비 배율, 꽁어름의 눈 배율, 귀뚤뚜기의 밤 배율 등이 저장되어 있다.
이는 코드에 고정된 규칙이 아니다. 배열을 비우거나 배율을 1로 바꾸면 기존 가중치로 돌아간다.
설정 수정 뒤 `export_environment_profiles.py`를 실행하고 해당 ini를 서버 실행 옵션
`--instance-environment 종류번호=파일`로 연결한다. 기존 30개 수치에 14개 수치와 가중치 배열이 더해졌다.

## 실제 적용되는 게임 동작

`RoomManager::createRoomLocked`의 최초 야생 생성에서 `std::discrete_distribution`으로
환경 가중치를 적용한다. 가중치가 모두 0이면 생성하지 않는다. 이미 생성된 포켓몬을
밤/비가 바뀔 때 지우거나 교체하지 않는다. 이후 리스폰은 별도 시스템이 생기면 연결한다.

`World::advanceWild`는 서버가 계산한 이동 배율을 야생의 기존 이동 속도에 곱한다.
`WildAi`가 탐지 배율을 Lua BT로 전달하여 배회 중 발견 범위와 전투 중 추적 유지 범위를 줄인다.
플레이어 속도·미끄러짐·기술 대미지는 아직 바꾸지 않는다. 플레이어 예측 이동과 서버 판정이
같은 입력을 사용하도록 맞춘 뒤 추가해야 한다.

## 클라이언트 비용과 생명주기 수정

- 비/눈은 기본 종류당 16개 지속 Niagara 열에서 방출한다. 합계 최대 64개로 제한한다.
- 충돌 피격은 기본 초당 12개 CPU 경로만 샘플링한다. 입자마다 서버 복제/시스템 생성하지 않는다.
- 눈의 긴 수명을 유지하며 플레이어가 충분히 이동할 때 열을 다시 배치한다.
- 낙하 열의 경로는 배치 시 지붕/배제 영역을 검사한다. 충돌 샘플은 피격 직전 다시 검사한다.
- `OceanUpdateSeconds` 기본 0.5초, 수위 오차 기본 1cm, 파고 오차 2cm, 방향 오차 2도로 물 구역/GPU 재구성을 제한한다.
- 연결 해제는 공통 `ClearInstanceWeatherState`에서 알린다. 빛·안개·수면까지 원래 레벨 상태로 복구한다.

지속 낙하와 피격은 같은 기상 값을 사용하는 별도 시각 샘플이다. 모든 낙하 입자가 개별 동적 물체와
완전히 동일하게 충돌하는 구현은 아니다. 동적 지붕/물체가 많아지면 GPU Collision 이미터로 확장한다.
실제 플레이·멀티플레이·장시간 부하 측정은 실행하지 않는다. 컴파일/순수 계산/저장된 에셋 검증과 구분한다.

## 2026-09-30: 재시작·플레이어 이동·해안·리스폰 연결

- `WeatherPersistence.cpp`: 공기 두 층, 지표 물/토양/물막/눈/얼음, 누적 물수지, 기후 위상, 물리 시간, 파도/모래 강도까지 저장해요. 같은 설정으로 복구하면 다음 계산도 같아요.
- `EnvironmentCheckpoint.cpp`: 체크섬, 임시 파일 교체, 정상 이전 파일 `.bak`으로 손상된 최신 저장을 복구해요. 둘 다 손상되면 기동을 멈추고 원본을 보존해요.
- `RoomEnvironmentPersistence.cpp`: 세계 경과 시간, 방 종류/번호, 다음 방 번호, 각 기후를 복구해요. 재접속을 기다리는 복구 방은 최초 입장 전에는 회수하지 않아요. 플레이어 접속/전투/야생 위치를 복구하는 저장은 아니에요.
- `main.cpp`: 별도 저장 스레드가 현실 30초마다 캡처·저장하고 정상 종료에는 마지막 상태를 저장해요. 디스크 I/O 중에는 방 잠금을 잡지 않아요. 비정상 종료는 마지막 저장 이후 최대 약 30초를 잃을 수 있어요.
- 서버가 꺼진 동안 세계 시간과 기후는 **정지**해요. 재기동 후 저장 시점에서 이어져요. 물리 설정이 바뀐 저장은 거절하므로 의도적으로 초기화할 때 저장 파일과 백업을 별도 보관한 뒤 새 경로로 실행하세요.

기본 저장은 실행 작업 폴더의 `state/instance-environment.state`예요. 운영에서는
`--environment-state <영구 저장 위치>`로 고정하세요. 여러 서버 프로세스가 같은 파일을 공유하면 안 돼요.
실행 상태 파일은 Git에 포함하지 않아요.

### 플레이어 예측과 수영

`MovementEnvironment.h`의 속도·얼음 마찰·수위·수영 속도·수역 목록을 서버가 정해요.
`World`가 공통 60Hz 이동 코어에 넣고 `MovementWire`가 EnterAck와 보정 상태로 보내요.
클라이언트는 보정에 들어온 환경으로 미확인 입력을 다시 재생하고 다음 예측에도 사용해요.
클라이언트 입력에는 환경 계수/수역을 싣지 않으므로 클라이언트가 수영 구역이나 마찰을 조작할 수 없어요.
코어 버전은 **4**예요. FieldServer·InstanceServer·클라이언트를 함께 재빌드해야 해요.

- 젖음/얼음 이동 배율은 플레이어에게도 적용해요. `IceTractionMultiplier`는 얼음에서 가속·제동·마찰을 줄여 미끄러짐을 만들어요.
- `WaterRegions`는 DA의 실제 XY 범위, 기준 수위(cm), 수영 진입 깊이(cm), 수영 허용 여부예요. 서버 설정에는 `water.0=minX,minY,maxX,maxY,seaLevelCm,swimDepthCm,0또는1`로 내보내요. 겹치는 수역은 거절해요.
- 수심은 현재 수위(기준+조석) 아래의 **공통 충돌 지형**을 검사해요. `GetWaterDepthCm()`와 `IsSwimming()`으로 BP에서도 읽어요. 파도는 시각 연출이고 수영의 판정 수위에는 평균 조석을 사용해요.
- 깊이와 플레이어 잠김 정도가 충분하면 `Swimming`으로 전환해요. 수면을 따라 이동하고 달리기/구르기는 실행되지 않아요. 이탈 깊이에 여유를 주어 수면에서 모드가 깜빡이지 않아요.
- 수영 불가 수역은 깊어지는 진입을 막아요. 밀물로 이미 잠긴 개체는 같거나 얕은 곳으로 탈출할 수 있어요. 텔레포트로 이동 상태를 초기화할 때도 현재 방의 환경 입력은 유지해요. 현재 육상 야생 AI도 깊은 물에는 생성하거나 이동하지 않아요. 잠수·수영 애니메이션·스태미나 규칙은 별도 게임 작업이에요.

`L_Environment_Coast`에는 육지, 얕은 해변, 해저를 실제 배치했어요. 기존
`DA_Environment_Coast`에 수역을 연결했고 서버와 클라이언트의 충돌을 같은 에디터 도구로 내보냈어요.
예제 등록은 `--instance-map <종류>=maps/collision/VFX/Weather/L_Environment_Coast.hhvcollision`와
`--instance-environment <종류>=config/environment/DA_Environment_Coast.ini`를 함께 사용하세요.
졸작의 다른 해안은 해당 레벨의 실제 수역을 DA에 지정하고 충돌을 내보내야 해요.

### 환경 기반 야생 리스폰

`RoomWildRespawn.cpp`의 고정 슬롯은 0 HP 또는 이미 제거된 야생을 회수해요.
`WildRespawnSeconds` 뒤 **그 순간의 비·눈·밤**으로 종족 가중치를 다시 계산해요.
최초 생성도 같은 경로를 사용해요. 모든 가중치가 0이면 생성하지 않고 기다려요.
충돌 없는 위치 탐색은 한 번에 16회, 실패하면 1초 뒤 다시 시도해요.
AI의 과거 타깃/배회 경로는 슬롯 회수 때 `forget()`으로 지워요.
전투/포획 구현은 성공 시 `World::setWildCurrentHp(id,0)`으로 확정할 수 있어요.

## 지구과학 엔진의 범위와 후속 확인

### 얼음·서리와 해안 거품 에셋

실제 편집 가능한 에셋은 기존 `/Game/VFX/Weather` 안에 만들었어요.
게임 맵의 머티리얼을 일괄 교체하거나 해안을 새로 배치하지 않았어요.

- `Materials/MF_EnvironmentIce`: 기존 BaseColor/Roughness/WorldNormal을 받아 얼음 균열과 서리를 섞는 공용 함수예요. Exposure로 적용 영역을 제한해요. 기존 머티리얼의 출력 앞에 연결할 수 있어요.
- `Materials/M_Environment_Ice`: 위 함수의 기본 머티리얼이에요. `MI_Environment_ClearIce`, `MI_Environment_Frost`는 얼음/서리 비중이 다른 인스턴스예요. `CellSizeCm`은 균열 텍스처의 월드 반복 크기(cm), `FrostAmount`는 서리 비중, `FullThicknessMm`는 완전히 얼어 보이는 물 환산 얼음 두께예요. `Override=-1`이 환경 연동이고 0~1은 수동 미리보기예요.
- `Materials/M_Environment_ShoreFoam`: 기존 Fab `T_Ocean_Foam`의 큰 거품/미세 포말 마스크를 겹친 머티리얼이에요. `FoamTint`, `FoamOpacity`, `FoamRoughness`를 조정해요. 장면 조명을 받아 낮/밤의 밝기가 맞춰져요. 파고가 거의 0이면 투명해지고 조석의 상대 높이(m)를 월드 cm로 바꾸어 수면을 따라 이동해요.
- `NS_Environment_ShoreFoam`: FoamLace/SmallBubbles/SeaSpray 세 Lightweight 이미터가 퍼지면서 커지고 서서히 소멸해요. 물결 주기에 따라 로컬 +Y 방향으로 전진/후퇴해요. SeaSpray는 작은 반투명 물방울이며 포말이 전진하는 구간에만 보이는 약한 물보라예요. 위치, 방출 상자 크기, 크기/속도/수명/방출량은 Niagara에서 수정해요.
- `BP_EnvironmentShoreFoam`: 해당 시스템을 지정한 배치용 NiagaraActor BP예요. 해안선의 평균 수면에 놓고 회전/스케일로 방향·폭을 맞춰요. 기준 조석이 0인 수면 높이에 두어야 조석을 이중 적용하지 않아요.

`UEInstanceWeatherDirector::ApplyParameters()`가 보간된 `IceMm`, `GroundTemperatureC`,
`WaveHeightM`, `TideLevelM`를 기존 `MPC_InstanceWeather`에 전달해요. 환경을 비활성화하면
얼음/파도/조석은 0, 지표 온도는 20으로 돌려요. main은 서버 상태를 사용하고 Yang2는
기존 로컬 계산 상태가 같은 연출 경로로 들어가요. 입자 위치를 서버에 복제하지 않아요.

`Client/Scripts/Unreal/create_ice_shore_assets.py`는 에디터 제작 스크립트이고 이미 있는
에셋의 작가 수정은 보존해요. 명시적으로 `--refresh`를 넘기면 이 스크립트가 만든 얼음/거품 그래프와 이미터만 재작성해요. 공용 함수의 기존 입출력 ID는 유지해요. `Shaders/EnvironmentIce.ush`, `EnvironmentFoam.ush` 내용은
에셋의 Custom 노드에 저장돼요. 게임 실행 시 Python/외부 shader 파일을 읽지 않아요.
`.ush` 수정은 `--refresh`로 반영하거나 에디터 Custom 노드에서 직접 수정해요.

`preview_ice_shore_assets.py`는 임시 레벨에서 실제 에셋을 렌더해
`Client/Saved/Codex/EarthScienceAssets`에 PNG를 저장해요. 미리보기는 얼어붙은 수면과 서리 바위, 경사진 모래 해안 두 장면이에요. 바다는 미리보기용 표면이고 실제 Water Body의
파도/충돌 판정은 바꾸지 않아요. 이 거품은 배치한 해안 구간용 시각 효과예요.
자동 해안선 추출, 파도 쇄파 물리, 물 위의 실제 얼음 지형 생성은 포함하지 않았어요.
2026-10-01 시각 개선: 큰 셀 경계 무늬를 제거하고 생성한 미세 균열/기포와 서리 결정 마스크를
사용해요. `T_Environment_IceFractures`, `T_Environment_FrostCrystals`는 같은 Materials 폴더에
있어요. `IceFractureTexture`, `FrostCrystalTexture`를 MI에서 교체할 수 있어요.
원본 PNG는 Git 제외된 `Client/SourceArt/Weather`에 보관하고 가져온 uasset만 커밋해요.
제작 프롬프트는 `Client/Scripts/Unreal/weather_texture_prompts.txt`에 있어요. 이미지 생성 도구로
만든 마스크이며 사진 스캔 데이터는 아니에요. 최종 미리보기는 생성 이미지가 아니라 Unreal 렌더예요.
균열의 두 층은 시점에 따라 서로 어긋나는 시차를 보여주고, 서리 높이 기울기가 노멀을 바꿔요.
큰 흰 피격 flipbook을 해안에 연속으로 띄우던 방식은 제거했어요. `SeaSpray`는 1.2~3cm
방울이 0.25~0.5초 동안 낮게 튀고 중력으로 내려오는 방식이에요. 방울 크기는 BP의 폭 스케일로
늘리지 않아요. `SprayTint`, `SprayOpacity`, `SprayRoughness`로 물방울 재질을 조절해요.
포말은 방출량/수명/불투명도를 낮추고 모래 교차 지점을 Depth Fade로 부드럽게 연결해요.
`FoamIntersectionFadeCm`으로 교차 경계의 페이드 길이를 조절해요.
이것도 배치용 시각 효과이며 실제 파도 충돌/쇄파 판정은 하지 않아요.

Clear Coat와 `IceCoatRoughness`로 얼음 반사를 조절해요.
`SurfaceTexture`/`SurfaceNormal`과 각 Weight를 MI에서 지정하면 원래 바위 무늬를 보존해요.
마스크는 선형 색 공간으로 읽고 엔진의 PowerOfTwo 빌드 옵션으로 mip/스트리밍을 사용해요. 이는 기존 불투명 표면 위의 시각 코팅이며
실제 두께를 가진 투명 얼음/유체 시뮬레이션은 아니에요.

기존 에디터 라이브러리의 `TickWaterPreview`는 이 비게임 레벨에서만 컴포넌트와
MPC 렌더 버퍼를 갱신하고 실제 생성 입자 수를 반환해요. PIE/게임 월드는 거절해요.
미리보기 스크립트는 BP의 실제 배치 인스턴스 연결과 입자 생성도 확인해요.

전투·포획 판정, 수영 애니메이션·스태미나·익사, 다른 해안의 신규 제작/수역 배치는
이 엔진의 필수 후속 작업으로 취급하지 않아요. 게임 규칙은 사용자가 별도로 정할 때 연결해요.
기존 맵/에셋의 실제 오류는 해당 오류만 수정해요.

엔진 자체의 후속 확인은 방 수가 증가할 때 계산 시간·패킷 크기·저장 비용을 측정하는 것,
운영용 저장의 소유권·백업 보관·저장 실패 모니터링을 정하는 것이에요.
한 인스턴스 내부에서도 위치별 날씨가 필요해질 때만 공간 셀과 수평 수분 이동을 추가해요.
실제 멀티플레이나 부하 실행은 이번 경고 수정에서 진행하지 않아요.

### Plain 식생 연결 경고 수정

`UEVegetationScatterActor`의 부모 Root는 Movable 기본값인데 생성한 식생 자식은 Static이라
부착이 취소됐어요. 부모 기본값을 Static으로 바꾸고 자식도 부모의 Mobility를 따르게 했어요.
BP에서 부모 이동성을 바꾸더라도 같은 문제가 반복되지 않아요.
`verify_environment_scenes.py`는 실제 저장된 Plain 식생의 부모 연결과 이동성을 확인해요.

기존 공통 충돌 파일 네 쌍은 서버/클라이언트 내용, 삼각형 수, 유한한 좌표, 저장 해시가
일치하는지 확인했어요. 이 확인은 모든 지형의 플레이 가능성이나 모든 오브젝트의 충돌 배치를
보증하는 플레이 검증은 아니에요. 이 수정에서 새 수역이나 충돌 지형을 추가하지 않았어요.

열/수분 계산은 게임용 평균 근사다. 증발 잠열의 2.45 MJ/kg 근사는
[FAO 기상 자료](https://www.fao.org/4/x0490e/x0490e07.htm)를 참고했다.
영하 포화 수증기압은 [UCAR 수증기압 공식](https://www.eol.ucar.edu/data-software/conventions-and-standards/water-vapor-pressure-formulations)의 얼음 위 Magnus 근사를 사용한다.
이 식을 사용했다고 기상 예보 정확도가 검증된 것은 아니다.
