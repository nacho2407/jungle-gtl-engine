# 렌더러 구조 안내서

## 1. 이 문서는 무엇을 설명하나

이 문서는 현재 코드베이스의 렌더러 구조를 처음 보는 사람 기준으로 설명하기 위한 문서입니다.

예전 구조에서는 `FRenderer` 하나가 너무 많은 일을 직접 처리했습니다.
지금 구조에서는 프레임 진입점, 씬 수집, 씬 렌더링, 뷰포트 합성, 화면 UI 렌더링, 특수 기능이 서로 다른 클래스로 나뉘어 있습니다.

이 문서는 다음 질문에 순서대로 답하도록 구성되어 있습니다.

1. 한 프레임은 어디서 시작되는가
2. 월드는 어디서 씬 데이터로 바뀌는가
3. 씬 데이터는 어디서 GPU 드로우 커맨드로 바뀌는가
4. 여러 뷰포트 결과는 어디서 백버퍼에 합성되는가
5. 화면 UI는 어디서 기록되고 어디서 그려지는가
6. 이번 리팩토링에서 책임은 어떻게 이동했는가

## 2. 먼저 한 줄로 요약하면

현재 구조의 핵심은 이 문장 하나입니다.

`FRenderer`는 프레임 오케스트레이터이고, 실제 작업은 각 전용 서브시스템이 맡는다.

조금 더 풀면 이렇게 이해하면 됩니다.

1. `FScenePacketBuilder`는 무엇을 그릴지 모은다.
2. `FSceneRenderer`는 그것을 어떻게 그릴지 결정하고 실행한다.
3. `FViewportCompositor`는 렌더 결과를 어디에 놓을지 결정한다.
4. `FScreenUIRenderer`는 마지막 화면 UI를 그린다.
5. 텍스트, SubUV, 아웃라인, 디버그 라인은 전용 feature가 처리한다.

## 3. 왜 이런 리팩토링을 했나

예전 구조의 가장 큰 문제는 `FRenderer`가 너무 많은 책임을 한 번에 들고 있었다는 점입니다.

예전 `FRenderer`는 아래 성격의 일을 한 클래스 안에서 같이 처리했습니다.

1. D3D11 디바이스와 스왑체인 관리
2. 프레임 시작과 종료
3. 씬 패스 실행
4. 렌더 커맨드 제출과 실행
5. UI 관련 callback host 역할
6. selection outline 후처리
7. 디버그 라인 버퍼 관리
8. 텍스트/SubUV 특수 렌더링
9. 뷰포트 합성

이 구조는 세 가지 문제를 만들었습니다.

1. 씬 수집 계층이 렌더러 구현을 알아야 했다.
2. 에디터가 렌더러 내부 패스 순서를 직접 제어했다.
3. 새 기능을 추가할수록 `FRenderer`가 계속 비대해졌다.

이번 리팩토링의 목적은 보기 좋은 분리 자체가 아니라, 책임 경계를 다시 세워서 구조가 다시 무너지지 않게 만드는 것입니다.

## 4. 가장 중요한 설계 원칙

이 구조를 이해할 때는 아래 규칙만 기억해도 절반은 끝납니다.

1. 월드 수집 계층은 D3D11 타입을 몰라야 한다.
2. `ViewportClient`는 씬 패킷을 만들 수는 있지만 렌더 실행 세부는 몰라야 한다.
3. `FRenderer`는 프레임을 시작하고 각 서브시스템을 호출하지만, 세부 기능을 직접 구현하지 않는다.
4. UI는 먼저 기록하고 나중에 렌더링한다.
5. 에디터는 callback을 주입하지 않고 데이터 요청 구조를 만들어 전달한다.

## 5. 큰 그림에서 어떤 객체들이 있나

### 5-1. 프레임 진입점

파일: `Engine/Source/Renderer/Renderer.h`

`FRenderer`는 렌더러의 얼굴입니다.

이 클래스가 하는 일은 아래와 같습니다.

1. 프레임 시작과 종료를 관리한다.
2. 게임 프레임 요청과 에디터 프레임 요청을 받는다.
3. 내부 서브시스템을 적절한 순서로 호출한다.
4. 공용 디바이스/컨텍스트 접근 지점을 제공한다.

중요한 점은, `FRenderer`가 더 이상 씬 수집기나 UI callback host가 아니라는 점입니다.

### 5-2. 디바이스와 스왑체인

파일: `Engine/Source/Renderer/RenderDevice.h`

`FRenderDevice`는 D3D11 디바이스 관련 책임만 담당합니다.

