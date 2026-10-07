# vehicle-body-control

가상 차량의 차체 편의 기능 팀 프로젝트 (사양서 VBC-SRS-001).

차량 상태(속도·기어·시동·조도·강우량·도어·스위치)를 입력받아 전조등, 와이퍼, 도어 잠금, 계기판 경고, 센서 고장 표시를 출력한다.

## 범위
| 구분 | 기능 | 비고 |
|---|---|---|
| 필수 | 조명(L), 와이퍼(W), 도어·잠금·계기판 경고(D), 공통 규칙(C) | 사양서 4·5장 |
| 선택 | 주행(R) | 사양서 6.1장 |
| 선택 | 팀 추가 기능(X) | 사양서 6.2장, 요구사항을 먼저 작성 |

> 속도·기어·시동·조도·강우량은 값이 같아도 0.5초보다 짧은 간격으로 계속 보내야 한다 (사양서 3.1, C2~C4).

## 팀
| 이름 | GitHub | 담당 |
|---|---|---|
| 김성진 (팀장) | @Dokbudoli | B 조명 (L1~L6) |
| 이태식 (GitHub 관리) | @taesik-lee303 | A 코어·통합 (C1~C5), F 입력/시연·주행 |
| 이지윤 | @Kenshi00 | C 와이퍼 (W1~W5) |
| 김의현 | @uihyun-kim | D 도어·잠금 (D1~D3) |
| 김주형 | @kjh082427 | E 계기판 (D4~D5) |

자세한 역할과 검증자는 [docs/roles.md](docs/roles.md).

## 시작하기 (팀원)
1. GitHub 관리자(이태식)의 초대를 수락한다.
2. `gh repo clone taesik-lee303/vehicle-body-control`
3. 그 폴더에서 `claude` 실행 (사용법과 확인 방법: [docs/claude-code-guide.md](docs/claude-code-guide.md))

## 협업 규칙
- `main`에 직접 push하지 않는다. 브랜치 → 작은 PR → 동료 리뷰 → 머지.
- 만든 사람과 검증하는 사람은 다르다.
- PR에는 요구사항 ID와 테스트 결과를 붙인다.
- 자세한 내용은 [CLAUDE.md](CLAUDE.md), [증명 기록](docs/verification.md) 참고.
## 시스템 작동 흐름

```
터미널에서 사용자 입력 받음
         ↓
    main.c
    ├─ 입력 파싱 (속도, 기어, 시동, 조도, 강우, 도어 등)
    ├─ vbc_rx_*() / vbc_set_*() 호출 ← 입력 저장
    ├─ vbc_step() 호출 ← 한 주기 실행
    │   ├─ common.c: C2~C4 판정 (VbcContext 만들기)
    │   ├─ wipers_step() 호출 ← C 와이퍼
    │   ├─ lighting_step() 호출 ← B 조명
    │   ├─ doors_step() 호출 ← D 도어·잠금
    │   └─ dashboard_step() 호출 ← E 계기판
    │       ↓ (각 모듈이 sys.out 업데이트)
    ├─ sys.out 읽기 (최종 결과)
    └─ 터미널에 출력
         ↓
    화면에 표시
```

각 모듈은 공통 인터페이스(`VbcContext`)를 통해 독립적으로 작동하므로, 입력/출력 방식을 바꿔도 코어 로직은 건드리지 않는다.

