# CLAUDE.md

가상 차량 차체 편의 기능 팀 프로젝트. C 언어로 구현한다. 요구사항의 근거는 사양서 VBC-SRS-001이다 (`docs/spec/VBC-SRS-001_v1.0.pdf`, 요약은 `docs/requirements.md`).

## 빌드와 테스트
- `make test`: 모든 테스트 실행. 변경 후 반드시 돌리고 결과를 PR에 붙인다.
- `make run`: 터미널 입력 실행. 명령은 `docs/console.md`.
- `make scenarios`: `scenarios/*.txt` 시나리오 실행 (요구사항 증명·시연용).
- 구조, 모듈 작성 규칙, 테스트 작성법: `docs/c-structure.md`. 담당: `docs/roles.md`.

## 작업 규칙
- `main`에 직접 커밋하지 않는다. 자기 담당 `feature/<모듈>` 브랜치(core, lighting, wipers, doors, dashboard, input)에서 작업하고 `develop`으로 PR한다.
- 모듈은 `VbcContext`만 보고 판단한다. 시간은 `ctx->now`만 쓴다 (`time()`, `sleep()` 금지).
- `src/common.h`, `src/vbc.h` 시그니처 변경은 A(코어)와 합의 후 `feature/core`에서만 한다.
- PR은 작게, 맡은 범위 안의 파일만 바꾼다.
- 테스트를 지우거나 기대값을 바꾸지 않는다. 기대값은 사양서에서 계산한다.
- 모든 변경에 요구사항 ID(C, L, W, D, R, X, CR-01)를 붙인다.
- 경계값(예: 2.9초/3초)과 조건별 경우(범위 밖, 끊김, 첫 수신 전, 정상 복귀)를 확인한다.
- 사양서에 없는 동작은 임의로 정하지 않는다. `docs/c-structure.md`의 질의 후보 표에 추가하고 팀에 알린다.
- 충돌을 합친 뒤에는 테스트를 다시 돌린다.

## 기록
- `docs/verification.md`에 요구사항 → 입력 → 기대 → 실제 → 증거를 남긴다.
- AI가 틀린 곳과 바로잡은 과정도 기록한다 (발표 항목 4).

## 참고 지식 (`docs/ref/`, 필요할 때만 읽는다)
요구사항의 근거는 사양서이고, 아래는 설명·테스트 설계용 배경이다. 사양서와 다르면 사양서가 맞다. 색인: `docs/ref/README.md`.
- 테스트 설계(동치분할, 경계값 3-value, 결정표, MC/DC), TC 양식, 리뷰·결함 기록: `docs/ref/sw-testing.md`
- C2~C4의 통신 개념(주기 송신, 수신 timeout, 유효성), 입력 신호 DBC풍 정의(제안): `docs/ref/vehicle-comm.md`
- 안전 우선 규칙(L5, W5, D1, C4)의 이유, HAZOP/ASIL: `docs/ref/functional-safety.md`
- VBC를 AUTOSAR 용어로 설명, 하지 않는 범위: `docs/ref/autosar.md`
- MCU 이식 시 대응과 주의(volatile, 32비트 ms tick 오버플로): `docs/ref/embedded-c.md`

## 테스트 작성 시 기억할 것
- 경계는 3값으로 본다: 기준 직전 / 기준 / 기준 직후 (예: 499 / 500 / 501 ms, 14 / 15 / 16 km/h, 9 / 10 / 11 %). `>=`/`>`는 사양서 문구로 확정하고 기대값을 사양서에서 계산한다.
- 범위가 있는 입력은 유효 1개 + 무효(아래, 위)를 각각 따로 시험한다.
- N초 연속은 "조건이 깨졌다가 다시 성립하면 처음부터"까지 확인한다 (2.9초 유지 → 해제 → 재성립).
- 값이 같아도 재수신이면 수신 시각이 갱신되어야 한다 (사양서 3.1).
- 문서의 `[보강]`·`[일반]` 표시는 강의 PDF에 없는 일반 지식이다. 근거로 인용하지 않는다.