이 클래스의 책임은 아래와 같습니다.

1. 디바이스/컨텍스트/스왑체인 생성
2. 백버퍼 RTV/DSV 생성
3. 리사이즈 처리
4. Present
5. 기본 백버퍼 바인딩

즉, 화면에 출력하는 기반 자원은 여기 있습니다.

### 5-3. 씬 렌더러

파일: `Engine/Source/Renderer/SceneRenderer.h`

`FSceneRenderer`는 현재 씬 렌더 경로의 핵심 실행기입니다.

이 클래스의 책임은 아래와 같습니다.

1. `FSceneRenderPacket`을 버킷 기반 `FRenderCommandQueue`로 변환
2. Default / Overlay / Transparent 버킷별 정렬 정책 적용
3. 씬 큐와 추가 큐를 섞지 않고 순차 실행
4. 와이어프레임 override 적용
5. 버킷별 패스 실행
6. 예전 `FRenderer` 내부에 있던 마지막 씬 실행 seam 관리

즉, `SubmitCommands`, `ExecuteCommands`, `ExecuteRenderPass`는 이제 `FRenderer`가 아니라 `FSceneRenderer`의 책임입니다.

### 5-4. 뷰포트 합성기

파일: `Engine/Source/Renderer/ViewportCompositor.h`

`FViewportCompositor`는 에디터 멀티 뷰포트 결과를 백버퍼에 배치하는 역할을 맡습니다.

이 클래스의 책임은 아래와 같습니다.

1. 뷰포트 장면 텍스처를 백버퍼 특정 영역에 배치
2. 합성용 셰이더와 상태 관리
3. 에디터 화면에서 최종 viewport layout 반영

예전 `BlitRenderer`보다 더 큰 의미의 정식 프레임 단계라고 이해하면 됩니다.

### 5-5. 화면 UI 렌더러

파일: `Engine/Source/Renderer/ScreenUIRenderer.h`

`FScreenUIRenderer`는 `FUIDrawList`를 실제 GPU 작업으로 바꾸는 렌더러입니다.

이 클래스의 책임은 아래와 같습니다.

1. UI draw list를 임시 메시로 변환
2. 텍스트 색상별 폰트 머티리얼 관리
3. 직교 투영 적용
4. 화면 UI를 백버퍼 위에 렌더링

즉 UI는 더 이상 씬 커맨드 큐에 섞여서 그려지지 않습니다.

## 6. 씬 프런트엔드는 어떻게 바뀌었나

### 6-1. 씬 패킷

파일: `Engine/Source/Level/SceneRenderPacket.h`

`FSceneRenderPacket`은 렌더러 비의존적인 씬 설명 데이터입니다.

이 구조는 “무엇이 보이는가”를 담고, “어떻게 그릴 것인가”는 담지 않습니다.

현재는 크게 세 종류의 프리미티브 버킷을 가집니다.

1. 메시 프리미티브
2. 텍스트 프리미티브
3. SubUV 프리미티브

중요한 점은 여기에는 `ID3D11*` 같은 타입이 없다는 점입니다.

### 6-2. 씬 패킷 빌더

파일: `Engine/Source/Level/ScenePacketBuilder.h`

예전의 `RenderCollector`는 이름은 수집기였지만 실제로는 렌더러 구현을 너무 많이 알고 있었습니다.

지금의 `FScenePacketBuilder`는 아래 일만 합니다.

1. 액터와 컴포넌트를 순회
2. ShowFlag 확인
3. 프러스텀 컬링
4. 프리미티브 분류
5. `FSceneRenderPacket` 채우기

즉, “보이는 것을 분류해서 담는 일”까지만 하고 멈춥니다.

## 7. 씬 패킷은 어디서 실제 드로우 커맨드가 되나

파일: `Engine/Source/Renderer/SceneCommandBuilder.h`

`FSceneCommandBuilder`는 패킷 안의 프리미티브를 실제 렌더 커맨드로 바꾸는 단계입니다.

이 단계에서 일어나는 일은 아래와 같습니다.

1. 메시 프리미티브를 실제 draw command로 확장
2. 텍스트 프리미티브를 text feature를 통해 메시로 변환
3. SubUV 프리미티브를 SubUV feature를 통해 메시로 변환
4. 레이어, 머티리얼, 정렬 키에 필요한 데이터를 채움

즉 씬 수집과 GPU 실행 사이의 중간 계층이라고 보면 됩니다.

### 7-1. 현재 씬 큐는 하나가 아니라 버킷 구조다

파일: `Engine/Source/Renderer/RenderCommand.h`

