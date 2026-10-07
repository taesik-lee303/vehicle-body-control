# 모듈별 테스트 콘솔

사용법, 콘솔별 테스트 순서, 규격 밖 값 넣기, 예제 파일은 저장소 최상위 [README.md](../../README.md)에 있습니다.

```bash
make con-doors       # 도어·잠금 D1~D3
make con-dashboard   # 계기판 경고 D4·D5
make con-lighting    # 조명 L1~L6
make con-wipers      # 와이퍼 W1~W5
./build/con_wipers < tools/consoles/examples/wipers.txt   # 예제 입력으로 한 번에
```
