# 차량 통신 개념 참조 (VBC 팀 프로젝트용)

> 출처: 교육자료 PDF 6종 (알테스 김성창). 표기 `[11 p22]` = 파일번호 + PDF 페이지(폼피드 기준, 슬라이드 번호보다 +1).
> 11=도메인별 요구사항·오류검출, 12=CAN 원리·에러처리, 13=CAN 클럭동기화·통신서비스, 14=CAN FD/XL, 15=LIN·SENT, 16=PEAK-CAN 실습.
> 본 프로젝트(VBC)는 실제 CAN/하드웨어가 없으므로 이 문서는 **개념 근거**용이다. 6장의 DBC 정의는 **제안/참고**이며 사양이 아니다.

---

## 1. 차량 네트워크 개요와 도메인별 요구사항

### 1.1 왜 네트워크인가
- 센서·ECU마다 점대점(I/O, PWM, ADC) 배선을 하면 원가, 생산시간, 신뢰성 문제가 커진다. 배선은 엔진/섀시 다음으로 비싸고 무겁다 [11 p4, 12 p25].
- CAN: Bosch 개발, 1986 SAE 공개, 1991 ISO 표준, 1991 벤츠 W140에 최초 적용(ECU 5개) [11 p5-7].
- 전통 E/E 구조: Central Gateway를 중심으로 파워트레인/섀시/바디/멀티미디어/ADAS 도메인별 망. 바디 도메인은 LIN 등 로컬망을 붙이는 구성이 흔하다 [11 p15].
- 최근 흐름(SDV): 도메인 DCU -> Zonal + Vehicle Computer, 백본은 Ethernet, 서비스 지향(SOME/IP) [11 p24-47]. VBC 범위 밖이므로 참고만.

### 1.2 도메인별 네트워크 요구사항 [11 p22]

| 도메인 | 설명 | End-to-End 지연 | 대역폭 | 메시지 주기 |
|---|---|---|---|---|
| Powertrain | 동력/구동 제어 | < 10 us | 낮음 | 2~100 ms |
| Chassis | 스티어링/브레이크/서스펜션 | < 10 us | 낮음 | 1~100 ms |
| **Body & Comfort** | 라디오, A/C, 창문, 시트, **전조등** | **< 10 ms** | 낮음 | **10~500 ms** |
| ADAS & Safety | 주행안전 | < 250 us ~ 1 ms/camera | 20~100 Mbps | 1~500 ms |
| HMI(Multimedia) | 운전자/동승자 정보 | < 10 ms | 시스템별 | 10~500 ms |

### 1.3 바디 도메인 특성 [11 p18]
- 와이퍼, 레인센서, 실내등, 전조등, 혼, 리모컨키, 히터/A/C, 시트, 윈도우 등. 편의/감성 품질 목적.
- 타 도메인보다 실시간성 요구가 낮고, **저비용**(적은 배선, 저가 칩), **Sleep/Wakeup 저전력** 필요.
- B-CAN(LS-CAN, 최대 125 kbps, 한 가닥 단선 시에도 통신하는 Fault Tolerant)에 BCM 등이 연결됨 [12 p98, p103].
- LIN 대표 예: 도어별 윈도우 모듈 4개를 메인 제어기가 LIN으로 구동 [15 p10]. 도어 열림 감시도 LIN 예시로 등장 [15 p25].

### 1.4 통신 방식 비교 [11 p89-90, 15 p7]

| 항목 | LIN | SENT | CAN(HS) | CAN FD | FlexRay | Ethernet |
|---|---|---|---|---|---|---|
| 속도 | ~20 kbps | 센서 1:1(타이머 기반) | 최대 1 Mbps(차량은 500 kbps) | 최대 8 Mbps(자료에 2/5/12 Mbps 표기도 있음) | 10 Mbps | 100M/1G/10G |
| 데이터 길이 | 0~8 B | 최대 6 nibble | 0~8 B | 0~64 B | 최대 254 B | 최대 1518 B |
| 배선 | 1선 | 1선+전원 | UTP 2선 | UTP 2선 | 2채널 | UTP(T1) |
| 매체 접근 | 단일 마스터 폴링 | 송신 전용 | CSMA/CR(ID 우선순위) | CSMA/CR | TDMA(시간 결정적) | 스위치 풀듀플렉스 |
| 오류 검출 | Checksum+패리티 | 4bit CRC | CRC-15 | CRC-17/21 | CRC-24 | CRC-32 |
| 결정적 지연 | 스케줄 테이블 | - | 아니오 | 아니오 | 예 | TSN으로 가능 |
| 트랜시버 가격 | 0.99 $ | 불필요 | 0.65~1.24 $ | 0.65 $ | 1.62 $ | 6.37 $ |
| 주 용도 | 도어/시트/윈도우 등 로컬 | 압력/위치 센서 | 파워트레인/섀시/바디 | 고속화 CAN | X-by-Wire | 멀티미디어/ADAS/백본 |