예전처럼 씬 커맨드를 한 배열에 모두 모아 한 번에 정렬하는 구조는 줄였습니다.

현재 `FRenderCommandQueue`는 아래 버킷으로 나뉩니다.

1. `DefaultCommands`
2. `OverlayCommands`
3. `TransparentCommands`

이렇게 나눈 이유는 아래와 같습니다.

1. 일반 메시와 기즈모/그리드를 같은 정렬 정책으로 처리할 필요가 없다.
2. 오버레이는 제출 순서를 유지하는 편이 자연스러운 경우가 많다.
3. 투명체는 이후 별도 정렬 정책을 넣을 자리가 필요하다.

즉, 지금 구조의 핵심은 “씬 큐 1개 타입”이 아니라 “씬 커맨드 버킷을 가진 큐 구조”입니다.

## 8. 특수 기능은 어디로 갔나

현재 특수 기능은 `Renderer/Feature` 아래 전용 클래스로 분리되어 있습니다.

### 8-1. `FTextRenderFeature`

파일: `Engine/Source/Renderer/Feature/TextRenderFeature.h`

책임:

1. 폰트 아틀라스 준비
2. 기본 텍스트 머티리얼 접근
3. 문자열을 글리프 메시로 변환

### 8-2. `FSubUVRenderFeature`

파일: `Engine/Source/Renderer/Feature/SubUVRenderFeature.h`

책임:

1. SubUV 머티리얼과 텍스처 초기화
2. billboard quad 메시 생성
3. SubUV 스프라이트 렌더 지원

### 8-3. `FOutlineRenderFeature`

파일: `Engine/Source/Renderer/Feature/OutlineRenderFeature.h`

책임:

1. outline mask 생성
2. 후처리 합성
3. outline 전용 셰이더와 리소스 관리

selection outline은 더 이상 callback으로 주입되지 않고, 요청 데이터로 전달됩니다.

### 8-4. `FDebugLineRenderFeature`

파일: `Engine/Source/Renderer/Feature/DebugLineRenderFeature.h`

책임:

1. 디버그 라인 요청 축적
2. 선분/AABB를 실제 라인 데이터로 확장
3. 임시 버텍스 버퍼 업로드
4. 디버그 라인 렌더링

즉 예전 `FRenderer::DrawLine`, `DrawCube`, `ExecuteLineCommands` 역할이 여기로 이동했습니다.

## 9. UI는 어떻게 바뀌었나

### 9-1. UI는 먼저 기록한다

파일: `Editor/Source/Slate/Widget/Painter.h`

예전 `FPainter`는 renderer를 직접 들고 있으면서 즉시 렌더링에 가까운 흐름을 가지고 있었습니다.

현재 `FSlatePaintContext`는 recorder입니다.

이 객체가 하는 일은 아래와 같습니다.

1. 채워진 사각형 기록
2. 외곽선 기록
3. 텍스트 기록
4. 클립 정보 기록
5. 순서 정보 기록

즉 “그린다”가 아니라 “그릴 내용을 적는다”가 핵심입니다.

### 9-2. Slate는 draw list만 만든다

파일: `Editor/Source/Slate/SlateApplication.h`

`FSlateApplication`은 현재 아래 역할에 집중합니다.

1. 위젯 트리 구성
2. 레이아웃 계산
3. 위젯 paint 순서 계산
4. `FSlatePaintContext`에 draw list 기록

이 계층은 렌더러 포인터를 몰라도 됩니다.

### 9-3. 실제 GPU UI 드로우는 별도 렌더러가 한다

파일: `Engine/Source/Renderer/ScreenUIRenderer.h`

기록된 `FUIDrawList`는 마지막에 `FScreenUIRenderer`가 GPU 드로우로 바꿉니다.

이 분리가 중요한 이유는 아래와 같습니다.

1. UI 계층이 렌더러 내부 상태를 몰라도 된다.
2. UI 기록과 GPU 실행 시점을 분리할 수 있다.
3. 화면 UI를 씬 커맨드 큐와 분리할 수 있다.

## 10. 게임 프레임은 실제로 어떤 순서로 흐르나

가장 단순한 진입 경로는 게임 프레임입니다.

### 10-1. 흐름 요약

