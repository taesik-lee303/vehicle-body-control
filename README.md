# 모듈별 개별 테스트 (test/module-consoles 브랜치)

도어·계기판·조명·와이퍼 모듈을 **실제 차량을 시험하듯 하나씩** 시험하는 브랜치입니다.
신호를 걸러 내지 않고 그대로 넣는 **신호 주입기**로 각 모듈의 `step` 함수만 실행해, 입력에 따라 출력이 어떻게 바뀌는지 확인합니다.

| 문서 | 내용 |
|---|---|
| **[docs/test/README.md](docs/test/README.md)** | 원칙, 명령, TC 작성법, 발견 사항(질의 후보 Q8~Q11), 한계, 직접 시험할 때 찾아볼 관점 |
| **[docs/test/tc-results.md](docs/test/tc-results.md)** | 실행한 테스트 케이스 104개와 결과 (자동 생성) |
| `docs/test/logs/` | 실행 로그 (증거) |
| `tools/consoles/tc/*.txt` | 테스트 케이스 정의 |

> 전체 시스템을 함께 돌려 보는 브라우저 시뮬레이터는 `develop` 브랜치 README를 보세요.

## 준비
Windows는 WSL Ubuntu 안에서 진행합니다. emsdk는 필요 없습니다.
```bash
sudo apt update && sudo apt install -y build-essential git python3
gh repo clone taesik-lee303/vehicle-body-control    # 처음 받는 경우
cd vehicle-body-control
git fetch origin && git checkout test/module-consoles
make test                                           # ALL TESTS PASSED 확인
```

## 실행
```bash
make tc              # 테스트 케이스 전체 실행 → docs/test/tc-results.md 갱신 (FAIL 있으면 실패)
make con-doors       # 도어·잠금 D1~D3 대화형
make con-dashboard   # 계기판 경고 D4·D5
make con-lighting    # 조명 L1~L6
make con-wipers      # 와이퍼 W1~W5
```

## 대화형으로 해 보기
```
$ make con-doors
선택> ign on
  → [  0.000s] (판정) 시동 켜짐으로 봄: 아니오 → 예
선택> gear d
  → 변화 없음
선택> speed 15
  → [  0.000s] 도어: 풀림 → 잠김
선택> cut speed                      ← 속도 신호만 끊기
  송신 끊음: speed (마지막 수신 후 0.5초가 지나야 판정이 바뀝니다)
선택> t 0.499                         ← 1ms 단위로 시간 진행
  → 0.499초 동안 변화 없음
선택> t 0.001
  → [  0.500s] (판정) 수신 중단: 아니오 → 예
  → [  0.500s] (판정) 시동 켜짐으로 봄: 예 → 아니오
  → [  0.500s] (판정) 유효한 속도: 예 → 아니오
선택> gear 7                          ← 정의 안 된 코드도 그대로 들어감
  → [  0.500s] (판정) 기어 코드 정의됨: 예 → 아니오
선택> e lock on                       ← 기대 확인
  ✔ PASS [-] lock = on  (0.500s)
```
- 모든 신호에 **아무 정수**(int32)를 넣을 수 있습니다: `gear 7`, `ign 2`, `fl -1`, `light 9`, `speed 99999`.
- 신호마다 끊기(`cut speed`), 다시 잇기(`resume speed`), 한 번만 보내기(`send gear`), 자동 재송신 끄기(`auto off`)가 됩니다.
- 시간은 `t <초>`로 1ms 단위까지 진행합니다.
- `h`를 누르면 명령 전체가 나옵니다. 자세한 설명은 [docs/test/README.md](docs/test/README.md) 3절에 있습니다.

## 현재 결과 (2026-10-08)
| 모듈 | TC | FAIL | 관찰(사양서에 없는 동작) 포함 |
|---|---|---|---|
| 도어·잠금 | 25 | 0 | 5 |
| 계기판 경고 | 23 | 0 | 3 |
| 조명 | 32 | 0 | 4 |
| 와이퍼 | 24 | 0 | 1 |

사양서 기준 확인은 모두 통과했습니다. 정의 안 된 코드(기어 7, 시동 2, 도어 2, 조명 스위치 7)에서 사양서에 처리 방법이 없는 동작 4가지가 드러났고, **질의 후보 Q8~Q11**로 정리했습니다(기어 신호가 깨지면 정차해도 문이 안 풀리는 경우 포함). 자세한 내용은 [docs/test/README.md](docs/test/README.md) 5절에 있습니다.

## 팀이 할 일
1. `docs/test/tc-results.md`의 TC를 **리뷰**합니다. 기대값이 사양서에서 맞게 계산됐는지 확인해 주세요. TC를 만든 사람(Claude Code)과 검증하는 사람은 달라야 합니다.
2. **빠진 테스트를 찾아** `tools/consoles/tc/<모듈>.txt`에 추가하고 `make tc`를 실행합니다. 찾아볼 관점은 [docs/test/README.md](docs/test/README.md) 8절에 있습니다.
3. Q8~Q11을 강사에게 질의하고, 답에 따라 코드와 TC를 고칩니다.
