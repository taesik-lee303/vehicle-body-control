# 모듈별 테스트 케이스 실행 결과

> 자동 생성 문서 (`make tc`, 2026-10-13 14:00, 커밋 `13721db` (커밋 안 된 변경 포함)). 손으로 고치지 마세요.
> TC 정의: `tools/consoles/tc/<모듈>.txt`, 실행 로그(증거): `docs/test/logs/<모듈>.log`. 사용법은 `docs/test/README.md`.

판정: **PASS** 사양서에서 계산한 기대와 같음 / **FAIL** 다름 / **관찰** 사양서에 정해지지 않은 동작이라 기대 없이 기록만 함.

## 요약

| 모듈 | TC | PASS | FAIL | 관찰 포함 | 확인(e) 건수 | 관찰(obs) 건수 |
|---|---|---|---|---|---|---|
| BCU 전체 (ECU) | 21 | 21 | 0 | 0 | 59 | 0 |
| 도어·잠금 (D1~D3) | 30 | 29 | 0 | 5 | 40 | 7 |
| 계기판 경고 (D4·D5) | 23 | 23 | 0 | 3 | 30 | 3 |
| 조명 (L1~L6) | 32 | 30 | 0 | 4 | 42 | 6 |
| 와이퍼 (W1~W5) | 24 | 24 | 0 | 1 | 50 | 1 |

## BCU 전체 (ECU)

실행: `./build/con_ecu < tools/consoles/tc/ecu.txt` · 로그: `docs/test/logs/ecu.log`

| TC ID | 요구사항 | 기법 | 입력 | 기대 (사양서) | 실제 | 판정 |
|---|---|---|---|---|---|---|
| TC-ECU-C5-01 | C5 | 시작 상태 | 초기 상태 | 전조등 꺼짐, 와이퍼 정지, 풀림, 경고 없음 | 기대와 같음 (확인 4건: headlight=off, wiper=stop, lock=off, warning=none) | PASS |
| TC-ECU-L6-01 | L6, C1, W4 | 경계값(시간 3-value) | 시동 ON, D, 속도 50, 조명 AUTO, 조도 1200, 와이퍼 LOW, 9.999초 | 와이퍼 저속, 전조등 꺼짐 | 기대와 같음 (확인 2건: wiper=low, headlight=off) | PASS |
| TC-ECU-L6-02 | L6, C1 | 경계값(시간 3-value) | 이어서 +1ms (10.000초) | 켜짐 | 기대와 같음 (확인 1건: headlight=on) | PASS |
| TC-ECU-L6-03 | L6, C1 | 경계값(시간 3-value) | 이어서 +1ms (10.001초) | 켜짐 | 기대와 같음 (확인 1건: headlight=on) | PASS |
| TC-ECU-L6-04 | L6, L3 | 상태 전이(직전 유지) | 이어서 와이퍼 OFF, 3초 (조도 1200) | 와이퍼 정지, 전조등 켜짐 유지 (L3: 어느 규칙도 정하지 않음) | 기대와 같음 (확인 2건: wiper=stop, headlight=on) | PASS |
| TC-ECU-L6-05 | L6, C1 | 재성립(처음부터) | 처음부터: 와이퍼 LOW 5초 → OFF 1초 → LOW 9.999초 | 꺼짐 | 기대와 같음 (확인 1건: headlight=off) | PASS |
| TC-ECU-L6-06 | L6, C1 | 재성립(처음부터) | 이어서 +1ms (다시 LOW 후 10.000초) | 켜짐 | 기대와 같음 (확인 1건: headlight=on) | PASS |
| TC-ECU-L6-07 | L6, W2, W3 | 결정 테이블 | 처음부터: 와이퍼 AUTO, 강우 10, 속도 0, 조명 AUTO, 조도 1200, 10초 | 와이퍼 정지 (W2 간헐을 W3로 낮춤), 전조등 꺼짐 | 기대와 같음 (확인 2건: wiper=stop, headlight=off) | PASS |
| TC-ECU-L6-08 | L6, W2, W3, C1 | 결정 테이블 + 경계값 | 이어서 속도 1 → 9.999초 → +1ms | 와이퍼 간헐, 9.999초 꺼짐, 10.000초 켜짐 | 기대와 같음 (확인 3건: wiper=int, headlight=off, headlight=on) | PASS |
| TC-ECU-D5-01 | D5, L5, L4, W5, W4 | 결정 테이블 | 처음부터: 시동 ON, D, 속도 30, 조도 60001, 강우 101 (스위치 둘 다 OFF) | 경고 조도 고장, 고장 표시 둘 다 켜짐, 전조등 켜짐 (L5 > L4), 와이퍼 정지 (W4) | 기대와 같음 (확인 7건: luxfault=yes, rainfault=yes, warning=lux, luxlamp=on, rainlamp=on, headlight=on, wiper=stop) | PASS |
| TC-ECU-D5-02 | D5, D4 | 결정 테이블 | 이어서 FL 열림 | 주행 중 도어 열림 | 기대와 같음 (확인 1건: warning=door) | PASS |
| TC-ECU-D5-03 | D5 | 결정 테이블 | 이어서 FL 닫힘 | 조도 고장 | 기대와 같음 (확인 1건: warning=lux) | PASS |
| TC-ECU-D5-04 | D5, L5, L4 | 결정 테이블 | 이어서 조도 30000 | 경고 강우 고장, 조도 고장 표시 꺼짐, 전조등 꺼짐 (L4 OFF) | 기대와 같음 (확인 3건: warning=rain, luxlamp=off, headlight=off) | PASS |
| TC-ECU-D5-05 | D5, W5 | 결정 테이블 | 이어서 강우 50 | 경고 없음, 강우 고장 표시 꺼짐 | 기대와 같음 (확인 2건: warning=none, rainlamp=off) | PASS |
| TC-ECU-D5-06 | D4, D5, C3 | 경계값(속도 3-value) | 이어서 조도 60001, FR 열림, 속도 0 | 조도 고장 (속도 0은 D4 아님) | 기대와 같음 (확인 1건: warning=lux) | PASS |
| TC-ECU-D5-07 | D4, D5 | 경계값(속도 3-value) | 이어서 속도 1 | 주행 중 도어 열림 | 기대와 같음 (확인 1건: warning=door) | PASS |
| TC-ECU-D5-08 | D4, D5, C3 | 동치(무효: 0 미만) | 이어서 속도 -1 | 유효한 속도 아님 → 조도 고장 | 기대와 같음 (확인 2건: speedvalid=no, warning=lux) | PASS |
| TC-ECU-C4-01 | C4, L4, W4, D3, D4 | 경계값(시간 3-value) | 처음부터: 시동 ON, D, 속도 30, 조명 ON, 와이퍼 LOW, FL 열림 → 차량 상태 끊기, 0.499초 | 수신 중단 아님: 켜짐, 저속, 잠김, 도어 열림 경고 | 기대와 같음 (확인 9건: headlight=on, wiper=low, lock=on, warning=door, rxlost=no, headlight=on, wiper=low, lock=on, warning=door) | PASS |
| TC-ECU-C4-02 | C4, L1, W1, D1, C3, D4 | 경계값(시간 3-value) | 이어서 +1ms (0.500초) | 수신 중단: 전조등 꺼짐, 와이퍼 정지, 잠김 유지, 경고 없음 (유효한 속도 아님) | 기대와 같음 (확인 5건: rxlost=yes, headlight=off, wiper=stop, lock=on, warning=none) | PASS |
| TC-ECU-C4-03 | C4, L1, W1, D1 | 경계값(시간 3-value) | 이어서 +1ms (0.501초) | 같음 | 기대와 같음 (확인 5건: rxlost=yes, headlight=off, wiper=stop, lock=on, warning=none) | PASS |
| TC-ECU-C4-04 | C4, L4, W4, D3, D4 | 정상 복귀 | 이어서 차량 상태 송신 재개 | 켜짐, 저속, 잠김, 도어 열림 경고 | 기대와 같음 (확인 5건: rxlost=no, headlight=on, wiper=low, lock=on, warning=door) | PASS |

