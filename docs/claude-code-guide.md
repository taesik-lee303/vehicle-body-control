# 팀원용 Claude Code 로컬 사용 안내

이 저장소의 `CLAUDE.md`와 `docs/ref/`를 내 PC의 Claude Code가 읽게 하는 방법이다. 따로 설정할 것은 없고, **저장소 폴더에서 `claude`를 실행**하면 된다.

## 1. 한 번만 준비
| 항목 | 확인 방법 |
|---|---|
| Git, GitHub CLI(`gh`) | `git --version`, `gh auth status` (로그인 안 돼 있으면 `gh auth login`) |
| C 빌드 도구 | `gcc --version`, `make --version` (Windows는 WSL Ubuntu 안에서 설치·실행 권장) |
| Claude Code | 설치 후 `claude` 실행, 로그인 |

> Windows 사용자: 저장소를 **WSL 안(예: `~/work/...`)** 에 clone하고 WSL 터미널에서 `claude`를 실행한다. `C:\` 쪽에 두고 WSL에서 열면 빌드·권한 문제가 생기기 쉽다.

## 2. 저장소 받기

### 처음 받는 경우
```bash
gh repo clone taesik-lee303/vehicle-body-control
cd vehicle-body-control
claude
```

### 이미 받아 둔 경우
```bash
cd vehicle-body-control
git fetch origin
git checkout main && git pull           # main을 최신으로
git checkout feature/<내 브랜치>
git merge origin/main                   # 내 브랜치에도 CLAUDE.md, docs/ref 반영
claude
```
- `CLAUDE.md`는 **지금 체크아웃된 브랜치의 파일**을 읽는다. 내 브랜치에 `main`을 merge하지 않으면 새 내용이 안 보인다.
- merge 후에는 `make test`(있는 브랜치에서)를 한 번 돌려 상태를 확인한다.

## 3. 제대로 읽었는지 확인
1. 저장소 **최상위 폴더**에서 `claude`를 실행한다 (하위 폴더에서도 상위의 `CLAUDE.md`는 읽지만, 확인은 최상위에서 한다).
2. Claude Code에서 `/memory`를 입력해 `CLAUDE.md`가 로드 목록에 있는지 본다.
3. 질문으로 확인한다.
   ```
   이 프로젝트의 작업 규칙과 참고 문서 위치를 요약해줘
   ```
   `main`에 직접 커밋하지 않기, 기대값은 사양서에서 계산하기, `docs/ref/` 안내가 나오면 정상이다.

## 4. 이렇게 쓰면 좋다
`docs/ref/`는 자동으로 다 읽히지 않는다. 필요한 때 파일을 지정해서 부른다.

| 하고 싶은 일 | 프롬프트 예 |
|---|---|
| 내 모듈 테스트 설계 | `docs/ref/sw-testing.md의 경계값 3-value 방식으로 L2(1초), L3(3초) 테스트케이스를 설계해줘. 기대값은 사양서 기준으로` |
| 0.5초 판정 근거 설명 | `docs/ref/vehicle-comm.md를 참고해서 C2~C4가 통신의 어떤 개념에 해당하는지 발표용으로 설명해줘` |
| 안전 규칙 이유 설명 | `docs/ref/functional-safety.md를 보고 L5, W5, D1이 안전 측으로 정해진 이유를 설명해줘` |
| 검증 기록 작성 | `docs/verification.md 형식에 맞춰 내 모듈 결과를 정리해줘` |

### 꼭 지킬 것
- 요구사항의 근거는 **사양서(VBC-SRS-001)** 다. `docs/ref/`는 배경 지식이고, 다르면 사양서가 맞다.
- 문서의 `[보강]`·`[일반]` 표시는 강의 PDF에 없는 일반 지식이다. 근거로 인용하지 않는다.
- AI가 시킨 대로 안 하거나 사양서와 다르게 답하면 **고치고, `docs/verification.md`의 "AI가 틀린 곳"에 기록**한다 (발표 항목).
- 테스트의 기대값은 코드 실행 결과로 채우지 않는다. 사양서에서 계산한다.

## 5. 알아둘 점
- **자동 메모리는 공유되지 않는다.** Claude Code가 내 PC에 쌓는 기억은 내 PC에만 있다. 팀이 같이 알아야 할 내용은 `CLAUDE.md`나 `docs/`에 적어 PR로 올린다.
- **개인 취향 설정**은 저장소 최상위에 `CLAUDE.local.md`를 만들어 적는다 (`.gitignore`에 들어 있어 커밋되지 않는다).
- **`CLAUDE.md`·`docs/ref/` 수정**은 다른 코드처럼 브랜치 → 작은 PR → 리뷰로 한다. 직접 `main`에 push하지 않는다.
- **같은 계정을 여러 명이 쓰는 경우** 사용 한도가 함께 소진된다. 긴 문서를 통째로 읽히기보다 필요한 파일만 지정한다.

## 6. 문제 해결
| 증상 | 확인 |
|---|---|
| Claude가 프로젝트 규칙을 모른다 | 저장소 폴더에서 실행했는지, `ls CLAUDE.md`로 파일이 있는지, `git branch --show-current`가 `main`을 merge한 브랜치인지 |
| `docs/ref/`가 없다 | `git merge origin/main`을 했는지 (내 브랜치에 아직 안 들어온 것) |
| `docs/c-structure.md`, `docs/console.md`가 없다 | 이 파일들은 `develop`/`feature/sim` 쪽 브랜치에 있다. 해당 브랜치를 merge한 뒤 쓴다 |
| `gh`가 로그인 안 돼 있다 | `gh auth login` |
