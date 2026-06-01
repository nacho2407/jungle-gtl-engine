# Depth of Field: 카메라 이론과 엔진 구현

## 1. 문서 목적

이 문서는 카메라의 피사계 심도(Depth of Field, DOF)를 이해하는 데 필요한
기본 광학 이론과 현재 엔진에 구현된 실시간 DOF 렌더링 파이프라인을 함께
정리한다.

현재 구현은 영화 제작용 물리 카메라를 완전히 재현하려는 시스템이 아니다.
게임 화면에서 중요한 시각적 특성을 안정적으로 제어하기 위한 실시간
근사 모델이다.

- 초점 거리 조절
- 조리개를 열었을 때 얕아지는 심도
- 전경과 배경 블러 분리
- 다각형 조리개 모양
- HDR 하이라이트 보케
- 깊이 경계에서 발생하는 halo 완화

---

## 2. 카메라 DOF 기본 이론

### 2.1 렌즈, 센서, 초점면

카메라는 렌즈로 들어온 빛을 이미지 센서에 투영한다. 카메라가 특정 거리에
초점을 맞추면, 그 거리의 물체에서 나온 빛은 센서 위에서 한 점으로 모인다.
이 위치를 **초점면(focus plane)**이라고 한다.

초점면보다 앞이나 뒤에 있는 물체의 빛은 센서 위에서 정확히 한 점으로
수렴하지 않는다. 대신 작은 원 또는 조리개 모양의 영역으로 퍼진다. 이
영역을 **착란원(Circle of Confusion, CoC)**이라고 한다.

```text
카메라 가까운 물체        초점면             멀리 있는 물체
        |                   |                      |
        v                   v                      v

   흐린 전경            선명한 영역             흐린 배경
   Near Blur            In Focus               Far Blur
    CoC < 0              CoC = 0                 CoC > 0
```

현재 엔진은 CoC의 부호를 사용하여 초점면 앞뒤를 구분한다.

- 음수 CoC: 카메라와 초점면 사이의 전경
- 양수 CoC: 초점면보다 뒤에 있는 배경
- 0에 가까운 CoC: 초점이 맞는 영역

### 2.2 Focal Length: 초점 거리

**Focal Length**는 렌즈의 광학적 특성으로, 일반적으로 밀리미터(mm) 단위로
표현한다. 렌즈가 무한대에 초점을 맞췄을 때 렌즈의 광학 중심부터 센서까지의
거리에 가깝다.

Focal Length는 다음 항목에 영향을 준다.

- Field of View(FOV)
- 화면 안에서 피사체가 보이는 크기
- 카메라 위치를 함께 조정했을 때의 원근감
- 물리적인 착란원 크기와 심도

현재 엔진의 DOF 셰이더는 Focal Length를 입력받지 않는다. 카메라 투영과
DOF 효과를 의도적으로 분리한 실시간 근사 모델을 사용한다.

### 2.3 Focus Distance: 피사체 초점 거리

**Focus Distance**는 카메라 렌즈 위치부터 선명하게 보이길 원하는 피사체
평면까지의 거리다. 이미지 센서부터 피사체까지의 거리를 의미하지 않는다.

현재 엔진 UI의 `Focal Distance`는 실질적으로 이 Focus Distance를
의미한다. 셰이더에서는 `DOFFocalDistance`로 전달된다.

### 2.4 Aperture Diameter와 F-Stop

조리개는 렌즈를 통과하는 빛의 양을 조절하는 구멍이다. 실제 조리개 구멍의
지름을 `D`, 렌즈의 Focal Length를 `f`라고 하면 F-Stop 또는 F-number는
다음과 같다.

```text
N = f / D
```

- `N`: F-Stop
- `f`: Focal Length
- `D`: 조리개 구멍의 지름

F-Stop 값이 작을수록 조리개가 더 크게 열린다.

```text
f/1.4 : 조리개가 크게 열림 -> 얕은 심도 -> 강한 블러
f/4.0 : 중간 정도의 심도
f/16  : 조리개가 작게 열림 -> 깊은 심도 -> 약한 블러
```