## 도어·잠금 (D1~D3)

실행: `./build/con_doors < tools/consoles/tc/doors.txt` · 로그: `docs/test/logs/doors.log`

| TC ID | 요구사항 | 기법 | 입력 | 기대 (사양서) | 실제 | 판정 |
|---|---|---|---|---|---|---|
| TC-D-C5-01 | C5 | 시작 상태 | 초기 상태 (시동 OFF, 기어 P) | 풀림 | 기대와 같음 (확인 1건: lock=off) | PASS |
| TC-D3-01 | D3 | 경계값(속도 3-value) | 시동 ON, 기어 D, 속도 14 | 풀림 유지 (15 미만) | 기대와 같음 (확인 1건: lock=off) | PASS |
| TC-D3-02 | D3 | 경계값(속도 3-value) | 이어서 속도 15 | 그 시각에 잠김 | 기대와 같음 (확인 1건: lock=on) | PASS |
| TC-D3-03 | D3 | 경계값(속도 3-value) | 처음부터 시동 ON, 기어 D, 속도 16 | 잠김 | 기대와 같음 (확인 1건: lock=on) | PASS |
| TC-D3-04 | D3 | 상태 전이(직전 유지) | 잠긴 뒤 속도 5로 감속, 2초 | 잠김 유지 | 기대와 같음 (확인 1건: lock=on) | PASS |
| TC-D3-05 | D3, C3 | 동치(무효: 250 초과) + 경계값 | 처음부터 시동 ON, D, 속도 251 | 풀림 (유효한 속도 아님) | 기대와 같음 (확인 2건: speedvalid=no, lock=off) | PASS |
| TC-D3-06 | D3, C3 | 동치(무효: 0 미만) | 속도 -1 | 풀림 | 기대와 같음 (확인 2건: speedvalid=no, lock=off) | PASS |
| TC-D3-07 | D3, C3 | 경계값(유효 상한) | 속도 250 | 잠김 | 기대와 같음 (확인 1건: lock=on) | PASS |
| TC-D3-08 | D3 | 동치(기어 R) | 처음부터 시동 ON, 기어 R, 속도 15 | 잠김 (D3는 기어 조건 없음) | 기대와 같음 (확인 1건: lock=on) | PASS |
| TC-D3-09 | D3 | 동치(기어 N) | 처음부터 시동 ON, 기어 N, 속도 15 | 잠김 | 기대와 같음 (확인 1건: lock=on) | PASS |
| TC-D2-01 | D2 | 결정 테이블(기어 P) | 잠긴 상태(N, 15)에서 기어 P | 풀림 | 기대와 같음 (확인 1건: lock=off) | PASS |
| TC-D2-02 | D2 | 결정 테이블(시동 OFF) | 처음부터 D, 30으로 잠근 뒤 시동 OFF | 풀림 | 기대와 같음 (확인 2건: lock=on, lock=off) | PASS |
| TC-D2-03 | D2 > D3 | 결정 테이블(우선순위) | 처음부터 시동 ON, 기어 P, 속도 30 | 풀림 (잠그지 않음) | 기대와 같음 (확인 1건: lock=off) | PASS |
| TC-D1-01 | D1, C4 | 경계값(시간 3-value) | D, 30으로 잠근 뒤 속도 신호만 끊기, 0.499초 | 수신 중단 아님, 잠김 | 기대와 같음 (확인 2건: rxlost=no, lock=on) | PASS |
| TC-D1-02 | D1, C4 | 경계값(시간 3-value) | 이어서 +1ms (0.500초) | 수신 중단, 잠김 유지 | 기대와 같음 (확인 2건: rxlost=yes, lock=on) | PASS |
| TC-D1-03 | D1, C4 | 경계값(시간 3-value) | 이어서 +1ms (0.501초) | 수신 중단, 잠김 유지 | 기대와 같음 (확인 2건: rxlost=yes, lock=on) | PASS |
| TC-D1-04 | D1 > D2 | 결정 테이블(Negative) | 수신 중단 중 시동 OFF 값 수신 | 잠김 유지 (D1이 먼저) | 기대와 같음 (확인 2건: rxlost=yes, lock=on) | PASS |
| TC-D1-05 | D1 → D2 | 상태 전이(복귀) | 속도 송신 재개 | 수신 정상, 시동 OFF라 풀림 | 기대와 같음 (확인 2건: rxlost=no, lock=off) | PASS |
| TC-D1-06 | D1 | Negative(풀린 상태) | 처음부터 시동 ON, D, 속도 0에서 세 신호 모두 끊기 0.5초 | 풀림 유지 | 기대와 같음 (확인 2건: rxlost=yes, lock=off) | PASS |
| TC-D1-07 | D1 → D3 | 상태 전이(복귀) | 속도 30으로 바꾼 뒤 송신 재개 | 잠김 | 기대와 같음 (확인 1건: lock=on) | PASS |
| TC-D3-10 | D3 | 상태 전이(직전 유지, 상태 주입) | 처음부터 시동 ON, D, 속도 5에서 잠김 주입, 2초 | 잠김 유지 (15 미만은 직전 유지) | 기대와 같음 (확인 1건: lock=on) | PASS |
| TC-D2-04 | D2 | 결정 테이블(기어 P, 상태 주입) | 이어서 기어 P | 풀림 | 기대와 같음 (확인 1건: lock=off) | PASS |
| TC-D2-05 | D2 | 결정 테이블(시동 OFF, 상태 주입) | 처음 상태(시동 OFF, 기어 P)에서 잠김 주입 | 그 시각에 풀림 | 기대와 같음 (확인 1건: lock=off) | PASS |
| TC-D1-08 | D1 | 결정 테이블(상태 주입) | 처음부터 시동 ON, D, 속도 0, 차량 신호 끊기 0.5초 뒤 잠김 주입, 1초 | 수신 중단, 잠김 유지 | 기대와 같음 (확인 2건: rxlost=yes, lock=on) | PASS |
| TC-D1-09 | D1 | 결정 테이블(상태 주입) | 이어서 풀림 주입, 1초 | 수신 중단, 풀림 유지 | 기대와 같음 (확인 2건: rxlost=yes, lock=off) | PASS |
| TC-D-Q8-01 | (정의 없음) Q8 | Negative(기어 코드) | 시동 ON, 기어 코드 7, 속도 30 | 기어 코드 정의 안 됨 판정. 도어 동작은 관찰 | 기대와 같음 (확인 1건: gearvalid=no)<br>관찰 lock=on (0.000s) | PASS + 관찰 |
| TC-D-Q8-02 | (정의 없음) Q8 | Negative(기어 코드) | 잠긴 상태에서 기어 코드 7 유지, 정차(속도 0) | 관찰 | 관찰 lock=on (0.000s) | 관찰 |
| TC-D-Q8-03 | (정의 없음) Q8 | Negative(기어 코드) | 처음부터 시동 ON, 기어 코드 -1, 속도 30 | 관찰 | 기대와 같음 (확인 1건: gearvalid=no)<br>관찰 lock=on (0.000s) | PASS + 관찰 |
| TC-D-Q9-01 | (정의 없음) Q9 | Negative(시동 코드) | 기어 D, 시동 코드 2, 속도 30 | 시동 코드 정의 안 됨 판정. 동작은 관찰 | 기대와 같음 (확인 1건: ignvalid=no)<br>관찰 ignon=yes (0.000s)<br>관찰 lock=on (0.000s) | PASS + 관찰 |
| TC-D-Q9-02 | (정의 없음) Q9 | Negative(시동 코드) | 잠긴 뒤 정차, 시동 코드 -1 | 관찰 | 기대와 같음 (확인 1건: ignvalid=no)<br>관찰 ignon=yes (0.000s)<br>관찰 lock=on (0.000s) | PASS + 관찰 |

