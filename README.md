![jungle_game_tech_lab_logo.png](./docs/media/jungle_game_tech_lab_logo.png)

# jungle-gtl-engine

**한국어** | [English](./README_en.md)

***[크래프톤 정글 게임테크랩](https://jungle.krafton.com/courses/gametechlab): 엔진 제작 실습 · 게임잼***

2026.02. ~ 2026.06.

C++20과 DirectX 11로 만든 **3D 게임 엔진**입니다. 주차마다 주제에 맞춰 렌더링, 에디터, 조명과 그림자, 애니메이션, 파티클, 물리 기능을 구현했고, 게임잼에서는 이 엔진으로 **Watermelon Game**, **BLACK OUT**, **FRACTURE LINE**을 만들었습니다.


## 🎮 프로젝트 소개

- **크래프톤 정글 게임테크랩**은 게임 엔진을 깊이 이해하고 직접 설계하고 구현할 수 있는 게임 프로그래머를 목표로 하는 24주 합숙 과정입니다. C++로 자체 게임 엔진을 만들고, 언리얼 엔진의 플러그인 / 툴을 개발합니다

- 이 저장소는 **엔진 제작 실습**의 결과물을 담고 있습니다. 주어진 주제에 맞춰 렌더링, 에디터, 조명과 그림자, 애니메이션, 파티클, 물리 같은 엔진 기능을 하나씩 구현했습니다

- 매주 **팀을 새로 꾸려** 그 주의 목표를 함께 구현했으며, 짧은 기간 안에 결과물을 완성하는 **게임잼**을 진행했습니다


## 📸 미리보기

![week03-1.gif](./week03/Docs/Media/week03-1.gif)

![week05-1.gif](./week05/Docs/Media/week05-1.gif)

![week06-3.gif](./week06/Docs/Media/week06-3.gif)

![week09-1.gif](./week09/Docs/Media/week09-1.gif)

![week11-2.gif](./week11/Docs/Media/week11-2.gif)

![week12-1.gif](./week12/Docs/Media/week12-1.gif)

![week13-1.gif](./week13/Docs/Media/week13-1.gif)

![week14-2.gif](./week14/Docs/Media/week14-2.gif)


## 📅 주차 목록

| 주차 | 주제 | 기간 | 담당 |
|------|------|------|------|
| **[🎱 0주차](./week00/README.md)** | DirectX 11 공 물리 시뮬레이션 | 2026.02. | DirectX 11 렌더링과 공 물리 시뮬레이션 |
| **[🍉 1주차](./week01/README.md)** | 1st Game Jam: Watermelon Game | 2026.03.09. ~ 2026.03.12. | 과일 병합과 점수 |
| **[🧊 2주차](./week02/README.md)** | Break into 3D | 2026.03.12. ~ 2026.03.19. | 피킹과 직렬화 |
| **[🔤 3주차](./week03/README.md)** | 텍스처와 와이어프레임 | 2026.03.19. ~ 2026.03.26. | 텍스트 렌더링 |
| **[🪟 4주차](./week04/README.md)** | OBJ 가져오기, 정적 메시, 다중 뷰포트 렌더링 | 2026.03.26. ~ 2026.04.02. | 엔진 코어와 에디터 도구 |
| **[🥧 5주차](./week05/README.md)** | 2nd Game Jam - Brute Force Competition<br>It's PIE Time! | 2026.04.02. ~ 2026.04.09. | 렌더링 최적화, PIE와 게임플레이 흐름 |
| **[🍳 6주차](./week06/README.md)** | Decal<br>P.S. I Love You - Clearing the Fog | 2026.04.09. ~ 2026.04.16. | 투영 데칼과 FXAA |
| **[💡 7주차](./week07/README.md)** | 빛이 있으라 | 2026.04.16. ~ 2026.04.23. | 라이팅과 에디터 버그 수정 |
| **[🌗 8주차](./week08/README.md)** | 빛이 있으면 그림자도 있다 | 2026.04.23. ~ 2026.04.30. | 에디터 입력 |
| **[🔋 9주차](./week09/README.md)** | 3rd Game Jam - 확실히 위임하라: BLACK OUT<br>Light, Camera, Action! | 2026.04.30. ~ 2026.05.07. | Lua 스크립팅과 게임 기획 |
| **[🦴 10주차](./week10/README.md)** | 무게 그 이상의 의미 | 2026.05.07. ~ 2026.05.14. | 스켈레탈 메시 컴포넌트와 CPU 스키닝 |
| **[🏃 11주차](./week11/README.md)** | 뛰어! | 2026.05.14. ~ 2026.05.21. | 애니메이션 런타임과 상태 머신 |
| **[🌪️ 12주차](./week12/README.md)** | 먼지의 춤 | 2026.05.21. ~ 2026.05.28. | 파티클 코어 |
| **[🧲 13주차](./week13/README.md)** | 3차원 세상에서 초점과 힘의 이해 | 2026.05.28. ~ 2026.06.04. | 천 시뮬레이션 |
| **[🎯 14주차](./week14/README.md)** | 4th Game Jam - 모든 준비는 끝났다. 이제 게임이 시작된다!: FRACTURE LINE | 2026.06.04. ~ 2026.06.09. | 레벨 디자인과 게임플레이 프레임워크 |


## 🛠️ 기술 스택

- **Language**: C++20, Lua

- **Graphics API**: DirectX 11

- **Build**: Visual Studio, MSBuild, vcpkg

- **Libraries**: Autodesk FBX SDK, Dear ImGui, DirectX Tool Kit for DirectX 11, fifo_map, FMOD Engine, imgui-node-editor, JSON for Modern C++, LuaJIT, miniaudio, NvCloth, NVIDIA PhysX SDK, RmlUi, SimpleJSON, sol2, SoLoud, stb, XInput


## 📁 디렉토리 구조

```
jungle-gtl-engine/
├── LICENSE // 저작권과 이용 안내
├── docs/   // README 미디어
├── week00/ // 0주차 프로젝트
├── week01/ // 1주차 프로젝트
├── week02/ // 2주차 프로젝트
├── week03/ // 3주차 프로젝트
├── week04/ // 4주차 프로젝트
├── week05/ // 5주차 프로젝트
├── week06/ // 6주차 프로젝트
├── week07/ // 7주차 프로젝트
├── week08/ // 8주차 프로젝트
├── week09/ // 9주차 프로젝트
├── week10/ // 10주차 프로젝트
├── week11/ // 11주차 프로젝트
├── week12/ // 12주차 프로젝트
├── week13/ // 13주차 프로젝트
└── week14/ // 14주차 프로젝트
```


## 👥 함께한 사람들

함께 엔진과 게임을 만든 분들입니다. 함께해 주셔서 감사합니다.

- 강건우([@keonwookang0914](https://github.com/keonwookang0914))

- 강명호([@DDing-Ho](https://github.com/DDing-Ho))

- 국동진([@puter2712git](https://github.com/puter2712git))

- 권현수([@kwonhyeonsoo-goo](https://github.com/kwonhyeonsoo-goo))

- 김기홍([@lin-ion](https://github.com/lin-ion))

- 김기훈([@kim-kihoon](https://github.com/kim-kihoon))

- 김연하([@Yonaim](https://github.com/Yonaim))

- 김지수([@jskim-research](https://github.com/jskim-research))

- 김태현([@MozziDog](https://github.com/MozziDog))

- 김형도([@DKael](https://github.com/DKael))

- 김형준([@Rocketstein](https://github.com/Rocketstein))

- 김호준([@esc10946](https://github.com/esc10946))

- 김효범([@deepBeom](https://github.com/deepBeom))

- 남윤지([@yoonjiii218](https://github.com/yoonjiii218))

- 박상혁([@UltraSion](https://github.com/UltraSion))

- 박세영([@mathison0](https://github.com/mathison0))

- 박준혁([@CaptainTangerine](https://github.com/CaptainTangerine))

- 성원희([@wwonnn](https://github.com/wwonnn))

- 심우진([@shimwoojin](https://github.com/shimwoojin))

- 양현석([@yanggogi1423](https://github.com/yanggogi1423))

- 오준혁([@JHCard7872](https://github.com/JHCard7872))

- 이상훈([@lsh-Brecht](https://github.com/lsh-Brecht))

- 이준협([@JunHyeop3631](https://github.com/JunHyeop3631))

- 이호진([@lois505](https://github.com/lois505))

- 전현길([@alogeclock](https://github.com/alogeclock))

- 정지윤([@jiyoon607](https://github.com/jiyoon607))

- 정찬일([@Chanil-Chong](https://github.com/Chanil-Chong))

- 조상현([@HaXXOrBunny](https://github.com/HaXXOrBunny))

- 조현석([@chodott](https://github.com/chodott))

- 한영서([@persona6whenrelease](https://github.com/persona6whenrelease))


## 🧹 정리하고, 닦고, 봉인하자!

- 발제 **17회**

- 게임잼 **4회**

- 입소일로부터 **93일**

- 주말 **26일**

- 17시간 × 93일 = **1,581시간**


## ⚖️ 라이선스

이 저장소의 코드 중 장민준이 작성한 부분은 MIT 라이선스로 공개하며, 다른 기여자가 작성한 부분의 권리는 각 기여자에게 있습니다. 서드파티 라이브러리와 에셋, 크래프톤 정글에서 제공한 자료는 각 권리자의 조건을 따릅니다. 이 저장소는 크래프톤 또는 크래프톤 정글의 공식 저장소가 아닙니다. 자세한 내용은 [LICENSE](./LICENSE)를 확인해 주세요.


## 📬 연락처

문의는 장민준(jmj0ok@gmail.com)에게 연락해 주세요.