현재 엔진 UI의 `Aperture (F-Stop)`은 이 방향성을 따른다. 값을 낮추면
블러가 강해지고 심도가 얕아진다. 다만 현재 카메라 모델에는 Focal Length가
없으므로 실제 조리개 지름 `D`를 계산하지는 않는다.

### 2.5 착란원 이론

초점이 맞지 않는 점광원은 센서에서 한 점이 아니라 일정한 면적으로 퍼진다.
이 퍼진 영역이 착란원이다.

- 완전히 둥근 조리개: 원형 CoC
- 6개의 날을 가진 조리개: 육각형에 가까운 CoC
- 날 수가 많고 둥근 조리개: 원형에 가까운 CoC

얇은 렌즈 모델을 단순화한 물리 기반 CoC 지름 식의 한 예는 다음과 같다.

```text
CoC = abs(z - s) / z * (f * f / (N * (s - f)))
```

- `z`: 렌더링할 픽셀의 깊이
- `s`: Focus Distance
- `f`: Focal Length
- `N`: F-Stop

부호를 표현하는 방식과 반지름 또는 지름 중 어느 값을 사용하는지에 따라
식의 형태는 조금씩 달라질 수 있다. 중요한 경향은 같다.

- 초점면에서 멀어질수록 CoC가 커진다.
- F-Stop이 작을수록 CoC가 커진다.
- 일반적으로 Focal Length가 길수록 CoC가 커진다.
- 초점면에 가까운 물체는 CoC가 작고 선명하게 보인다.

### 2.6 현재 엔진의 CoC 근사식

현재 엔진은 물리 렌즈 공식을 그대로 사용하지 않고 게임용 근사식을
사용한다.

```hlsl
float SignedDistance = ViewDepth - FocusDistance;
float FocusRange = FocusDistance * max(DOFAperture, 0.01f);
float Radius =
    saturate(abs(SignedDistance) / max(FocusRange, 0.001f))
    * DOFMaxCoCRadius;
return (SignedDistance < 0.0f) ? -Radius : Radius;
```

실질적인 의미는 다음과 같다.

```hlsl
float blurAmount = saturate(abs(pixelDepth - focusDepth) / focusRange);
```

`Aperture (F-Stop)`은 `focusRange`를 조절한다. F-Stop을 낮추면
`focusRange`가 좁아져 초점면에서 조금만 벗어나도 CoC가 빠르게 커진다.

이 식은 실제 렌즈를 정확하게 복원하는 물리식은 아니다. 현재 엔진 범위에서
예측 가능하고 아티스트가 조절하기 쉬운 근사식이다.

---

## 3. 심도의 깊이에 영향을 주는 요소

### 3.1 Aperture / F-Stop

조리개를 열어 F-Stop을 낮추면 심도가 얕아진다. 초점면 근처의 좁은
영역만 선명하고 나머지는 빠르게 흐려진다.

조리개를 닫아 F-Stop을 높이면 심도가 깊어진다. 더 넓은 범위가 선명하게
보인다.

현재 엔진에 구현된 핵심 DOF 조절 요소다.

### 3.2 Focus Distance

가까운 물체에 초점을 맞추면 일반적으로 선명한 영역이 좁아진다. 먼 물체에
초점을 맞추면 선명한 영역이 넓어지는 경향이 있다.

현재 근사식에서도 `FocusRange`가 `FocusDistance`에 비례하므로 이 특성이
반영된다.

### 3.3 Focal Length

같은 카메라 위치와 Focus Distance를 기준으로 긴 렌즈는 일반적으로 더
얕은 심도를 만든다. 다만 Focal Length는 FOV도 함께 바꾸므로, 실제 촬영에서
같은 구도를 유지하려면 카메라 위치까지 함께 조절해야 한다.

현재 엔진 DOF 셰이더에는 포함하지 않았다.

### 3.4 Sensor Size

센서 크기는 같은 Focal Length에서 FOV에 영향을 준다. 동일한 화면 구도를
유지한다는 조건에서는 큰 센서를 사용하는 카메라가 얕은 심도를 만들기
쉬운 편이다.

현재 엔진은 물리적인 Sensor Width를 모델링하지 않는다.

