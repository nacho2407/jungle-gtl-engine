DirectX 11과 ImGui를 기반으로 구현한 간단한 3D 에디터 프로토타입.
---
**클래스 계층도**

UObject<br />
├─ AActor<br />
│  ├─ ACamera<br />
│  ├─ AGizmoActor<br />
│  └─ APrimitiveActor<br />
│     ├─ APlane<br />
│     ├─ ACube<br />
│     ├─ ASphere<br />
│     └─ ATorus<br />
│<br />
├─ UActorComponent<br />
│  └─ USceneComponent<br />
│     ├─ UCameraComponent<br />
│     ├─ UGizmoComponent<br />
│     └─ UPrimitiveComponent<br />
│        └─ UStaticMeshComponent<br />
│<br />
├─ UStaticMesh<br />
└─ UWorld<br />

<br />
App : 애플리케이션의 실행 흐름을 관리하는 최상위 계층<br />
	- 프로그램 초기화, 메인 루프 실행, 입력 처리, UI 패널 호출, 오브젝트 선택 및 기즈모 조작, 씬 저장/로드 제어<br />
<br />
2.	Core : 엔진 전반에서 공통으로 사용하는 기초 시스템<br />
	- 좌표, 행렬, 쿼터니언 연산 제공, 엔진 오브젝트 생성과 수명 관리<br />
<br />
3.	Engine : 씬을 이루는 실제 에디터 오브젝트 계층<br />
	- 액터와 컴포넌트 구조 관리, 월드에 액터 추가/삭제, 렌더 가능한 데이터 구성, 기즈모 및 보조 오브젝트 관리<br />
<br />
4.	Render : 렌더링 계층<br />
	- 실제 화면 렌더링, 백버퍼 관리, 리사이즈 처리, 픽킹을 위한 프록시 렌더링<br />
<br />
5. 	GUI : ImGui 연동 계층<br />
	- UI 패널 렌더링 지원<br />
<br />
6. 	Serialization : 씬 저장/로드<br />
	- 월드 데이터를 JSON 형태로 저장, 저장된 JSON을 다시 읽어 월드 복원<br />
<br />

---
**구현 기능**<br />
<br />
- 카메라 좌우, 전후방 이동 (W,A,S,D), 카메라 Uaw, Pitch 회전 (마우스 우클릭)<br />
- 3차원 공간 상에서 오브젝트(Plane, Cube, Sphere, Torus)를 배치<br />
- 마우스 클릭 시 배치된 오브젝트 선택, 선택 시 해당 오브젝트 강조<br />
- World 좌표계, 선택된 오브젝트의 Local 좌표계를 나타내는 3차원 Axis 렌더링<br />
- 선택된 오브젝트 Gizmo 컨트롤을 통해 변형 (Transform - Location, rotation, Scale) 가능.<br />
- Space Bar 키와 툴바 버튼을 통해 Transform 모드를 변경<br />
- Property 창을 통해서도 변형 가능<br />
- 생성된 오브젝트의 총 개수와 사용된 메모리 Stat 창으로 표시<br />
- 씬 저장, 로드, 초기화 기능<br />
- Property/Control/Console 패널 구현, Console에는 UE_LOG로 로그 출력<br />
- Window size 실시간 변경<br />