- 가격은 자료 기준(수량 조건 상이)이므로 상대 비교로만 사용. 표는 자료의 표가 PDF 추출 중 열이 섞여 있어 본문 서술과 대조해 재구성함.

---

## 2. CAN 핵심

### 2.1 특성 [11 p49-52, 12]
- 멀티마스터, **이벤트 트리거**: 어느 노드든 원할 때 송신. 주기 메시지라도 노드 간 송신 시점을 맞추는 것은 불가능(클럭 오차, SW 실행 편차) [11 p51].
- 충돌 시 ID 우선순위 중재, **ID가 낮을수록 우선순위 높음**. 밀린 프레임은 가중치 없이 다음 idle 후 재경쟁 -> 낮은 우선순위 메시지는 지연이 예측 불가, 이론상 무한 지연 가능 [11 p52, 12 p60].
- 버스 사용률은 60~70% 이하 권장 [11 p52]. 실습에서 버스부하 100%일 때 ID 0x7FF는 전송되지 못하고 0x150만 통과함(starvation) [16 p121-125].
- 물리: CAN_H/CAN_L 차동 신호, 열성(1)=차 0V / 우성(0)=약 2V, **우성이 열성을 이긴다**, 양 끝 120 ohm 종단 [12 p34-35, p43].
- 속도와 버스 길이: 1 Mbps 40 m, 500 kbps 130 m, 250 kbps 270 m, 125 kbps 530 m [13 p17].
- 계층: OSI 중 물리(ISO 11898-2)+데이터링크(11898-1)만 사용. 8 B 초과 전송은 상위 CAN TP(진단 등)가 분할 [12 p20, 13 p75].

### 2.2 표준 프레임(CAN 2.0A, 11bit ID) [12 p31, p46-47, p80-91]

| 필드 | 크기(bit) | 내용 |
|---|---|---|
| SOF | 1 | 우성 0, 하드 동기화 |
| Arbitration | 11 + RTR | ID(중재용), RTR 0=데이터/1=리모트 |
| Control | 6 | IDE, 예약비트, DLC 4bit(0~8) |
| Data | 0~8 B | MSB 먼저 전송 |
| CRC | 15 + delimiter 1 | 다항식 x15+x14+x10+x8+x7+x4+x3+1, SOF~Data 범위 |
| ACK | slot 1 + delimiter 1 | 수신 성공 노드가 우성으로 덮어씀 |
| EOF | 7 | 열성 7개, 이후 IMS(Intermission) 3bit |

- 프레임 종류: Data / Remote / Error / Overload [12 p79].
- CAN 2.0B: 29bit 확장 ID(IDE=1, SRR=1). 같은 11bit ID면 표준 프레임이 우선. 승용은 11bit, 트럭/버스는 J1939(29bit) [12 p123-124].
- 최대 프레임 130 bit (stuff 19 포함) -> 500 kbps에서 260 us, 유효 데이터율 약 30.8 kB/s [12 p73].

### 2.3 중재 (CSMA/CR) [12 p48-53]
- 동시에 SOF를 보낸 노드들이 ID를 MSB부터 한 비트씩 송출하며 버스를 되읽는다. 내가 1을 보냈는데 버스가 0이면 중재 패배, 즉시 수신 모드 전환. 데이터는 깨지지 않음.
- 예: A=011, B=010 -> 3번째 비트에서 A 탈락, B 계속 송신.