### 3.5 피사체 거리와 배경 분리 거리

물체가 초점면에서 멀어질수록 CoC가 커진다. 따라서 캐릭터 바로 뒤의 벽보다
캐릭터에서 멀리 떨어진 배경이 더 흐리게 보인다.

### 3.6 Shutter Speed와 ISO

Shutter Speed와 ISO는 CoC의 기하학적 크기를 직접 바꾸지 않는다.

- Shutter Speed: 노출 시간과 Motion Blur에 영향
- ISO: 노출 증폭과 노이즈 특성에 영향
- Aperture: 노출과 DOF에 모두 영향

현재 엔진에서는 Exposure와 DOF를 독립적으로 조절한다. F-Stop, Shutter
Speed, ISO를 사진 촬영의 노출 모델로 연결하지는 않았다.

---

## 4. 블러 모양과 보케

### 4.1 초점이 맞지 않은 빛이 조리개 모양으로 보이는 이유

초점이 맞지 않은 점광원은 렌즈의 조리개 모양을 따라 퍼진다. 조리개가
원형이면 흐린 점광원도 원에 가깝게 보인다. 조리개가 6각형이면 밝은
점광원은 육각형 보케로 보일 수 있다.

일반적인 DOF 블러와 눈에 띄는 보케는 별개의 광학 현상이 아니다. 둘 다
착란원 확산에서 발생한다. 보케는 밝은 HDR 하이라이트와 큰 CoC 때문에
조리개 모양이 특히 선명하게 드러난 경우다.

### 4.2 실시간 렌더러에서 보케를 별도로 처리하는 이유

Gather 방식의 블러는 주변 픽셀을 샘플링하여 흐린 이미지를 만들 수 있다.
하지만 제한된 샘플 수로 처리하면 작고 밝은 HDR 하이라이트가 평균화되어
희미해지기 쉽다. 결과적으로 부드러운 블러는 만들 수 있지만 선명한 보케
모양은 잘 살아나지 않는다.

현재 엔진은 두 효과를 분리한다.

- 일반 DOF 블러: Gather 기반 필터
- 밝은 하이라이트 보케: Additive Sprite Scatter

### 4.3 HDR이 필요한 이유

Tone Mapping 이전의 HDR SceneColor에는 화면 흰색보다 밝은 값이 존재할 수
있다. 예를 들어 선형 공간에서 `1.0`보다 큰 하이라이트를 보존하면, 보케
합성 후에도 밝은 광원처럼 보이는 결과를 만들 수 있다.

현재 DOF 버퍼는 `DXGI_FORMAT_R16G16B16A16_FLOAT` 형식을 사용한다.
다운샘플, 블러, 재합성 과정에서도 HDR 색상을 유지할 수 있다.

---

## 5. 렌더링 파이프라인에서의 위치

DOF는 `EarlyPostProcess`에서 Opaque 지오메트리와 Height Fog 이후,
`AlphaBlend` 이전에 실행된다.

```text
Opaque
  -> EarlyPostProcess
       -> Height Fog
       -> DOFDownSampling
       -> DOFCoCPrefilter
       -> DOFBlurFar
       -> DOFBlurNear
       -> DOFBokeh
       -> DOFRecombine
  -> AlphaBlend
  -> SelectionMask
  -> EditorLines
  -> PostProcess
  -> FXAA
  -> Gizmo / Overlay / UI
  -> GammaCorrection
```

이 순서는 의도적인 선택이다. 일반적인 반투명 물체는 Depth Write를 하지
않는다. 추가 처리를 하지 않은 채 화면 전체 DOF에 포함하면 잘못된 깊이로
블러될 수 있다.

현재 엔진은 반투명 물체를 DOF 이후에 렌더링하므로 반투명 물체는 선명하게
남는다. 고급 엔진은 별도의 Translucent DOF, Responsive Translucency 또는
머티리얼 단위 옵션을 제공하기도 한다.

---

## 6. DOF 셰이더 파이프라인

### 6.1 공통 유틸: `Common/DepthOfField.hlsli`

모든 DOF 셰이더가 공유하는 상수와 함수를 모아둔 파일이다.

주요 함수:

- `LinearizeSceneDepth()`: 하드웨어 Depth를 View Space Depth로 변환한다.
- `CalculateSignedCoC()`: Near/Far 부호를 포함한 CoC 반경을 계산한다.
- `InterleavedGradientNoise()`: 픽셀마다 샘플 순서를 다르게 만든다.
- `PolygonBoundaryRadius()`: 다각형 조리개 경계를 계산한다.
- `MapDiskSampleToPolygonAperture()`: 원판 샘플을 조리개 모양으로 변형한다.
- `DominantSignedCoC()`: 더 강한 Near 또는 Far CoC를 선택한다.
- `BokehHighlightRatio()`: 임계값 이상의 HDR 하이라이트를 추출한다.

이 파일에는 32개의 Poisson Disk 샘플 패턴도 포함되어 있다.

### 6.2 1단계: `DOFDownSampling.hlsl`

```text
입력: Full-resolution SceneColor + SceneDepth
출력: Half-resolution RGB Color + Alpha Signed CoC
```

2x2 영역을 읽어 half-resolution 작업 버퍼를 만든다. 단순 평균을 사용하지
않고 각 픽셀의 CoC와 밝기를 계산하여 가중 평균한다.

도입한 품질 개선:

- **CoC-aware Downsample**: 블러가 강한 픽셀에 더 높은 가중치를 준다.
- **Highlight-aware Downsample**: 밝은 픽셀을 추가로 보존하여 작은 보케
  후보가 축소 과정에서 사라지지 않게 한다.
- **Dominant Signed CoC**: 서로 호환되지 않는 Near/Far 깊이를 평균내지
  않고 더 강한 CoC를 보존한다.

이 처리가 없으면 얇은 전경 실루엣과 작은 하이라이트가 half-resolution
축소 과정에서 쉽게 사라진다.

### 6.3 2단계: `DOFCoCPrefilter.hlsl`

```text
입력: Half-resolution Color + Signed CoC
출력: 안정화된 Half-resolution Color + Signed CoC
```

주변 3x3 픽셀을 검사하여 CoC와 색상을 정돈한다. Near와 Far 정보를 따로
누적한다.

도입한 품질 개선:

- Near/Far CoC를 분리하여 누적
- 중심, 십자 방향, 대각선에 서로 다른 Spatial Weight 적용
- Near CoC가 충분히 강하면 전경을 조금 더 우선
- 선택된 CoC 방향에 따라 색상을 부분적으로 필터링

이 단계는 실루엣 주변의 불안정한 경계를 완화하고 두 Blur Pass에 더
신뢰할 수 있는 입력을 제공한다.

### 6.4 3단계: `DOFBlurFar.hlsl`

```text
입력: Prefilter 결과
출력: Far Blur Texture
대상: CoC > 0
```

초점면보다 뒤에 있는 배경을 Gather 방식으로 흐리게 만든다.

도입한 품질 개선:

- 눈에 띄는 동심원 Ring 대신 32개의 Poisson Disk 샘플 사용
- 픽셀별 Interleaved Gradient Noise로 샘플 순서 변경
- CoC 크기에 따른 Adaptive Sample Count 적용

```text
작은 CoC        : 10 samples
CoC > 2 pixels  : 16 samples
CoC > 6 pixels  : 24 samples
CoC > 10 pixels : 32 samples
```

- 작은 블러는 원판에 가깝게 유지
- 큰 블러는 선택한 다각형 조리개 모양이 더 드러나도록 변형
- Neighbor CoC에 따라 유효한 Far 샘플의 가중치 조절

작은 블러의 비용은 낮게 유지하고, 큰 블러에는 더 많은 샘플과 조리개
형태를 적용한다.

### 6.5 4단계: `DOFBlurNear.hlsl`

```text
입력: Prefilter 결과
출력: Near Blur RGB + Coverage Alpha
대상: CoC < 0
```

Near Blur는 Far Blur와 다르게 처리해야 한다. 배경 픽셀 자체에는 Near CoC가
없어도 주변의 흐린 전경 물체가 해당 위치까지 번져 들어올 수 있기 때문이다.

도입한 품질 개선:

- 32개의 Poisson / Polygon 샘플 사용
- 주변 전경 픽셀의 CoC가 현재 픽셀까지 도달하는지 `reachWeight`로 검사
- 평균 Coverage와 최대 Coverage를 함께 누적
- 최종 Coverage Mask에 `smoothstep()` 적용

Alpha 채널은 전경 블러가 현재 픽셀을 얼마나 덮는지 나타낸다. 이를 통해
캐릭터 외곽이 딱딱하게 끊기거나 배경색이 부자연스럽게 새는 현상을 줄인다.

### 6.6 5단계: `DOFBokeh.hlsl`

```text
입력: Prefilter 결과
출력: Half-resolution Additive Bokeh Texture
방식: Sprite Scatter
```

조건을 만족하는 half-resolution 픽셀마다 Quad Sprite를 생성한다. Pixel
Shader는 Quad를 선택한 다각형 조리개 모양으로 잘라낸다.

도입한 품질 개선:

- `Bokeh Threshold`: 충분히 밝은 HDR 픽셀만 Sprite 생성
- `Bokeh Intensity`: 보케 밝기 조절
- `Bokeh Radius Scale`: CoC 대비 보케 크기 조절
- `Aperture Blade Count`: 3각형부터 16각형까지 조리개 모양 지원
- Additive Blend: 밝은 하이라이트 에너지 보존

PNG 보케 텍스처를 사용하는 방식이 아니다. 조리개 실루엣을 셰이더에서
수학적으로 계산한다.

### 6.7 6단계: `DOFRecombine.hlsl`

```text
입력:
  - Full-resolution Sharp SceneColor
  - Full-resolution SceneDepth
  - Half-resolution Far Blur
  - Half-resolution Near Blur
  - Half-resolution Bokeh

출력:
  - Full-resolution HDR SceneColor
```

Sharp 원본, Far Blur, Near Blur, Bokeh를 최종 합성한다.

도입한 품질 개선:

- **Bilateral Upsample**: half-resolution 결과를 단순 확대하지 않고
  주변 3x3 영역의 CoC와 깊이 방향을 비교한다.
- Far 샘플이 잘못된 깊이 방향에 속하면 가중치를 크게 낮춘다.
- Near Blur는 Coverage Alpha를 사용하여 전경 블러를 원본 위에 확산한다.
- Bokeh는 일반 블러 합성 후 Additive 방식으로 더한다.

이 단계는 깊이 경계 주변의 halo를 줄이는 역할을 한다.

---

## 7. 적용된 품질 개선 기법 요약

현재 파이프라인은 단일 Fullscreen Disk Blur보다 발전된 구조다.

| 기법 | 목적 |
| --- | --- |
| Half-resolution DOF 버퍼 | 넓은 블러 커널의 비용 절감 |
| HDR `R16G16B16A16_FLOAT` 버퍼 | 밝은 하이라이트 에너지 보존 |
| Signed CoC | 전경과 배경 블러 구분 |
| CoC-aware Downsample | 축소 과정에서 강한 블러 영역 보존 |
| Highlight-aware Downsample | 작은 보케 후보 보존 |
| Dominant CoC 선택 | 서로 다른 깊이를 단순 평균하지 않도록 처리 |
| CoC Prefilter | Gather 이전 경계 안정화 |
| Near / Far Pass 분리 | 전경과 배경의 색상 번짐 감소 |
| Poisson Disk Samples | 반복적인 Ring 패턴 완화 |
| Interleaved Gradient Noise | 픽셀별 반복 샘플링 무늬 완화 |
| Polygon Aperture Mapping | 조절 가능한 다각형 조리개 블러 |
| Adaptive Far Sample Count | 큰 블러에 더 많은 연산 투자 |
| Near Coverage Propagation | 전경 블러가 배경 위로 자연스럽게 확산 |
| Bilateral Upsample | half-res 합성 과정의 halo 감소 |
| Sprite Scatter Bokeh | 밝은 HDR 하이라이트의 조리개 모양 보존 |

---

## 8. 에디터 조절 항목

