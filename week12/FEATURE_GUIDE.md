# Week08 Team6 엔진 설명 문서

## 1. Shadow (그림자 시스템)

다양한 광원에 대해 실시간 그림자 효과를 제공하며, 효율적인 메모리 관리를 위해 아틀라스 방식과 다각도의 필터링 기법을 지원합니다.

### 1.1 개요
- **컴포넌트**: `ULightComponent` 및 하위 컴포넌트 (`UDirectionalLightComponent`, `UPointLightComponent` 등)
- **렌더 패스**: 
  - `ERenderPass::Shadow`: 깊이 정보 생성
  - `ERenderPass::VSMConversion`: VSM용 모먼트 생성 및 블러링 (선택적)
- **쉐이더**: 
  - `Shadow.hlsl`: 깊이 렌더링
  - `ShadowFunction.hlsl`: 그림자 샘플링 및 필터링 함수
  - `VSMShadow.hlsl`, `VSMBlurComputeShader.hlsl`: VSM 관련 처리

### 1.2 Shadow Mapping 기법
- **CSM (Cascaded Shadow Maps)**:
  - Directional Light에 사용되며, 카메라 거리에 따라 최대 4개의 캐스케이드(Cascade)로 분할하여 렌더링합니다.
  - **Practical Split**: 로그 분할과 선형 분할을 보간(Lambda)하여 원거리와 근거리의 해상도 밸런스를 조절합니다.
- **PSM (Perspective Shadow Maps)**:
  - 투영 행렬을 왜곡시켜 카메라 근처의 그림자 해상도를 극대화하는 기법입니다.
  - Slideback 기술을 적용하여 가상 카메라 위치를 조정함으로써 안정성을 확보합니다.
- **Point Light Shadow**:
  - 6개의 면을 가진 큐브맵(`TextureCubeArray`) 방식을 사용하여 전방향 그림자를 처리합니다.

### 1.3 필터링 기법
- **PCF (Percentage Closer Filtering)**:
  - 하드웨어 비교 샘플러(`SampleCmpLevelZero`)를 사용하여 인접 텍셀들을 샘플링하고 평균화함으로써 부드러운 경계선을 생성합니다.
  - `ShadowSoftness` 속성을 통해 커널 크기를 조절할 수 있습니다.
- **VSM (Variance Shadow Maps)**:
  - 깊이($d$)와 깊이의 제곱($d^2$)을 저장하여 확률적인 그림자 투과율을 계산합니다.
  - **Chebyshev's Inequality**: 통계적 상한선을 사용하여 부드러운 그림자를 생성하며, 하드웨어 필터링 및 블러링과 호환성이 높습니다.
  - 가우시안 블러(Gaussian Blur)를 적용하여 매우 부드러운 그림자 표현이 가능합니다.

### 1.4 Shadow Atlas 관리
- **2D Shadow Atlas**:
  - 8192x8192 해상도의 대형 텍스처 하나에 모든 2D 그림자 맵을 배치합니다.
  - **Binary Tree Packing**: 이진 트리 알고리즘을 사용하여 빈 공간을 효율적으로 찾아 타일을 할당합니다.
  - **Tiers**: 라이트의 중요도나 거리에 따라 고정된 해상도 티어(256, 512, 1024, 2048) 중 하나를 할당받습니다.
- **Cube Map Atlas**:
  - 최대 32개의 포인트 라이트를 지원하기 위해 `TextureCubeArray`를 별도로 관리합니다.
  - 각 면은 512x512 해상도로 고정되어 있습니다.