### 2.4 비트 스터핑과 동기화 [12 p67-72, 13 p36-53]
- SOF~CRC 시퀀스 구간에서 같은 값이 5비트 연속이면 반대 비트 1개 삽입(수신 측 제거). 목적: 엣지를 강제해 클럭 재동기화.
- CRC delimiter/ACK/EOF는 스터핑 제외. Error/Overload Flag는 의도적으로 6비트 연속(스터핑 규칙 위반)을 사용.
- 비동기 통신이라 노드마다 크리스털 오차(ppm 단위)가 있어 동기화 필요. 하드 동기화(SOF 엣지에서 비트 타이머 재시작) + 재동기화(이후 엣지에서 PHASE_SEG1/2를 SJW 한도 내 조정).
- 비트 타임 = SYNC_SEG(1TQ) + PROP_SEG + PHASE_SEG1 + PHASE_SEG2, **샘플링 포인트는 PHASE_SEG1 끝**. 예: fCAN 100 MHz, BRP=9 -> TQ 0.1 us, TSEG1=14, TSEG2=3 -> 20TQ = 2 us(500 kbps), 샘플 포인트 80% [13 p19-31].
- 전파지연 제약: 한 비트 안에 최원단 노드 간 중재/ACK가 성립해야 함. 케이블 약 5 ns/m + 트랜시버 약 100 ns [13 p14-16, p54-57].

### 2.5 오류 검출 5종 [12 p106-111]

| 검출 | 방식 | 누가 |
|---|---|---|
| Bit monitoring | 보낸 비트와 되읽은 비트 비교 | 송신 노드 |
| Stuff error | 같은 값 6비트 연속 | 수신 |
| Form error | 고정 형식 필드(delimiter, EOF 등) 위반 | 수신 |
| CRC error | 계산 CRC != 수신 CRC | 수신 |
| ACK error | ACK slot에 우성이 없음(수신 노드 없음 포함) | 송신 |

- 오류 검출 시 Error Frame 송출 -> 진행 중 프레임 폐기 -> **자동 재전송** [12 p106, p59].
- Error Flag: Active는 우성 6비트(전 노드가 오류를 인지), Passive는 열성 6비트(버스 영향 없음). 길이 6~12bit + Delimiter 8 열성. CRC 오류는 ACK delimiter 다음 비트에, 나머지는 즉시 다음 비트에 송출 [12 p93, p116-118].
- Overload Frame: 다음 프레임 지연용, 에러 카운터 증가 없음 [12 p94-95].

### 2.6 에러 상태 (Fault Confinement) [12 p113-121]
- TEC(송신)/REC(수신) 카운터: 송신 오류 +8, 성공 -1. 수신 오류 +1(첫 오류 +8), 성공 -1.
- 상태 천이: **Error Active** -> (TEC>127 또는 REC>127) **Error Passive** -> (TEC>255) **Bus Off**. Bus Off에서는 버스 배제로 오류 전파를 막고, 11비트 열성 x 256회 관측 또는 리셋 시 복귀.
- Error Passive 송신 노드는 프레임 후 8비트 추가 대기(Suspend Transmission).
- 수신 노드가 하나도 없으면 ACK error -> 에러 프레임 재전송 반복 -> 카운터 상승 -> 버스 배제 [12 p59].

### 2.7 CRC의 한계 [11 p134, 12 p74-77]
- 설계 목표 Hamming distance 6이지만, 스터프 비트와 결합한 2비트 오류(비트 시프트)가 있어 실효 HD=2. 즉 100% 보장은 1비트 오류뿐.
- **안전 관련 신호는 별도 Checksum 또는 카운트 값을 데이터에 추가해 무결성 검증 후 사용**해야 한다고 명시 [12 p77]. (6장 alive counter/E2E 근거)

---

## 3. CAN FD / LIN / SENT 요점