## 계기판 경고 (D4·D5)

실행: `./build/con_dashboard < tools/consoles/tc/dashboard.txt` · 로그: `docs/test/logs/dashboard.log`

| TC ID | 요구사항 | 기법 | 입력 | 기대 (사양서) | 실제 | 판정 |
|---|---|---|---|---|---|---|
| TC-E-C5-01 | C5 | 시작 상태 | 초기 상태 | 경고 없음 | 기대와 같음 (확인 1건: warning=none) | PASS |
| TC-D4-01 | D4 | 경계값(속도 3-value) | 문 FL 열림, 속도 0 | 경고 없음 (속도 > 0 아님) | 기대와 같음 (확인 1건: warning=none) | PASS |
| TC-D4-02 | D4 | 경계값(속도 3-value) | 이어서 속도 1 | 주행 중 도어 열림 | 기대와 같음 (확인 1건: warning=door) | PASS |
| TC-D4-03 | D4 | 경계값(속도 3-value) | 이어서 속도 2 | 주행 중 도어 열림 | 기대와 같음 (확인 1건: warning=door) | PASS |
| TC-D4-04 | D4, C3 | 경계값(유효 상한) | 속도 250 | 주행 중 도어 열림 | 기대와 같음 (확인 1건: warning=door) | PASS |
| TC-D4-05 | D4, C3 | 동치(무효: 250 초과) | 속도 251 | 경고 없음 (유효한 속도 아님) | 기대와 같음 (확인 2건: speedvalid=no, warning=none) | PASS |
| TC-D4-06 | D4, C3 | 동치(무효: 0 미만) | 속도 -1 | 경고 없음 | 기대와 같음 (확인 1건: warning=none) | PASS |
| TC-D4-07 | D4 | 동치(문 위치) | 속도 30, FL 닫고 RR만 열림 | 주행 중 도어 열림 | 기대와 같음 (확인 1건: warning=door) | PASS |
| TC-D4-08 | D4 | 동치(문 모두 닫힘) | 속도 30, RR도 닫힘 | 경고 없음 | 기대와 같음 (확인 1건: warning=none) | PASS |
| TC-D4-09 | D4, C3 | 경계값(시간 3-value) | 속도 30, 문 FR 열림, 속도 신호만 끊고 0.499초 | 주행 중 도어 열림 (아직 유효) | 기대와 같음 (확인 1건: warning=door) | PASS |
| TC-D4-10 | D4, C3 | 경계값(시간 3-value) | 이어서 +1ms (0.500초) | 경고 없음 (유효한 속도 없음) | 기대와 같음 (확인 2건: speedvalid=no, warning=none) | PASS |
| TC-D4-11 | D4, C3 | 상태 전이(복귀) | 속도 송신 재개 | 주행 중 도어 열림 | 기대와 같음 (확인 1건: warning=door) | PASS |
| TC-D5-01 | D5 | 결정 테이블(도어+조도+강우) | 처음부터 속도 30, 문 FL 열림, 조도 70000, 강우 101 | 주행 중 도어 열림 | 기대와 같음 (확인 1건: warning=door) | PASS |
| TC-D5-02 | D5 | 결정 테이블(조도+강우) | 이어서 문 닫힘 | 조도 센서 고장 | 기대와 같음 (확인 1건: warning=lux) | PASS |
| TC-D5-03 | D5 | 결정 테이블(강우만) | 이어서 조도 30000 | 강우 센서 고장 | 기대와 같음 (확인 1건: warning=rain) | PASS |
| TC-D5-04 | D5 | 결정 테이블(없음) | 이어서 강우 50 | 경고 없음 | 기대와 같음 (확인 1건: warning=none) | PASS |
| TC-D5-05 | D5, C2 | 경계값(조도 범위) | 조도 60000 → 60001 → -1 | 없음 → 조도 고장 → 조도 고장 | 기대와 같음 (확인 3건: warning=none, warning=lux, warning=lux) | PASS |
| TC-D5-06 | D5, C2 | 경계값(강우 범위) | 조도 30000, 강우 100 → 101 → -1 | 없음 → 강우 고장 → 강우 고장 | 기대와 같음 (확인 3건: warning=none, warning=rain, warning=rain) | PASS |
| TC-D5-07 | D5, C2 | 경계값(시간 3-value) | 강우 50, 조도 송신만 끊고 0.499 / 0.500초 | 없음 → 조도 고장 | 기대와 같음 (확인 2건: warning=none, warning=lux) | PASS |
| TC-D5-08 | D5 | 결정 테이블(시동 무관) | 이어서 시동 OFF | 조도 센서 고장 유지 (D5에 시동 조건 없음) | 기대와 같음 (확인 1건: warning=lux) | PASS |
| TC-E-Q10-01 | (정의 없음) Q10 | Negative(도어 코드) | 처음부터 속도 30, 문 FL 코드 2 | 도어 코드 정의 안 됨 판정. 경고는 관찰 | 기대와 같음 (확인 1건: doorvalid=no)<br>관찰 warning=door (0.000s) | PASS + 관찰 |
| TC-E-Q10-02 | (정의 없음) Q10 | Negative(도어 코드) | 문 FL 코드 -1 | 관찰 | 기대와 같음 (확인 1건: doorvalid=no)<br>관찰 warning=door (0.000s) | PASS + 관찰 |
| TC-E-Q8-01 | (정의 없음) Q8 | Negative(기어 코드) | 문 FL 열림(1), 속도 30, 기어 코드 7 | 관찰 (D4에는 기어 조건 없음) | 기대와 같음 (확인 1건: gearvalid=no)<br>관찰 warning=door (0.000s) | PASS + 관찰 |