| 항목 | 의미 |
| --- | --- |
| `DepthOfField` | 전체 DOF 파이프라인 활성화 |
| `Aperture (F-Stop)` | 낮출수록 강한 블러와 얕은 심도 |
| `Focal Distance` | 카메라부터 선명한 피사체 평면까지의 거리 |
| `Max CoC Radius` | 픽셀 단위 최대 블러 반경 |
| `Aperture Blade Count` | 3각형부터 16각형까지의 조리개 날 수 |
| `Bokeh Enable` | 별도의 Sprite Scatter Bokeh Pass 활성화 |
| `Bokeh Threshold` | 보케로 사용할 최소 HDR 하이라이트 값 |
| `Bokeh Intensity` | 보케 Sprite 밝기 배율 |
| `Bokeh Radius Scale` | 보케 Sprite 반경 배율 |

---

## 9. 현재 구현의 한계

현재 구현은 실시간 게임 엔진에 적합하지만 완전한 시네마틱 DOF 시스템과
동일하지는 않다.

- 반투명 물체는 DOF 이후 렌더링되므로 선명하게 남는다.
- CoC 계산식은 물리적으로 정확한 Thin Lens Formula가 아니라 근사식이다.
- Focal Length와 Sensor Width를 카메라 모델에 포함하지 않는다.
- F-Stop을 Exposure와 연결하지 않는다.
- Shutter Speed와 ISO를 사진 촬영 파라미터로 모델링하지 않는다.
- Temporal Accumulation 또는 Temporal Stabilization이 없다.
- Tile Classification 기반 Adaptive Workload 최적화가 없다.
- 매우 큰 CoC를 위한 Multi-resolution Blur Hierarchy가 없다.
- Sprite Scatter에 Tile별 Bokeh Budget 제한이 없다.
- 복잡하고 얇은 실루엣의 Occlusion 처리는 여전히 근사적이다.

---

## 10. 향후 개선 권장 사항

### 10.1 Temporal Stabilization

Motion Vector를 사용하여 프레임 사이의 DOF 결과를 안정화한다. 카메라가
움직일 때 발생하는 보케 깜빡임을 줄이는 데 효과적이다.

### 10.2 Tile Classification

Tile마다 최대 Near CoC, Far CoC, Highlight Energy를 계산한다. 선명한
영역의 불필요한 작업을 건너뛰고 큰 블러가 필요한 Tile에 연산을 집중한다.

### 10.3 Multi-resolution Blur Hierarchy

매우 큰 CoC는 quarter-resolution 또는 Mip Chain에서 처리한다. 적은
샘플 수로 더 넓은 블러를 만들 수 있다.

### 10.4 향상된 Occlusion-aware Composition

Downsample과 Prefilter 단계에서 Near/Far Color Layer를 더 명시적으로
보존한다. 얇은 전경 물체 주변으로 배경색이 새는 현상을 더 줄일 수 있다.

### 10.5 Compute 기반 Scatter

보케 수가 많아지면 Sprite Draw Scatter를 Compute Pipeline으로 교체하거나
보완한다. Tile Binning, Budget Control, 안정적인 연산량 관리에 유리하다.

### 10.6 선택적 Physical Camera Mode

향후 시네마틱 카메라가 필요해지면 Focal Length, Sensor Width, Shutter
Speed, ISO를 포함한 모드를 추가한다. 현재의 단순하고 조절하기 쉬운
DOF 모드는 기본값으로 유지할 수 있다.

---

## 11. 결론

현재 엔진의 DOF는 다음과 같은 다단계 HDR Post Process 파이프라인이다.

```text
DOFDownSampling
  -> DOFCoCPrefilter
  -> DOFBlurFar
  -> DOFBlurNear
  -> DOFBokeh
  -> DOFRecombine
```

Signed CoC, Near/Far 분리, CoC-aware 필터링, Poisson / Polygon Gather,
Bilateral Upsample, Sprite Scatter Bokeh를 결합했다.

엄격한 물리 카메라 시뮬레이션보다 조절하기 쉬운 구조를 유지하면서도,
게임 화면에서 알아보기 쉬운 DOF의 핵심 시각적 특성과 경계 품질을
개선하는 데 초점을 맞춘 구현이다.

