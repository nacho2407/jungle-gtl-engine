# 📄 WEEK3_TEAM3 설명문서

3주차 과제에 대해 작성한 문서입니다.
해당 프로젝트의 주요 사항과 명세에 구체적으로 언급되지 않은 구현 내용을 정리합니다.

---

## ⌨️ 단축키

* `CTRL`
  → 다중 선택 (이미 선택된 오브젝트는 선택 해제)

* `CTRL + A` (Scene Manager에서 컴포넌트 클릭 후)
  → 현재 씬 전체 선택

* `Delete`
  → 선택된 오브젝트 삭제

---

## 🧩 배치 렌더링 (Batch Rendering)

* **Dynamic Vertex Buffer 기반 동적 배치 렌더링**

* 다음 요소들을 별도로 배칭 처리:

  * Grid & World Axis
  * AABB (디버깅용 라인)

* AABB는 디버깅 용도로 간주하여 별도 배칭

* 라인 수가 많을 경우:

  * **청크 단위로 분할**
  * Dynamic Vertex Buffer에 수용 가능한 만큼 나누어 렌더링

---

## 🔤 FName

* LookUpTable 기반으로:

  * `DisplayIndex`
  * `ComparisonIndex`
    를 부여

* 문자열 비교 대신 **ComparisonIndex 기반 비교 수행**

  * 문자 단위 비교보다 훨씬 빠름

---

## 🔺 Index Buffer

* Index Buffer를 사용하여:

  * VertexBuffer에 저장되는 정점 수 절약
  * 동일 정점을 중복 저장하지 않음

---

## 📐 Grid 사이즈 재조절

* Grid 크기 변경 시:

  * 기존 **IMMUTABLE VertexBuffer 해제**
  * 새로운 VertexBuffer 재할당

---

## 🎬 Scene Manager & Multi Scene

* Scene을 분리하여 관리
* Scene Manager의 역할:

  * 여러 Scene 관리
  * 현재 활성 Scene 관리

### 특징

* 각 Scene은 개별 렌더링 오브젝트 보유
* **활성 Scene만 렌더링**

### Picking 기능

* Scene Manager 패널에서 Picking 가능
* `CTRL`을 이용한 **멀티 피킹 지원**

  * 뷰포트 & 패널 모두 가능

### Multi Picking 동작 기준

* 마지막으로 선택된 오브젝트 기준으로:

  * 이동
  * 회전
  * 스케일 조정
  * 삭제

---

## 🎞️ SubUV

* 하나의 텍스처를 **동적으로 분할하여 UV 좌표 생성**

### 설정 가능 요소

* 행(Row) / 열(Column) 분할 수
* 사용할 범위
* 프레임 수
* 재생 속도
* 루프 여부

### 현재 UI에서 지원

* 크기
* 프레임 속도
* 루프 여부

---

## 🅰️ Dynamic Glyph Atlas

한글 텍스트 지원을 위한 방식

### 기존 방식 문제

* 모든 한글 글자를 미리 Bake → 비효율적

### 해결 방식

* **필요한 글리프만 런타임에 생성**

### 동작 과정

1. 문자열에서 각 문자 → **UTF-32 코드포인트 추출**
2. ImGui에서 사용하는 라이브러리를 이용해:

   * 폰트 파일로부터 글자 Bitmap 생성
3. 생성된 Bitmap을:

   * **빈 텍스트 아틀라스에 삽입**
4. 코드포인트 ↔ 아틀라스 위치 매핑 저장

### 장점

* 이미 생성된 글자는 재사용
* 메모리 절약 및 유연성 확보
