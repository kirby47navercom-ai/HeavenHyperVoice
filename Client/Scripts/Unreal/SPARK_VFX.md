# 스파크 VFX

에셋 폴더: `/Game/VFX/Pokemon/Spark`

| 에셋 | 용도 |
|---|---|
| `NS_Spark_Aura` | 몸 둘레에서 짧은 3D 번개 가지가 번갈아 튀는 반복 효과. 작은 전기 입자와 약한 노란 광채 포함. |
| `NS_Spark_Impact` | 적중 순간의 섬광, 번개 가지, 사방으로 흩어지는 전기와 잔광. 1회 재생. |
| `BP_Spark_Aura` / `BP_Spark_Impact` | 각 Niagara 시스템을 담은 배치용 블루프린트. 피격 액터는 1.5초 뒤 제거. |
| `L_Spark_Showcase` / `LS_Spark_Preview` | 두 효과를 함께 보는 맵과 2초 미리보기 시퀀스. |

## 사용

- 돌진 시작 시 `NS_Spark_Aura`를 `Spawn System Attached`로 포켓몬 몸통 소켓에 붙인다. 효과 원점은 **몸 중심**이다. 발밑 루트에 붙일 경우 Relative Location.Z를 몸통 높이만큼 올린다.
- 몸을 두르는 번개의 기본 반경은 약 **56cm**다. Niagara 컴포넌트의 Relative Scale로 포켓몬 크기에 맞춘다. 길쭉한 몸은 X/Y/Z 크기를 각각 조절할 수 있다. 공격 효과는 로컬 공간에서 재생하므로 부착된 몸을 따라간다.
- 돌진이 끝나거나 취소되면 해당 컴포넌트를 `Deactivate`한다. 남은 입자의 최대 수명은 0.22초다. 즉시 숨겨야 하면 `Destroy Component`를 사용한다. `BP_Spark_Aura` 액터로 붙였다면 사용 후 `Destroy Actor`한다.
- 서버에서 적중이 확정되면 `NS_Spark_Impact`를 적중 위치에 `Spawn System at Location`으로 별도 생성하고 Auto Destroy를 켠다. 대부분의 타격 번개는 0.29초 이내, 잔입자는 최대 0.48초에 사라진다.
- 이동, 충돌, 데미지, 기술 호출은 포함하지 않았다. 두 효과가 서로를 자동 생성하지 않는다.

## 편집

- 몸 효과: `BodyArcs` / `SurfaceFlecks` / `ChargeGlow`.
- 피격 효과: `ContactArcs` / `ContactFlash` / `DischargeBolts` / `ScatteredCharge` / `ImpactGlow`.
- Niagara에서 각 레이어의 생성량, 수명, 크기, 피격 입자의 속도를 조절한다.
- `Meshes/SM_Spark_BranchedArc`는 짧은 가지가 있는 3D 번개 메시다. `Initial Mesh Orientation`의 Random으로 번개가 몸의 여러 방향에 나타난다.
- `Materials/M_Spark_Arc`의 `CoreColor`, `EdgeColor`, `Brightness`로 번개 색과 밝기를 조절한다. Material Instance를 만들면 원본을 유지하며 변형할 수 있다.
- 나머지 `M_Spark_*` 머티리얼은 편집 가능한 노드와 Custom 마스크로 구성되어 있다. 별도 원본 이미지 파일이 필요하지 않다.

## 미리보기

`L_Spark_Showcase`를 열고 `LS_Spark_Preview`를 Sequencer에서 재생한다. 60fps 기준 몸 효과는 0프레임부터, 피격은 36프레임부터 재생한다. **42프레임**에서 두 효과를 함께 볼 수 있다. 맵만 열면 피격이 이미 끝나 보일 수 있으므로 시퀀스를 사용한다.

제작 스크립트는 `create_spark_vfx.py`다. 기존 `create_water_gun_vfx.py`와 `UEWaterVFXEditorLibrary`를 재사용한다. 기존 Spark 에셋이 있으면 덮어쓰지 않고 중단한다. Niagara 컴파일과 에디터 외형을 확인했으며 PIE·전투 테스트는 하지 않았다.