### 3.1 CAN FD [11 p63-64, 14]
- CAN의 500 kbps/8 B 한계를 해결. Bosch 2012 제안, 2015 ISO. 기존 CAN 방식을 유지하며 속도와 길이만 확장(FlexRay는 전환 부담 때문에 널리 못 퍼짐).
- 중재 구간은 기존 속도, **데이터 구간만 고속**(BRS=1일 때 전환). 데이터 0~64 B(0~8, 12, 16, 20, 24, 32, 48, 64).
- 새 제어비트: EDL(=1이면 FD), BRS(속도 전환), ESI(송신 노드가 Error Passive면 1).
- CRC: 16 B 이하 CRC-17, 17~64 B CRC-21. **스터프 비트를 CRC 계산에 포함**하고 stuff bit count(3bit)를 추가해 비트열 길이 변화 오류를 막음 [14 p29-41].
- 고속 구간에서 트랜시버 루프 지연이 샘플 포인트를 넘으므로 2.5 Mbps 초과 시 TDC(지연 보상) 필요 [14 p47-50].
- 중재(ID 우선순위) 지연 문제는 그대로다 [11 p11].
- CAN XL(참고): 최대 20 Mbps/2048 B, CAN FD와 한 버스 공존, SOME/IP·Ethernet 터널링, CANsec 보안 [14 p65-72].

### 3.2 LIN [11 p66-68, 15]
- 1선, 최대 20 kbps, 단일 마스터/다수 슬레이브. 마스터가 **Header(Break+Sync 0x55+PID)** 를 보내면 해당 슬레이브가 Response(0~8 B + Checksum)를 보냄. 충돌 자체가 없음 [15 p12-20].
- 버스 전압 60% 이상=1(열성), 40% 이하=0(우성) [15 p6].
- PID = ID 6bit(0~59 신호용, 60/61 진단) + 패리티 2bit. Checksum은 데이터(2.x는 PID 포함) 합의 1의 보수, 수신 시 합산해 0xFF면 정상 [15 p18-21].
- 프레임 종류: Unconditional(스케줄 테이블 주기 전송), Event-triggered(발생 시만 응답, 충돌 시 마스터가 해소 스케줄 실행. 도어 열림 감시 예), Sporadic(마스터만, 갱신된 프레임만), Diagnostic [15 p22-27].
- 스케줄 테이블: Frame slot = Tbase(5 또는 10 ms) x n [15 p33].
- 상태관리: 슬레이브는 unconditional 프레임에 **response_error 비트**를 실어 송수신 오류를 보고, 마스터가 이를 모아 고장 노드 판단 및 Limp home 진입 [15 p39-40].
- NM: 150 us 이상 0으로 Wakeup, 마스터 Go-to-Sleep 명령 후 4~10 s 내 버스 비활성 [15 p34-36].

### 3.3 SENT [11 p70-71, 15 p42-51]
- SAE J2716, 센서->ECU 1:1 단방향, 트랜시버 불필요, 아날로그/PWM 대체.
- 프레임: Sync/Cal 56 tick + Status nibble + 데이터 nibble 최대 6 + CRC nibble. nibble 값은 falling-to-falling 12~27 tick = 0~15. 1 tick = 3~90 us.
- CRC: x4+x3+x2+1, seed 0101, Status nibble 제외. 1비트 오류 전부 검출. Slow channel로 진단값을 직렬 전송.

---

## 4. 주기 메시지, 타임아웃, 신호 유효성, fail-safe

### 4.1 PDF가 말하는 것