1. `FEngine::RenderFrame()`가 렌더 구간을 시작한다.
2. 활성 `ViewportClient`가 현재 월드 기준으로 씬 패킷을 준비한다.
3. `IViewportClient::BuildSceneRenderPacket()`가 `FScenePacketBuilder`를 호출한다.
4. `ViewportClient`가 `FGameFrameRequest`를 채운다.
5. `FRenderer::RenderGameFrame()`이 요청을 받는다.
6. `FSceneRenderer::RenderPacketToTarget()`이 패킷을 씬 커맨드로 바꾼다.
7. `FSceneRenderer`가 Default / Transparent / Overlay 버킷을 필요한 방식으로만 실행한다.
8. 필요하면 `FDebugLineRenderFeature`가 디버그 라인을 그린다.
9. `FRenderDevice`가 Present한다.

### 10-2. 흐름 도식

```text
World
 -> ViewportClient
 -> ScenePacketBuilder
 -> FSceneRenderPacket
 -> FGameFrameRequest
 -> FRenderer
 -> FSceneRenderer
 -> GPU Scene Pass
 -> DebugLineFeature
 -> Present
```

## 11. 에디터 프레임은 어떤 점이 다르나

에디터 프레임은 게임 프레임보다 단계가 더 많습니다.
여러 뷰포트, outline, debug overlay, viewport 합성, 화면 UI가 모두 들어가기 때문입니다.

### 11-1. 흐름 요약

1. `FEditorEngine::RenderFrame()`이 프레임을 시작한다.
2. `FEditorViewportRenderService::RenderAll()`이 에디터 프레임에 필요한 데이터를 모은다.
3. 활성 뷰포트마다 `FSceneRenderPacket`을 만든다.
4. 활성 뷰포트마다 `FOutlineRenderRequest`를 만든다.
5. 활성 뷰포트마다 `FDebugLineRenderRequest`를 만든다.
6. 뷰포트마다 `FViewportScenePassRequest`를 구성한다.
7. `FSlateApplication::BuildDrawList()`가 화면 UI draw list를 만든다.
8. 이 모든 것을 `FEditorFrameRequest`로 묶는다.
9. `FRenderer::RenderEditorFrame()`이 각 뷰포트의 씬 큐와 추가 큐를 분리된 경로로 렌더링한다.
10. 필요하면 outline/debug line feature를 실행한다.
11. `FViewportCompositor`가 뷰포트 결과를 백버퍼에 합성한다.
12. `FScreenUIRenderer`가 최종 UI를 그린다.
13. `FRenderDevice`가 Present한다.

### 11-2. 흐름 도식

```text
EditorViewportRenderService
 -> Viewport Scene Packets
 -> Outline Requests
 -> Debug Line Requests
 -> Slate Draw List
 -> FEditorFrameRequest
 -> FRenderer::RenderEditorFrame
 -> SceneRenderer per Viewport
 -> OutlineFeature
 -> DebugLineFeature
 -> ViewportCompositor
 -> ScreenUIRenderer
 -> Present
```

## 12. 이번 리팩토링에서 책임은 어떻게 이동했나

이 섹션이 이번 변경을 이해하는 핵심입니다.

### 12-1. 예전에는 `FRenderer`에 있던 책임

예전에는 아래 책임이 `FRenderer` 안에 섞여 있었습니다.

1. 디바이스/스왑체인
2. 씬 커맨드 제출
3. 씬 커맨드 실행
4. render pass 실행
5. 디버그 라인
6. outline 리소스
7. text/SubUV 기능 접근
8. GUI callback host
9. 포스트 렌더 callback
10. viewport 합성

### 12-2. 지금은 어디로 옮겨졌나

현재는 아래처럼 이동했습니다.

1. 디바이스/스왑체인 -> `FRenderDevice`
2. 씬 커맨드 제출/실행/패스 실행 -> `FSceneRenderer`
3. viewport 합성 -> `FViewportCompositor`
4. 화면 UI 렌더링 -> `FScreenUIRenderer`
5. text 메시 생성 -> `FTextRenderFeature`
6. SubUV 메시 생성 -> `FSubUVRenderFeature`
7. outline 리소스와 실행 -> `FOutlineRenderFeature`
8. 디버그 라인 버퍼와 실행 -> `FDebugLineRenderFeature`
9. UI callback 기반 연결 -> 제거
10. selection outline callback 주입 -> request 데이터 방식으로 교체

추가로 최근 변경으로 아래도 달라졌습니다.

1. 씬 커맨드 전체 정렬 -> 버킷별 정렬로 축소
2. `AdditionalCommands`와 메인 씬 큐 병합 -> 별도 큐 유지 후 순차 실행
3. 화면 UI -> 씬 레이어가 아니라 완전 별도 렌더 경로

### 12-3. `RenderCollector`는 무엇이 달라졌나

예전 `RenderCollector`는 수집기처럼 보였지만 실제로는 렌더러 구현과 특수 기능까지 너무 많이 알고 있었습니다.

