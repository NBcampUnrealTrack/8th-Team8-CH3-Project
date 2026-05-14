"""
Tank 스켈레탈 메시(SKM_Tank)로부터 UE 기본 FK Control Rig 에셋을 생성합니다.

사전 조건:
  - 에디터에서 Edit → Plugins → Python Editor Script Plugin 활성화 후 재시작 권장
  - Control Rig 플러그인 활성화(기본 UE)

실행 방법 (예):
  - 출력 로그(Output Log):  py "../../../../Scripts/ue/create_tank_fk_control_rig.py"
    (프로젝트 루트 기준 경로는 환경에 맞게 조정)

생성 후:
  - Level Sequence에서 바인딩에 + Track → Control Rig → Filter Asset By Skeleton ON 상태에서
    새 FK 리그가 목록에 나와야 합니다.
  - 여전히 안 보이면 SKM_Tank가 참조하는 Skeleton 이 에셋과 일치하는지 확인하세요.

참고: UE는 액터를 시퀀서에 드래그한다고 Control Rig 트랙을 자동으로 넣지 않습니다.
      마네킹은 CR_Mannequin 등 완성 에셋이 있어 추가가 쉬울 뿐입니다.
"""

import unreal

TANK_MESH_PATH = "/Game/Enemy/Tank/Base_mesh/SKM_Tank.SKM_Tank"


def main() -> None:
	mesh = unreal.EditorAssetLibrary.load_asset(TANK_MESH_PATH)
	if not mesh:
		unreal.log_error(f"[Tank FK Rig] 메시를 찾을 수 없음: {TANK_MESH_PATH}")
		return

	sk = mesh.get_editor_property("skeleton")
	if sk:
		unreal.log_warning(f"[Tank FK Rig] Skeleton: {sk.get_path_name()}")
	else:
		unreal.log_warning("[Tank FK Rig] 메시에 Skeleton 이 없습니다.")

	try:
		rig = unreal.ControlRigBlueprintFactory.create_control_rig_from_skeletal_mesh_or_skeleton(
			mesh,
			False,
		)
	except Exception as exc:
		unreal.log_error(f"[Tank FK Rig] 생성 실패: {exc}")
		return

	if not rig:
		unreal.log_error("[Tank FK Rig] 팩토리가 None 을 반환했습니다.")
		return

	path = rig.get_path_name()
	unreal.log_warning(f"[Tank FK Rig] 생성됨: {path}")

	try:
		unreal.EditorAssetLibrary.save_loaded_asset(rig)
	except Exception as exc:
		unreal.log_warning(f"[Tank FK Rig] 저장 시도 중 경고: {exc}")


main()