## 조명 (L1~L6)

실행: `./build/con_lighting < tools/consoles/tc/lighting.txt` · 로그: `docs/test/logs/lighting.log`

| TC ID | 요구사항 | 기법 | 입력 | 기대 (사양서) | 실제 | 판정 |
|---|---|---|---|---|---|---|
| TC-L-C5-01 | C5 | 시작 상태 | 초기 상태 | 전조등 꺼짐, 조도 고장 표시 꺼짐 | 기대와 같음 (확인 2건: headlight=off, luxlamp=off) | PASS |
| TC-L1-01 | L1 > L4 | 결정 테이블 | 시동 OFF, 스위치 ON | 꺼짐 | 기대와 같음 (확인 1건: headlight=off) | PASS |
| TC-L4-01 | L4 | 결정 테이블 | 시동 ON, 스위치 ON (조도 30000) | 켜짐 | 기대와 같음 (확인 1건: headlight=on) | PASS |
| TC-L4-02 | L4 | 결정 테이블 | 스위치 OFF, 조도 300, 2초 | 꺼짐 (어두워도) | 기대와 같음 (확인 1건: headlight=off) | PASS |
| TC-L2-01 | L2, C1 | 경계값(시간 3-value) | 처음부터 시동 ON, AUTO, 조도 999 후 0.999초 | 꺼짐 | 기대와 같음 (확인 1건: headlight=off) | PASS |
| TC-L2-02 | L2, C1 | 경계값(시간 3-value) | 이어서 +1ms (1.000초) | 켜짐 | 기대와 같음 (확인 1건: headlight=on) | PASS |
| TC-L2-03 | L2 | 경계값(조도 3-value) | 처음부터 시동 ON, AUTO, 조도 1000, 3초 | 꺼짐 (1000 미만 아님) | 기대와 같음 (확인 1건: headlight=off) | PASS |
| TC-L2-04 | L2, C1 | 상태 전이(연속 끊김 → 다시 셈) | 조도 300 0.9초 → 1000 0.1초 → 300 0.999초 | 꺼짐 (처음부터 다시 셈) | 기대와 같음 (확인 1건: headlight=off) | PASS |
| TC-L2-05 | L2, C1 | 상태 전이(연속 끊김 → 다시 셈) | 이어서 +1ms | 켜짐 | 기대와 같음 (확인 1건: headlight=on) | PASS |
| TC-L3-01 | L3, C1 | 경계값(시간 3-value) | 켜진 상태에서 조도 1501, 2.999초 | 켜짐 | 기대와 같음 (확인 1건: headlight=on) | PASS |
| TC-L3-02 | L3, C1 | 경계값(시간 3-value) | 이어서 +1ms (3.000초) | 꺼짐 | 기대와 같음 (확인 1건: headlight=off) | PASS |
| TC-L3-03 | L3 | 경계값(조도) + 직전 유지 | 조도 300으로 켠 뒤 조도 1500, 5초 | 켜짐 유지 (1500 초과 아님) | 기대와 같음 (확인 2건: headlight=on, headlight=on) | PASS |
| TC-L3-04 | L3 | 동치(1000~1500 직전 유지, 꺼진 상태) | 조도 30000으로 끈 뒤 조도 1200, 5초 | 꺼짐 유지 | 기대와 같음 (확인 2건: headlight=off, headlight=off) | PASS |
| TC-L2-06 | L2, C1 | 동치(AUTO가 아닐 때 세지 않음) | 스위치 OFF, 조도 300 0.5초 → AUTO 0.999초 | 꺼짐 (AUTO부터 셈) | 기대와 같음 (확인 2건: headlight=off, headlight=on) | PASS |
| TC-L5-01 | L5 > L4 | 동치(무효: 위) | 처음부터 시동 ON, 스위치 OFF, 조도 60001 | 켜짐 + 고장 표시 | 기대와 같음 (확인 2건: headlight=on, luxlamp=on) | PASS |
| TC-L5-02 | L5, C2 | 경계값(유효 상한) | 조도 60000 | 꺼짐(L4 OFF), 고장 표시 꺼짐 | 기대와 같음 (확인 2건: headlight=off, luxlamp=off) | PASS |
| TC-L5-03 | L5, C2 | 동치(무효: 아래) | 조도 -1 | 켜짐 + 고장 표시 | 기대와 같음 (확인 2건: headlight=on, luxlamp=on) | PASS |
| TC-L5-04 | L5, C2 | 경계값(시간 3-value) | 조도 30000 후 조도 송신만 끊고 0.499 / 0.500초 | 표시 꺼짐 → 켜짐 | 기대와 같음 (확인 3건: luxlamp=off, luxlamp=on, headlight=on) | PASS |
| TC-L5-05 | L5, L1 | 결정 테이블(표시는 시동 무관) | 이어서 시동 OFF | 전조등 꺼짐, 고장 표시 유지 | 기대와 같음 (확인 2건: headlight=off, luxlamp=on) | PASS |
| TC-L5-06 | L5, C2 | 상태 전이(복귀) | 시동 ON, 조도 송신 재개 | 고장 표시 꺼짐 | 기대와 같음 (확인 1건: luxlamp=off) | PASS |
| TC-L6-01 | L6, C1 | 경계값(시간 3-value) | 처음부터 시동 ON, AUTO, 낮, level 저속 9.999초 | 꺼짐 | 기대와 같음 (확인 1건: headlight=off) | PASS |
| TC-L6-02 | L6, C1 | 경계값(시간 3-value) | 이어서 +1ms (10.000초) | 켜짐 | 기대와 같음 (확인 1건: headlight=on) | PASS |
| TC-L6-03 | L6 | 상태 전이(작동하는 동안 유지) | 이어서 5초 | 켜짐 | 기대와 같음 (확인 1건: headlight=on) | PASS |
| TC-L6-04 | L6, L3, C1 | 상태 전이(해제) | level 정지 | 바로 꺼짐 (밝음 3초는 그동안 계속 셈) | 기대와 같음 (확인 1건: headlight=off) | PASS |
| TC-L6-05 | L6 | 동치(간헐도 작동) | level 간헐 10초 | 켜짐 | 기대와 같음 (확인 1건: headlight=on) | PASS |
| TC-L6-06 | L6 | 동치(AUTO가 아니면 L6 아님) | 처음부터 시동 ON, 스위치 OFF, level 고속 10초 | 꺼짐 (L4) | 기대와 같음 (확인 1건: headlight=off) | PASS |
| TC-L6-07 | L6, C1 | 결정 테이블(L4가 정하는 동안에도 셈) | 이어서 AUTO로 | 바로 켜짐 (10초는 이미 셈) | 기대와 같음 (확인 1건: headlight=on) | PASS |
| TC-L-C4-01 | L1, C4 | 경계값(시간 3-value) | 처음부터 시동 ON, 스위치 ON, 세 신호 끊고 0.499 / 0.500초 | 켜짐 → 꺼짐 (시동 꺼짐으로 봄) | 기대와 같음 (확인 3건: headlight=on, rxlost=yes, headlight=off) | PASS |
| TC-L-Q11-01 | (정의 없음) Q11 | Negative(조명 스위치 코드) | 처음부터 시동 ON, 스위치 ON(켜짐) → 코드 7, 조도 300·30000 각 5초 | 스위치 코드 정의 안 됨 판정. 동작은 관찰 | 기대와 같음 (확인 1건: lightvalid=no)<br>관찰 headlight=on (0.000s)<br>관찰 headlight=on (5.000s)<br>관찰 headlight=on (10.000s) | PASS + 관찰 |
| TC-L-Q11-02 | (정의 없음) Q11 | Negative(조명 스위치 코드) | 처음부터 시동 ON, 꺼진 상태에서 코드 -1, level 저속 10초 | 관찰 | 관찰 headlight=on (10.000s) | 관찰 |
| TC-L-Q9-01 | (정의 없음) Q9 | Negative(시동 코드) | 처음부터 스위치 ON, 시동 코드 2 | 관찰 | 기대와 같음 (확인 1건: ignvalid=no)<br>관찰 headlight=on (0.000s) | PASS + 관찰 |
| TC-L-IF-01 | (모듈 간 약속) L6 | Negative(와이퍼 단계 코드) | 처음부터 시동 ON, AUTO, level 코드 9 10초 | 관찰 (와이퍼 모듈이 내보낼 수 없는 값) | 관찰 headlight=on (10.000s) | 관찰 |

