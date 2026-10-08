# 모듈별 테스트 콘솔 (신호 주입기)

사용법, 원칙, TC 작성법, 발견 사항: [docs/test/README.md](../../docs/test/README.md)
실행한 TC와 결과(자동 생성): [docs/test/tc-results.md](../../docs/test/tc-results.md)

```bash
make con-doors      # 대화형 (con-dashboard, con-lighting, con-wipers)
make tc             # tc/*.txt 전체 실행 → 결과 문서 갱신
```

| 파일 | 내용 |
|---|---|
| `modcon.c/h` | 신호 주입기 공통 틀: 명령, 송신, 1ms 시간 진행, 판정 표시, e/obs/tc |
| `con_<모듈>.c` | 모듈별 콘솔 (메뉴에 보일 신호와 호출할 step 함수만 정의) |
| `tc/<모듈>.txt` | 테스트 케이스 |
| `tc/run_tc.py` | TC 실행·결과 문서 생성 (`make tc`) |