| 개념 | 내용 | 출처 |
|---|---|---|
| 정적 신호 정의 | CAN DB(DBC/Fibex)에 ID, 신호, **통신 주기, Time-out 시간, Time-out 시 대체 값**을 OEM이 사전 정의 | [11 p29], [13 p62] |
| 주기 송신 | 대부분의 CAN 프레임은 주기 송신. 주기가 짧을수록 높은 우선순위(낮은 ID) | [12 p62] |
| Deadline | 실시간 deadline = 통신 주기(예: 100 ms 주기면 100 ms 내 1회 전송 가능해야 함) | [12 p62] |
| 부하 예 | 주기 1 ms 평균 14.3%, 10 ms 1.5%, 100 ms 0.2% 버스 부하 | [12 p62] |
| 송신 deadline 감시 | 송신 요청 후 지정 시간 내 Confirmation이 없으면 콜백(`Com_CbkTxTOut`)으로 오류 통지 | [13 p69] |
| 수신 deadline 감시 | 주기 메시지가 지정 시간 내 수신되는지 감시(AUTOSAR COM Rx deadline monitoring) | [13 p71] |
| 송신 모드 | Direct / Direct+min delay / Periodic / Mixed (AUTOSAR COM) | [13 p65-68] |
| COM 주기 처리 | `Com_MainFunctionRx/Tx`를 주기(예: 5 ms) 호출해 신호 처리 | [13 p61] |
| 지연 원인 | 특정 메시지가 지연 중인지 송신 ECU 고장인지 구분하기 어려움 -> 수신측 감시 필요 | [11 p52] |
| 무결성 | 안전 신호는 Checksum 또는 카운트 값을 추가해 검증 후 사용 | [12 p77] |
| 오류 검출 계열 | Parity(짝수 비트 오류 못 잡음) < Checksum(순서 변경/0 추가 못 잡음) < CRC(HW 구현 용이, 고정 길이에 강함) | [11 p111-124] |
| 센서 값 유효성 | UDS 센서 읽기 설계 시 "센서 ID, 크기, 변환식, **정상 값 범위(이외는 고장 표시)**" 정의 | [16 p101] |
| 센서 범위 이탈 | ToF 센서는 신호강도 0이면 측정 범위 이탈(값은 무의미/랜덤) -> 값을 쓰지 말고 out-of-range 처리 | [16 p112-113, p115] |
| 슬레이브 오류 보고 | LIN response_error 비트, 마스터가 상태 판단 | [15 p40] |
| 저전력 | AUTOSAR NM: 모든 노드가 주기 NM 메시지를 멈추면 Sleep | [13 p129] |

- 자료에 **Alive counter, E2E Profile(AUTOSAR E2E)이라는 용어는 없다**. "카운트 값 추가"(12 p77), Checksum 개념(11 p113-122, LIN 15 p21)까지만 근거가 있다.
- fail-safe라는 용어도 직접 쓰이지 않는다. 유사 개념: Time-out 시 **대체 값**(11 p29), LIN **Limp home**(15 p34, p39), CAN **Bus Off**로 고장 노드 격리(12 p113).

### 4.2 신호 유효성 3단계 (자료 개념을 종합한 정리, 해석)
1. **도착성**: 정해진 시간 안에 새 프레임이 왔는가 (deadline/timeout).
2. **무결성**: 비트 오류/위조가 없는가 (CRC, Checksum, 카운터). CAN 하드웨어가 CRC까지 처리하므로 앱은 주로 카운터/체크섬을 추가 검증.
3. **범위/타당성**: 값이 DBC [min|max] 안인가, 센서 자체가 유효 표시인가 (range check, 센서 고장 표시).
- 하나라도 실패하면 해당 신호를 **무효**로 두고 정의된 대체 동작(안전 측 기본값)을 수행.

---

## 5. 실습(PEAK-CAN) 요약 [16]

| 항목 | 값 |
|---|---|
| 보드/MCU | Infineon TC375 Lite Kit V2 / TC375TP (TriCore) + Easy Module Shield |
| 개발 도구 | AURIX Development Studio(Eclipse), TASKING VX-Tools 컴파일러, UDE(PLS) 디버거(DAP/JTAG), Putty 시리얼 |
| CAN 도구 | PEAK PCAN-USB(FD) + PCAN-View, 실습용 NGV_CAN.exe |
| 트랜시버 | MCP2542 (보드 R39 저항 납땜 확인 필수), CAN_H 쪽 점퍼선 검은 끝 |
| 속도 설정 | 500 kbit/s (Can_Init의 BD_500K / BD_1M), CAN FD는 `CanFd_Init(arb속도, data속도, 노드)` |
| API | `Can_SendMsg(id, data, len)`, `Can_RecvMsg(&id, data, &len)`(수신 ISR), `CanFd_SendMsg/RecvMsg`, `GPIO_SetLed` |
| 필터 | `Can_SetFilterMask(id, mask)`: (수신ID XOR AcceptID) AND Mask == 0이면 수신. 0x2xx만: (0x200, 0x700), 0x150만: (0x150, 0x7FF), 0~0x1FF: (0x0, 0x600) |
| 송신 예 | ID 0x100 "Hello!!!"를 500 ms 또는 100 ms마다 `delay_ms` 루프로 주기 송신 |
| DBC 실습 | ID 0x301, DLC 4, 주기 200 ms. `SG_LightSen : 0|12@1+ (0.00081,0)`, `SG_VoltSen : 16|12@1+ (0.00081,0) "V"`. 12bit ADC(0~4095 = 0~3.3 V) |
| 변환식 | physical = offset + scale x raw. 예: 621 rpm / 0.125 = 4968 = 0x1368 |
| ToF 센서 | ID 0x200, 100 ms 주기, 앞 3 B가 거리(mm), 5~6번째 B 신호강도 0이면 범위 이탈 |
| 진단 | UDS ReadDataByIdentifier(0x22), 요청 ID 0x7E0 / 응답 0x7E8, 응답 SID+0x40, 부정 응답 0x7F |
| 기타 실습 | Bus Load 100% starvation, CAN FD 64 B 전송(DLC 12=12 B 등), CAN-CAN FD 게이트웨이 |