## 와이퍼 (W1~W5)

실행: `./build/con_wipers < tools/consoles/tc/wipers.txt` · 로그: `docs/test/logs/wipers.log`

| TC ID | 요구사항 | 기법 | 입력 | 기대 (사양서) | 실제 | 판정 |
|---|---|---|---|---|---|---|
| TC-W-C5-01 | C5 | 시작 상태 | 초기 상태 | 정지, 강우 고장 표시 꺼짐 | 기대와 같음 (확인 2건: wiper=stop, rainlamp=off) | PASS |
| TC-W1-01 | W1 > W4 | 결정 테이블 | 시동 OFF, 스위치 HIGH, 강우 80 | 정지 | 기대와 같음 (확인 1건: wiper=stop) | PASS |
| TC-W4-01 | W4 | 결정 테이블 | 시동 ON (스위치 HIGH) | 고속 | 기대와 같음 (확인 1건: wiper=high) | PASS |
| TC-W4-02 | W4 | 결정 테이블 | 스위치 LOW | 저속 | 기대와 같음 (확인 1건: wiper=low) | PASS |
| TC-W4-03 | W4 | 결정 테이블 | 스위치 OFF (강우 80) | 정지 | 기대와 같음 (확인 1건: wiper=stop) | PASS |
| TC-W4-04 | W4 > W5 | 결정 테이블 | 스위치 HIGH, 강우 101 (고장) | 고속, 강우 고장 표시 켜짐 | 기대와 같음 (확인 2건: wiper=high, rainlamp=on) | PASS |
| TC-W2-01 | W2 | 경계값(10%: 9/10/11) | 처음부터 시동 ON, AUTO, 속도 30, 강우 9 → 10 → 11 | 정지 → 간헐 → 간헐 | 기대와 같음 (확인 3건: wiper=stop, wiper=int, wiper=int) | PASS |
| TC-W2-02 | W2 | 경계값(40%: 39/40/41) | 강우 39 → 40 → 41 | 간헐 → 저속 → 저속 | 기대와 같음 (확인 3건: wiper=int, wiper=low, wiper=low) | PASS |
| TC-W2-03 | W2 | 경계값(70%: 69/70/71) | 강우 69 → 70 → 71 | 저속 → 고속 → 고속 | 기대와 같음 (확인 3건: wiper=low, wiper=high, wiper=high) | PASS |
| TC-W2-04 | W2, C2 | 경계값(유효 범위 0/100) | 강우 0 → 100 | 정지 → 고속 | 기대와 같음 (확인 2건: wiper=stop, wiper=high) | PASS |
| TC-W3-01 | W3 | 결정 테이블(구간별 한 단계 낮춤) | 속도 0, 강우 70 → 40 → 10 → 9 | 저속 → 간헐 → 정지 → 정지 | 기대와 같음 (확인 4건: wiper=low, wiper=int, wiper=stop, wiper=stop) | PASS |
| TC-W3-02 | W3, C3 | 경계값(속도 0/1) | 강우 50, 속도 1 → 0 | 저속 → 간헐 | 기대와 같음 (확인 2건: wiper=low, wiper=int) | PASS |
| TC-W3-03 | W3, C3 | 동치(무효 속도는 0이 아님) | 강우 50, 속도 -1 → 251 | 저속 (유효한 속도가 없어 W3 아님) | 기대와 같음 (확인 3건: speedvalid=no, wiper=low, wiper=low) | PASS |
| TC-W3-04 | W3, C3 | 경계값(시간 3-value) | 속도 0, 강우 50, 속도 신호만 끊고 0.499 / 0.500초 | 간헐 → 정지 (수신 중단 → W1) | 기대와 같음 (확인 3건: wiper=int, rxlost=yes, wiper=stop) | PASS |
| TC-W5-01 | W5 | 동치(무효: 위) | 처음부터 시동 ON, AUTO, 속도 30, 강우 101 | 간헐, 고장 표시 | 기대와 같음 (확인 2건: wiper=int, rainlamp=on) | PASS |
| TC-W5-02 | W5 > W3 | 결정 테이블 | 이어서 속도 0 | 간헐 유지 (낮추지 않음) | 기대와 같음 (확인 1건: wiper=int) | PASS |
| TC-W5-03 | W5, C2 | 동치(무효: 아래) | 강우 -1 | 간헐, 고장 표시 | 기대와 같음 (확인 2건: wiper=int, rainlamp=on) | PASS |
| TC-W5-04 | W5, C2 | 경계값(시간 3-value) | 강우 50, 강우 송신만 끊고 0.499 / 0.500초 | 표시 꺼짐 → 켜짐, 간헐 | 기대와 같음 (확인 3건: rainlamp=off, rainlamp=on, wiper=int) | PASS |
| TC-W5-05 | W5, W1 | 결정 테이블(표시는 시동 무관) | 이어서 시동 OFF | 정지, 고장 표시 유지 | 기대와 같음 (확인 2건: wiper=stop, rainlamp=on) | PASS |
| TC-W5-06 | W5 | 결정 테이블(표시는 스위치 무관) | 시동 ON, 스위치 OFF | 정지, 고장 표시 유지 | 기대와 같음 (확인 2건: wiper=stop, rainlamp=on) | PASS |
| TC-W-C4-01 | W1, C4 | 경계값(시간 3-value) | 처음부터 시동 ON, HIGH, 세 신호 끊고 0.499 / 0.500초 | 고속 → 정지 | 기대와 같음 (확인 2건: wiper=high, wiper=stop) | PASS |
| TC-W-SWR-01 | SWR-W4-005 (팀 요구사항) | Negative(스위치 코드) | 처음부터 시동 ON, AUTO, 속도 30, 강우 50 → 스위치 코드 7, -1 | 저속 유지 (마지막 정상 값 AUTO) | 기대와 같음 (확인 3건: wipervalid=no, wiper=low, wiper=low) | PASS |
| TC-W-SWR-02 | SWR-W4-005 (팀 요구사항) | Negative(스위치 코드) | 스위치 HIGH → 코드 99, 강우 0 | 고속 유지 (마지막 정상 값 HIGH) | 기대와 같음 (확인 1건: wiper=high) | PASS |
| TC-W-Q9-01 | (정의 없음) Q9 | Negative(시동 코드) | 처음부터 스위치 HIGH, 시동 코드 2 | 관찰 | 기대와 같음 (확인 1건: ignvalid=no)<br>관찰 wiper=high (0.000s) | PASS + 관찰 |