지금은 아래처럼 나뉩니다.

1. `FScenePacketBuilder`는 월드에서 무엇이 보이는지만 모은다.
2. `FSceneCommandBuilder`는 그것을 실제 씬 드로우 커맨드로 확장한다.
3. feature 클래스는 텍스트/SubUV 같은 특수 확장을 담당한다.

즉 “수집”과 “렌더 명령 조립”이 분리되었습니다.

### 12-4. 에디터는 무엇이 달라졌나

예전 에디터 코드는 renderer 내부 패스 실행 순서를 직접 많이 알고 있었습니다.

현재 에디터는 아래 역할만 합니다.

1. 뷰포트별 scene request를 만든다.
2. outline/debug line request를 만든다.
3. Slate draw list를 만든다.
4. 프레임 요청을 `FRenderer`에 넘긴다.

즉, 에디터는 “실행기”가 아니라 “프레임 설명자 생성기”가 되었습니다.

## 13. 새 기능을 추가할 때 어디에 넣어야 하나

새 기능을 넣을 때 가장 먼저 해야 할 질문은 이것입니다.

“이 기능은 무엇을 그릴지 설명하는 데이터인가, 아니면 실제 렌더링 기능인가?”

### 13-1. 씬에 보일 프리미티브 종류를 추가하는 경우

예: decal, custom sprite, world marker

순서는 보통 아래와 같습니다.

1. `FSceneRenderPacket`에 새 프리미티브 버킷을 추가한다.
2. `FScenePacketBuilder`에 수집 로직을 추가한다.
3. `FSceneCommandBuilder`에 드로우 커맨드 변환 로직을 추가한다.
4. 별도 GPU 리소스가 필요하면 `Renderer/Feature` 아래 새 feature 클래스를 만든다.

### 13-2. 후처리나 디버그 기능을 추가하는 경우

예: selection highlight, debug normals, picking overlay

이 경우는 보통 씬 패킷보다 frame request 쪽이 더 맞습니다.

1. 새 request struct를 만든다.
2. 에디터나 게임 쪽에서 요청 데이터를 채운다.
3. `FRenderer`가 적절한 시점에 전용 feature를 호출하게 만든다.

중요한 점은 callback을 다시 들여오지 않는 것입니다.

## 14. 디버깅할 때는 어디부터 보면 되나

무언가 화면에 안 나오면 아래 순서로 보는 것이 가장 빠릅니다.

1. `ViewportClient`가 올바른 씬 패킷을 만들었는가
2. `FScenePacketBuilder`가 그 프리미티브를 실제로 수집했는가
3. `FSceneCommandBuilder`가 원하는 커맨드를 만들었는가
4. `FSceneRenderer`가 해당 레이어를 실행했는가
5. 에디터라면 `FViewportCompositor`가 결과를 백버퍼에 올렸는가
6. 화면 UI라면 `FSlatePaintContext`가 draw list를 기록했는가
7. `FScreenUIRenderer`가 그 draw list를 실제로 그렸는가

즉, “수집 -> 변환 -> 실행 -> 합성 -> UI” 순으로 좁혀가면 됩니다.

## 15. 파일 지도로 다시 보면

가장 자주 보는 파일만 다시 정리하면 아래와 같습니다.

1. 프레임 진입점: `Engine/Source/Renderer/Renderer.h`
2. 디바이스와 Present: `Engine/Source/Renderer/RenderDevice.h`
3. 씬 실행: `Engine/Source/Renderer/SceneRenderer.h`
4. 뷰포트 합성: `Engine/Source/Renderer/ViewportCompositor.h`
5. 화면 UI 렌더링: `Engine/Source/Renderer/ScreenUIRenderer.h`
6. 씬 수집: `Engine/Source/Level/ScenePacketBuilder.h`
7. 씬 패킷 데이터: `Engine/Source/Level/SceneRenderPacket.h`
8. UI 기록기: `Editor/Source/Slate/Widget/Painter.h`
9. UI draw list 생성: `Editor/Source/Slate/SlateApplication.h`
10. 에디터 프레임 조립: `Editor/Source/Viewport/Services/EditorViewportRenderService.h`

## 16. 마지막으로 머릿속 모델 하나만 남긴다면

이 구조는 이렇게 기억하면 됩니다.

`ScenePacketBuilder`가 무엇을 그릴지 정리하고, `SceneRenderer`가 그것을 실제로 그리며, `ViewportCompositor`가 결과를 배치하고, `ScreenUIRenderer`가 마지막 UI를 덮는다.