- 이 중 VBC에 직접 쓸 것은 "물리값 = offset + scale x raw", "주기 송신 루프", "범위 이탈이면 값 사용 금지" 세 가지다.

---

## 6. VBC 적용 (제안/참고)

### 6.1 규칙-개념 매핑

| VBC 규칙/요소 | 대응 통신 개념 | 근거 |
|---|---|---|
| C2/C3/C4 (speed/gear/ignition 0.5 s 미수신 -> 수신 손실, ignition OFF 취급) | **수신 deadline(timeout) 감시** + timeout 시 대체값(ignition OFF) | [13 p71], [11 p29] |
| lux/rain 0.5 s 미수신 -> 센서 고장 | 수신 timeout + 센서 고장 표시 | [13 p71], [16 p101] |
| lux/rain 등 범위 이탈 -> 센서 고장 | 신호 **유효성(range check)** = DBC [min\|max] | [11 p29], [16 p101, p113] |
| 콘솔이 50 ms마다 패널값 재송신 | **주기 송신(Periodic Tx)**, 값이 불변이어도 송신 | [12 p62], [13 p67] |
| "0.5 s보다 자주 재송신" | 송신 주기 < timeout (deadline >= 주기) | [12 p62] |
| 수신 손실 시 ignition OFF 취급 | 안전 측 대체 값 / fail-safe 기본 상태 | [11 p29] |
| BCM(= VBC 본체) | 바디 도메인 ECU. 입력 수신 -> 판단 -> 램프/와이퍼/도어락/경고 출력 | [12 p103], [11 p18] |
| 콘솔 + 센서 시뮬레이터 | 송신 노드(다른 ECU/센서 노드 역할) | [11 p29] |
| 입력 갱신 여부 추적 | 수신 시각(타임스탬프) 저장, 주기 태스크에서 경과 비교 | 4.1 |

### 6.2 왜 0.5 s인가 (해석)
- 송신 50 ms, timeout 500 ms이면 **연속 10주기 누락**부터 손실로 판정한다 (500 / 50 = 10).
- 단발성 지연(CAN 중재 지연, 태스크 지터, 콘솔 스톨 한두 번)은 흡수하고, 진짜 끊김은 0.5 s 내에 잡는 균형값이다. 바디 도메인 요구 주기(10~500 ms) 상한과도 맞는다 [11 p22].
- 판정식 예: `now - last_rx_time >= 500 ms` 이면 손실. 경계(정확히 500 ms)는 사양의 "0.5 s 동안 수신 없음"을 따라 팀이 한 곳에서 정의하고 테스트로 고정할 것.
- 제안: 50 ms 주기와 500 ms timeout을 상수로 분리(`CYCLE_MS`, `TIMEOUT_MS`)하고 비율(10)을 코드에 하드코딩하지 않는다.
- 주의: "주기보다 자주 재송신"은 사양 요구다. 재송신이 멈춘 입력(값 불변)도 timeout 대상이라는 점을 테스트에 반드시 포함(값이 같아도 수신 이벤트로 취급, 시각 갱신).