## 요구사항별 TC

요구사항 칸에 적힌 ID 기준입니다. 표에 없거나 TC가 적은 요구사항은 아직 덜 시험된 곳입니다.

| 요구사항 | TC 수 | TC |
|---|---|---|
| C1 | 17 | TC-ECU-L6-01, TC-ECU-L6-02, TC-ECU-L6-03, TC-ECU-L6-05, TC-ECU-L6-06, TC-ECU-L6-08, TC-L2-01, TC-L2-02, TC-L2-04, TC-L2-05, TC-L3-01, TC-L3-02, TC-L2-06, TC-L6-01, TC-L6-02, TC-L6-04, TC-L6-07 |
| C2 | 10 | TC-D5-05, TC-D5-06, TC-D5-07, TC-L5-02, TC-L5-03, TC-L5-04, TC-L5-06, TC-W2-04, TC-W5-03, TC-W5-04 |
| C3 | 15 | TC-ECU-D5-06, TC-ECU-D5-08, TC-ECU-C4-02, TC-D3-05, TC-D3-06, TC-D3-07, TC-D4-04, TC-D4-05, TC-D4-06, TC-D4-09, TC-D4-10, TC-D4-11, TC-W3-02, TC-W3-03, TC-W3-04 |
| C4 | 9 | TC-ECU-C4-01, TC-ECU-C4-02, TC-ECU-C4-03, TC-ECU-C4-04, TC-D1-01, TC-D1-02, TC-D1-03, TC-L-C4-01, TC-W-C4-01 |
| C5 | 5 | TC-ECU-C5-01, TC-D-C5-01, TC-E-C5-01, TC-L-C5-01, TC-W-C5-01 |
| D1 | 11 | TC-ECU-C4-02, TC-ECU-C4-03, TC-D1-01, TC-D1-02, TC-D1-03, TC-D1-04, TC-D1-05, TC-D1-06, TC-D1-07, TC-D1-08, TC-D1-09 |
| D2 | 7 | TC-D2-01, TC-D2-02, TC-D2-03, TC-D1-04, TC-D1-05, TC-D2-04, TC-D2-05 |
| D3 | 14 | TC-ECU-C4-01, TC-ECU-C4-04, TC-D3-01, TC-D3-02, TC-D3-03, TC-D3-04, TC-D3-05, TC-D3-06, TC-D3-07, TC-D3-08, TC-D3-09, TC-D2-03, TC-D1-07, TC-D3-10 |
| D4 | 18 | TC-ECU-D5-02, TC-ECU-D5-06, TC-ECU-D5-07, TC-ECU-D5-08, TC-ECU-C4-01, TC-ECU-C4-02, TC-ECU-C4-04, TC-D4-01, TC-D4-02, TC-D4-03, TC-D4-04, TC-D4-05, TC-D4-06, TC-D4-07, TC-D4-08, TC-D4-09, TC-D4-10, TC-D4-11 |
| D5 | 16 | TC-ECU-D5-01, TC-ECU-D5-02, TC-ECU-D5-03, TC-ECU-D5-04, TC-ECU-D5-05, TC-ECU-D5-06, TC-ECU-D5-07, TC-ECU-D5-08, TC-D5-01, TC-D5-02, TC-D5-03, TC-D5-04, TC-D5-05, TC-D5-06, TC-D5-07, TC-D5-08 |
| L1 | 5 | TC-ECU-C4-02, TC-ECU-C4-03, TC-L1-01, TC-L5-05, TC-L-C4-01 |
| L2 | 6 | TC-L2-01, TC-L2-02, TC-L2-03, TC-L2-04, TC-L2-05, TC-L2-06 |
| L3 | 6 | TC-ECU-L6-04, TC-L3-01, TC-L3-02, TC-L3-03, TC-L3-04, TC-L6-04 |
| L4 | 8 | TC-ECU-D5-01, TC-ECU-D5-04, TC-ECU-C4-01, TC-ECU-C4-04, TC-L1-01, TC-L4-01, TC-L4-02, TC-L5-01 |
| L5 | 8 | TC-ECU-D5-01, TC-ECU-D5-04, TC-L5-01, TC-L5-02, TC-L5-03, TC-L5-04, TC-L5-05, TC-L5-06 |
| L6 | 16 | TC-ECU-L6-01, TC-ECU-L6-02, TC-ECU-L6-03, TC-ECU-L6-04, TC-ECU-L6-05, TC-ECU-L6-06, TC-ECU-L6-07, TC-ECU-L6-08, TC-L6-01, TC-L6-02, TC-L6-03, TC-L6-04, TC-L6-05, TC-L6-06, TC-L6-07, TC-L-IF-01 |
| Q8 | 4 | TC-D-Q8-01, TC-D-Q8-02, TC-D-Q8-03, TC-E-Q8-01 |
| Q9 | 4 | TC-D-Q9-01, TC-D-Q9-02, TC-L-Q9-01, TC-W-Q9-01 |
| Q10 | 2 | TC-E-Q10-01, TC-E-Q10-02 |
| Q11 | 2 | TC-L-Q11-01, TC-L-Q11-02 |
| SWR-W4-005 | 2 | TC-W-SWR-01, TC-W-SWR-02 |
| W1 | 5 | TC-ECU-C4-02, TC-ECU-C4-03, TC-W1-01, TC-W5-05, TC-W-C4-01 |
| W2 | 6 | TC-ECU-L6-07, TC-ECU-L6-08, TC-W2-01, TC-W2-02, TC-W2-03, TC-W2-04 |
| W3 | 7 | TC-ECU-L6-07, TC-ECU-L6-08, TC-W3-01, TC-W3-02, TC-W3-03, TC-W3-04, TC-W5-02 |
| W4 | 9 | TC-ECU-L6-01, TC-ECU-D5-01, TC-ECU-C4-01, TC-ECU-C4-04, TC-W1-01, TC-W4-01, TC-W4-02, TC-W4-03, TC-W4-04 |
| W5 | 9 | TC-ECU-D5-01, TC-ECU-D5-05, TC-W4-04, TC-W5-01, TC-W5-02, TC-W5-03, TC-W5-04, TC-W5-05, TC-W5-06 |

