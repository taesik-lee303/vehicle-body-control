# CLAUDE.md

가상 차량 차체 편의 기능 팀 프로젝트. C 언어로 구현한다. 요구사항의 근거는 사양서 VBC-SRS-001이다 (`docs/spec/VBC-SRS-001_v1.0.pdf`, 요약은 `docs/requirements.md`).

## 빌드와 테스트
- `make test`: 모든 테스트 실행. 변경 후 반드시 돌리고 결과를 PR에 붙인다.
- `make run`: 데모 실행.
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