### 6.3 BCM 관점 구조 제안
1. **수신 계층**: 입력 신호별 `{value, last_rx_ms, valid}` 보관. 수신 시 값과 시각을 갱신.
2. **감시 계층**(주기 태스크): timeout 검사 -> `rx_lost` 플래그. 범위 검사 -> `sensor_fault` 플래그.
3. **판단 계층**: L/W/D/C 규칙은 원본 값이 아니라 **유효성 처리된 값**(대체값 적용 후)만 사용. 예: speed/gear/ignition 손실 -> ignition OFF 취급.
4. **출력 계층**: 램프/와이퍼/도어락/대시보드 경고.
- 이렇게 나누면 규칙 테스트에서 "입력 지연 주입", "범위 이탈 주입"을 쉽게 재현할 수 있다.

### 6.4 입력 신호 DBC풍 정의 (제안/참고, 사양 아님)
> 범위는 프로젝트 사양에서 가져온 값이고, bit 길이/scale/Msg 구성/ID는 설명용 가정이다. 단위와 범위는 실제 사양서와 대조해 확정할 것.

| 신호명(제안) | 단위 | 범위 | raw bit | scale/offset | 주기 | Timeout | Timeout/범위 이탈 시 처리(제안) |
|---|---|---|---|---|---|---|---|
| VehicleSpeed | km/h | 0 ~ 250 | 8 | 1 / 0 | 50 ms | 500 ms | 수신 손실(C 규칙). 범위 이탈 처리는 사양에 따름 |
| GearPosition | enum | 사양 정의값 | 4 | 1 / 0 | 50 ms | 500 ms | 수신 손실, 정의 외 값은 무효 |
| IgnitionState | enum | 사양 정의값 | 2~4 | 1 / 0 | 50 ms | 500 ms | **OFF 취급** |
| Lux (조도) | lux | 0 ~ 60000 | 16 | 1 / 0 | 50 ms | 500 ms | 센서 고장 |
| RainAmount | % | 0 ~ 100 | 8 | 1 / 0 | 50 ms | 500 ms | 센서 고장 |
| DoorOpen (FL/FR/RL/RR) | bool | 0/1 | 1 x 4 | 1 / 0 | 50 ms | 사양 확인 | 사양 확인 |
| 스위치류(헤드램프/와이퍼 등) | enum | 사양 정의값 | 2~4 | 1 / 0 | 50 ms | 사양 확인 | 사양 확인 |

- 도어/스위치의 timeout 적용 여부는 이 자료에 근거가 없다. 사양서(C 규칙)를 따른다.
- DBC 문법 예시(자료 형식 [13 p62, 16 p90] 차용, 가상 ID):

```
BO_ 256 PanelInput : 8 Console
 SG_ VehicleSpeed : 0|8@1+  (1,0) [0|250]   "km/h" BCM
 SG_ Gear         : 8|4@1+  (1,0) [0|15]    ""     BCM
 SG_ Ignition     : 12|2@1+ (1,0) [0|3]     ""     BCM
 SG_ Lux          : 16|16@1+ (1,0) [0|60000] "lux" BCM
 SG_ Rain         : 32|8@1+ (1,0) [0|100]   "%"    BCM
```

(형식: `start|len@endian sign (scale,offset) [min|max] "unit" receiver`)

### 6.5 안전 관련 보강 (선택, 해석)
- 실제 차량이라면 12 p77에 따라 안전 신호에 카운터/체크섬을 붙인다. 가상 구현에서는 필수 아님. 필요하면 "alive counter가 N주기 정지 -> 입력 정체(stuck)로 판정" 확장 테스트를 따로 고려.
- Bus Off 개념(오류 누적 시 노드 격리)은 VBC에서 "입력이 계속 무효이면 해당 기능을 안전 상태로 유지"하는 정책에 대응시킬 수 있다.

### 6.6 팀 체크리스트
- [ ] 입력별 `last_rx`와 `valid` 분리, timeout 경계 조건 테스트
- [ ] 값 불변 재송신이 timeout을 리셋하는지 테스트
- [ ] 범위 경계값(0, 250, 60000, 100 및 -1/251/60001/101) 테스트
- [ ] speed/gear/ignition 개별 및 동시 손실, lux/rain 개별 고장 조합 테스트
- [ ] 손실 후 수신 재개 시 정상 상태로 복원되는지 확인
- [ ] 50 ms / 500 ms를 상수로 분리
