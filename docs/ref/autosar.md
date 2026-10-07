# AUTOSAR Classic 요약과 VBC 매핑 (참고 문서)

> 출처: 교육자료 `01.pdf` (약 1000쪽, mobilgene studio 기반 Classic AUTOSAR 교육). 본문의 `(p.N)`은 PDF 쪽 번호.
> 표기: **[PDF]** = 자료에 있는 내용, **[일반]** = 자료에 없어 AUTOSAR 일반 지식으로 보충한 내용. 자료에는 Dcm/Dem/BswM 상세와 COM 타임아웃 설정값이 거의 없다(p.626, p.653, p.795에서 이름만 언급).
> VBC는 **AUTOSAR가 아니다**. 이 문서는 "구조 어휘"와 "양산에서는 어떻게 하는가"를 대응시키는 용도다.

---

## 1. 개요 · 목적 · 계층

### 1.1 AUTOSAR란 (p.8-12)
- AUTomotive Open System ARchitecture. 모토는 "표준은 협력, 구현은 경쟁". 목표는 SW/HW 분리, 재사용, 비용 효율, 차량 수명 중 SW 교체/업데이트 (p.8, p.23-25).
- 파트너 4단계: Core / Premium / Development / Associate (p.9). 버전: 2.1(2006) → 3.0(2008, ISO 26262 대응) → 4.0(2010) → 4.2/4.3/4.4 → R20-11~R22-11 (p.10).
- Classic vs Adaptive (p.12):

| 항목 | Classic | Adaptive |
|---|---|---|
| 대상 | MCU(자원 제한) | MPU(고성능) |
| OS | OSEK/VDX 기반 RTOS | POSIX(Linux 등) |
| 언어 | C | C++ |
| 통신/구성 | 정적 구성, 신호 기반 | 동적, 서비스 기반 |
| RTE | 있음 | 없음(서비스 호출) |
| 분야 | 섀시/파워트레인/바디/진단 | IVI, 자율주행, V2X |

VBC(램프/와이퍼/도어락)는 전형적인 **Classic, 바디 도메인** 기능이다.

### 1.2 계층 구조 (p.17-22)
```
 Application Layer   : SWC들 (HW 독립)                  <- VBC의 lighting/wiper/door 모듈
 ---------- RTE ----------  (SWC<->SWC, SWC<->BSW 통신 서비스, 도구가 생성)
 Services Layer      : OS, ComM, BswM, EcuM, WdgM, Dcm/Dem, NvM, COM/PduR ...
 ECU Abstraction     : CanIf, IoHwAb, MemIf/Fee/Ea, 외부 드라이버(외부 WDG/EEPROM)
 MCAL                : Can, Port, Dio, Adc, Pwm, Icu, Fls, Mcu ... (MCU 내장 장치)
 Complex Drivers     : 표준에 없는 기능용
 ---------- MCU HW
```
- ASW는 **RTE가 정의한 표준 인터페이스만** 사용 (p.20). 설정으로 생성되는 API 예: `Rte_Read_<port>_<elem>`, `Rte_Call_<port>_<op>`. BSW 표준 API 예: `Com_SendSignal(id,data)` (p.21).
- 표준 영역(통신/메모리/OS/진단)과 도메인 특화 영역(램프 구동, 모터 구동 드라이버)을 구분한다 (p.18).
- 개발 5단계 (p.15-16): Configure System → Implement Component → Extract ECU-specific info → Configure ECU → Generate Executable. Top-down이며 VFB 위에서 SWC를 설계한 뒤 ECU에 매핑한다.

---

## 2. SWC · Runnable · Port · Interface · RTE (p.224-249)

### 2.1 기술(Description) 3단계 (p.225)
| 레벨 | 내용 | 구성요소 |
|---|---|---|
| VFB | 컴포넌트와 연결 | Component, Composition, Port, Interface, Assembly/Delegation Connector |
| RTE | 컴포넌트의 내부 행동 | Runnable, Event, Access Point |
| Implementation | 코드 | Runnable body (`Rte_<SWC>.h` include, 인자/반환 `void`, p.235) |

### 2.2 핵심 용어
- **Component-type / Prototype** (p.226): 타입은 설계도, 프로토타입은 인스턴스. (교육 도구는 다중 인스턴스 미지원으로 표기)
- **SWC 종류** (p.227): Application, Composition, Service, Sensor-Actuator(ASW와 IoHwAb 사이), Parameter, ECU-Abstraction, Complex Device Driver, NV Block 등.
- **Port** (p.229): P-Port(제공)와 R-Port(요구). 한 포트는 정확히 한 컴포넌트에 속한다.
- **Interface 6종** (p.230): Sender-Receiver, Client-Server, Mode Switch, Trigger, Parameter, NvData.
  - **S/R**: 단방향 신호(Data Element) 송수신. 값 전달용.
  - **C/S**: 클라이언트가 Operation을 호출하고 서버가 수행 후 응답. 서비스/액추에이터 호출용.
- **Connector** (p.231): Assembly(P↔R, 같은 Interface), Delegation(Composition 내부 포트를 바깥으로 노출).
- **Runnable** (p.232): SWC 구현의 최소 단위. RTE가 호출하는 `void f(void)`. 한 SWC에 여러 개 가능 (예: `MainCyclic`, `Setting`).
- **RTE Event 12종** (p.233): TimingEvent(주기), DataReceivedEvent, DataReceiveErrorEvent, OperationInvokedEvent, DataSendCompleted, ModeSwitchEvent, Trigger 등. BackgroundEvent는 교육 도구 미지원.
- **Access Point / API** (p.234):

| 용도 | API |
|---|---|
| S/R 송신 | `Rte_Write_<P>_<DE>(data)`, `Rte_Send_...` (큐) |
| S/R 수신 | `Rte_Read_<R>_<DE>(&data)`, `Rte_Receive_...`, `Rte_DRead_...`(값 반환) |
| C/S 호출 | `Rte_Call_<R>_<Op>(...)` (동기/비동기) |
| 모드 | `Rte_Switch_...`, `Rte_Mode_...` |
| Implicit | `Rte_IRead_/Rte_IWrite_<Runnable>_...` (실행 시작 시 스냅샷) |

### 2.3 RTE 설정: Runnable을 Task에 올리기 (p.237-239)
- ECU 설정 쪽 `RteSwComponentInstance`(어느 SWC 인스턴스) → `RteEventToTaskMapping`(RteEventRef, RteMappedTaskRef, **PositionInTask**, **RteUsedOsAlarmRef**).
- 즉 "주기 Timing Event 하나 = OsAlarm이 OsTask를 깨우고, Task 안에서 PositionInTask 순서로 Runnable 호출" (p.239, p.962의 `OsTask_ASW_FG1_100ms` 예).
- 설계 12단계 (p.242): Interface → Component → Port → Composition/Prototype → Connector → Runnable → Event → Access Point → EcuSwMapping → RteSwComponentInstance → RteEventToTaskMapping → Runnable 코드.

### 2.4 교육 예제 요약 (p.254-341)
스위치 입력 → LED 출력: ① TimingEvent(주기) ② **S/R**로 SeatSwitch→SeatHeatingControl 값 전달 ③ **C/S**로 IoHwAb `WriteDirect(level)` 호출해 실제 핀 출력 (p.255, p.295, p.329). "주기 구동=Timing, 값 전달=S/R, 하드웨어 접근=C/S"라는 역할 분담이 핵심이다.

---

## 3. OS (OSEK/VDX 기반, p.99-183)

### 3.1 Task / 스케줄링 (p.106-110)
- Task = OS가 관리하는 실행 단위. **Basic**(running/ready/suspended)과 **Extended**(+waiting, Event 대기 가능).
- 우선순위 기반, 같은 우선순위는 먼저 들어온 것 먼저. **Full preemptive / Non-preemptive** 선택. 우선순위: Interrupt > 스케줄러 > Task (p.125).
- Hook: PreTask/PostTask/Error/Startup/Shutdown (p.103).

### 3.2 Event / Alarm / Counter / Resource
| 객체 | 역할 | 비고 |
|---|---|---|
| Event (p.118-122) | Extended Task 간 동기화 (`WaitEvent`, `SetEvent`) | Set은 Basic에서도 가능 |
| Counter (p.131) | 타이머 소스를 tick으로 변환 (예: 1 ms = 1 tick) | |
| **Alarm** (p.131-134) | Counter가 값에 도달하면 Task 활성화 / Event Set / Callback / Counter 증가 | `SetRelAlarm(id, 시작tick, 주기tick)`, 주기 0이면 1회 |
| Resource (p.137) | 임계영역 보호(Priority Ceiling). Get/ReleaseResource | 멀티코어 간 불가 |
| ISR Cat1/Cat2 (p.125) | Cat2는 OS API 사용 가능 | |
| Error (p.144) | `E_OS_*` 코드, ErrorHook | |

주기 Task 표준 패턴 (p.202-216 실습): Task 정의 → Alarm이 Counter 기준 N tick마다 `ActivateTask` → OS-Application에 소속 → Task 본문 작성. 부팅 시 1회 초기화는 **Autostart Task**로 (p.189).

### 3.3 AUTOSAR OS 확장 (p.149-182)
- 확장성 SC1~SC4: SC1=OSEK+Counter+**Schedule Table**+Stack Monitoring, SC2=+Timing Protection, SC3=+Memory Protection/OS-Application/IOC/CallTrustedFunction/Service Protection, SC4=SC2+SC3 (p.149-151).
- **Schedule Table** (p.153-154): expiry point(offset tick + 액션=Task activate/Event set)의 집합. single-shot 또는 repeating, 자동 시작 가능. "여러 주기 작업을 한 시간축에 offset으로 배치"하는 정적 스케줄.
- Timing Protection: 실행시간, 자원 점유시간, 활성화 간격 감시 (p.161-163).

---

## 4. 통신 스택 (p.607-654, 실습 p.655-778)

### 4.1 CAN 스택 (p.625, p.628)
```
 ASW -Rte_Write/Send-> RTE -> COM -> PduR -> CanIf -> Can(MCAL) -> CAN HW
                                  (CanTp는 PduR~CanIf 사이: 8B 초과 세그먼트, ISO 15765)
```
| 모듈 | 역할 (PDF 요약) |
|---|---|
| COM (p.634) | 신호 ↔ I-PDU 패킹/언패킹, 신호 라우팅(게이트웨이), **수신 신호 감시**, 주기 송신, 신호별 Timeout/Received/Send 알림 |
| PduR (p.636) | I-PDU ID 기준 **정적** 라우팅 (런타임 동적/내용 기반 라우팅 없음) |
| CanTp (p.637-641) | ISO 15765 세그멘테이션. SF/FF/CF/FC 프레임. 진단(Dcm) 같은 긴 메시지용 |
| CanIf (p.642-646) | CAN HW 독립 계층. Tx 요청/확인, Rx 표시, 컨트롤러 시작/정지, HOH/HRH 핸들 |
| Can (p.647) | 하드웨어 접근, 인터럽트(Rx/Tx/Bus-off) |
| CanTrcv | 트랜시버 상태 (Normal/Sleep/Standby) |

PDU 단계: 신호(ComSignal) → I-PDU → N-PDU(CanTp) → L-PDU(프레임) → P-PDU(비트) (p.628, p.632). DBC의 Frame/Signal이 I-PDU/ComSignal(→RTE S/R Data Element)로 변환된다 (p.630-633, 실습은 Import DBC, p.673).

### 4.2 상태/네트워크 관리 스택 (p.626-627)
- **ComM**: 통신 허용/금지 요청 중계, Nm에 Network Request/Release, CanSM에 명령.
- **CanSM**: 버스별 제어 흐름, **Bus-Off 복구**, Bus-Off 시 DEM에 진단 이벤트 저장.
- **BswM**: 모드/통신 상태별 규칙(Rule)에 따른 Action 제어.
- **Nm/OsekNm**: 네트워크 normal ↔ bus-sleep 전환 조정, 모든 노드 동기 sleep.

### 4.3 신호 송수신 흐름 (p.649-653)
- 주기 송신: Timing Task → Runnable → `Rte_Write` → COM 송신 버퍼 → 스택 → 송신.
- 주기 수신: 스택 → COM 신호 저장 → OS 알림으로 RTE 버퍼 저장 → Timing Runnable이 `Rte_Read`.
- 이벤트 수신: 위와 같으나 **DataReceivedEvent**로 Runnable을 깨운다 (p.652).
- COM 콜백 속성 (p.653): 송신측 `ComErrorNotification`, `ComTimeoutNotification`; 수신측 `ComNotification`, `ComInvalidNotification`, `ComTimeoutNotification` (생성 API 예: `Rte_COMCbkRxTOut_<Signal>`, `Rte_COMCbkInv_<Signal>`).

### 4.4 타임아웃 / 초기값 / Invalidation **[일반]** (PDF는 콜백 이름만 언급)
| 개념 | 설정 | 의미 |
|---|---|---|
| Rx 데드라인 | `ComFirstTimeout`, `ComTimeout` | 일정 시간 안에 새 수신이 없으면 타임아웃 발생 |
| 타임아웃 액션 | `ComRxDataTimeoutAction` = `NONE` / `REPLACE` (버전에 따라 `SUBSTITUTE`) | NONE: 마지막 값 유지, REPLACE: 초기값(대체값)으로 교체 |
| 초기값 | `ComSignalInitValue` | 첫 수신 전, 그리고 REPLACE 시 사용 |
| Invalid 값 | `ComSignalDataInvalidValue`, `ComDataInvalidAction`(NOTIFY/REPLACE) | 송신자가 "센서 불량"을 뜻하려고 보내는 예약 값 |
| RTE 쪽 | `Rte_Read` 반환: `RTE_E_OK` / `RTE_E_INVALID` / `RTE_E_NEVER_RECEIVED` / `RTE_E_MAX_AGE_EXCEEDED`(타임아웃) | ASW가 상태를 알고 분기 |

---

## 5. 진단 · 메모리 (간단히)

### 5.1 메모리 스택 (p.779-806)
- 6개 모듈: **NvM**(서비스) → **MemIf** → **Fee**(내장 Flash를 EEPROM처럼, 하위 Fls) / **Ea**(외부 EEPROM, 하위 Eep+SPI) (p.783).
- 블록 (p.787-788): RAM block(전역변수), NV block, ROM block(기본값), Administrative block. 타입 Native(1회 저장) / Redundant(2회 저장) / Dataset(여러 NV 블록 선택).
- 기본값 복원: NV 블록이 비었거나 깨지면 ROM block이 기본값 제공 (`NvM_RestoreBlockDefaults`, 결과 `NVM_REQ_RESTORED_FROM_ROM`, p.787, p.803).
- API: `NvM_ReadBlock/WriteBlock/ReadAll/WriteAll`은 비동기, 결과는 콜백/폴링. ASW는 C/S 포트로 `Rte_Call_<Port>_WriteBlock(SrcPtr)` 식 호출 (p.784, p.798-803). 우선순위: Immediate(크래시 데이터) 큐와 일반 큐 (p.794).
- NvM 에러는 **Dem**으로 보고 (p.795). 실습은 Native/Redundant(CRC16) 블록 설정과 WriteBlock 결과 확인 (p.857-881).

### 5.2 진단 **[일반]** (PDF에 Dem/Dcm 상세 없음)
- **Dem**: 진단 이벤트를 받아 디바운싱, DTC 상태 비트 관리, 스냅샷/확장 데이터를 NvM에 저장. ASW는 이벤트 PASSED/FAILED를 보고한다.
- **Dcm**: UDS(ISO 14229) 서비스 처리. 하위는 PduR → CanTp. 예: DTC 조회/삭제, DID 읽기, IO 제어.

---

## 6. 구성 · 개발 흐름 (ARXML, mobilgene studio)

### 6.1 산출물과 도구 (p.28-59, p.81-98)
- **mobilgene studio**: 현대차 표준 플랫폼용 통합 개발 도구(Eclipse 기반). ECU 설정 + MCAL 설정 + 코드 생성 + 컴파일/링크 통합 (p.55). 기능: AUTOSAR Explorer, SW Composition/Component Editor, Port Interface Editor, Data Mapping, RTE Editor(Task Mapping), ECUC Editor, DBC/LDF/FIBEX/Simulink ARXML 임포트, Validation (p.59). 빌드는 SCons+Python이며 `Build/generate.py`의 `generateBSW` source 목록에 App 소스를 추가한다 (p.966).
- 교육 하드웨어: TRK-MPC5606B 보드 + CodeWarrior 디버깅 (p.30-41, p.65-76). 일부 쪽에는 "Odin Studio"로 표기 (p.187, p.935).
- 파일 관례: `Configuration/System/Swcd_App/*.arxml`(SWC 정의), `Configuration/ECU/Ecud_*.arxml`(Os/Rte/Com/CanIf/IoHwAb 등 BSW 설정), MCAL 설정, `Generated/`(생성물), `Static_Code/`(직접 작성 코드) (p.46-51, p.104, p.598-601).

### 6.2 표준 작업 순서 (실습이 반복하는 패턴)
1. SWC 타입 생성(`.arxml`), Port/Interface/Runnable/Event 정의 (p.262-269, p.472-512)
2. Composition에 프로토타입 배치, P↔R **Assembly Connector** 연결 (p.270-272, p.552-565)
3. ECU Extract / `ECU Software Components Mapping` (SWC를 ECU에 할당) (p.274-280)
4. (통신 시) DBC Import → Data Mapping으로 ASW 포트 ↔ CAN 신호 연결 (p.670-684, p.745-747)
5. `Configure ECU & Generate Code`로 BSW/RTE 설정과 코드 생성 (p.281-284, p.748-753)
6. RTE Editor **Task Mapping**: 이벤트(Runnable)를 OsTask에 배치 (p.285-289, p.593-596, p.762-766)
7. Runnable 뼈대 생성(`Generate Code`) 후 본문 작성, 빌드 스크립트에 소스 추가 (p.290-293, p.963-967)
8. 빌드 → `.elf` → 보드 플래시/디버그 (p.603-605, p.992)

### 6.3 실습이 만드는 것 (한 줄 요약)
| 실습 | 만들어지는 것 | 쪽 |
|---|---|---|
| OS 실습 | 초기화용 비주기 Task, 1초 Alarm으로 구동하는 LED 토글 주기 Task | p.184-217 |
| RTE 실습 | 100 ms Timing Event → S/R → C/S(IoHwAb) 체인으로 스위치→LED | p.251-341 |
| IoHwAb 실습 | MCAL(Port/ADC/DIO/PWM) 설정 → IoHwAb P-Port → 3개 SWC(HeatingDial 입력, SHControl 로직, SeatHeating PWM 출력) 연결: **시트히터 제어** | p.343-606 |
| CAN 실습 | DBC 임포트, `ECU1_Msg_SH`(Tx)/`ECU2_Msg_PD`(Rx) 신호를 SeatSwitch SWC에 S/R 매핑, DataReceivedEvent로 수신, PCAN-USB로 확인 | p.655-778 |
| Memory 실습 | NvM Native/Redundant 블록 + Fee 블록 설정, App SWC가 WriteBlock 호출 | p.779-931 |
| 응용 | TORCS 시뮬레이터와 CAN 연동한 CC(정속)/LKAS SWC. 100 ms Runnable이 `Rte_Read`로 입력, `Rte_Write`로 출력 | p.932-1001 |

### 6.4 IoHwAb / MCAL (p.343-400)
- IoHwAb는 핀/전압/반전 같은 ECU 하드웨어 사정을 숨기고 **신호 기반** 인터페이스를 위에 제공한다 (p.347). 입력은 GET, 출력은 SET 오퍼레이션 (p.353). Sensor-Actuator SWC는 `Rte_Call_<p>_<op>`로 접근하고, 내부는 MCAL Port/Dio/Adc/Icu/Ocu/Pwm API를 호출한다 (p.352).

---

## 7. VBC 적용

### 7.1 매핑 표 (VBC → AUTOSAR)
| VBC 개념 | AUTOSAR 대응 | 설명 |
|---|---|---|
| `core/common.c` 입력 수신 → context 구조체 | **COM + RTE** (신호 수신, 버퍼, `Rte_Read`) | 외부 입력을 한 지점에서 받고 이후 모듈은 읽기만 한다 |
| `VbcContext` (const, 읽기 전용) | **R-Port 묶음** (S/R Data Element 모음, implicit `Rte_IRead` 스냅샷) | 한 step 동안 값이 안 바뀐다 = Implicit read |
| `out` 구조체 (모듈 출력) | **P-Port**(S/R `Rte_Write`) 또는 IoHwAb 호출 | 모듈은 자기 출력만 쓴다 |
| `lighting_step()` 등 `*_step(ctx,&out)` | **Runnable** (TimingEvent) | 개념은 동일. 파라미터로 명시했을 뿐 |
| 모듈 (lighting / wiper / door) | **Application SWC** (Runnable 여러 개 가능) | 한 모듈 = 한 SWC |
| 10 ms / 50 ms step 주기 | **TimingEvent 주기** → `RteEventToTaskMapping` → **OsAlarm**(Counter tick)이 OsTask 활성화 | 램프 10 ms Task, 와이퍼 50 ms Task 식. 같은 Task 안 순서는 PositionInTask |
| 주입형 ms 클럭 (`now_ms`) | **OS Counter** (tick 기준 시간) | 테스트에서 가짜 시계로 바꿀 수 있는 이유 = 시간 소스 추상화 |
| HoldTimer (N초 유지) | **Runnable 내부 상태**(static / Per-Instance Memory) + tick 카운트, 또는 OS Alarm one-shot (`SetRelAlarm(id, N*tick, 0)`) | 표준 "홀드 타이머" 객체는 없고 보통 주기 Runnable의 카운터로 구현 |
| C2~C4 신호 단절 0.5 s 감지 | **COM Rx 데드라인 모니터링** (`ComTimeout` = 0.5 s) | 수신 갱신이 없으면 타임아웃 |
| 단절 시 "마지막 값 유지" | `ComRxDataTimeoutAction = NONE` | |
| 단절 시 "안전한 기본값으로" | `ComRxDataTimeoutAction = REPLACE` + `ComSignalInitValue` | 대체값 정책의 근거 어휘 |
| 센서 결함 값 (범위 밖/예약 코드) | **Invalid value** (`ComSignalDataInvalidValue`, `RTE_E_INVALID`) | "수신은 되지만 쓸 수 없는 값" |
| 단절/결함 플래그를 context에 담아 모듈이 분기 | `Rte_Read` **반환 코드**(`RTE_E_OK`/`RTE_E_INVALID`/`RTE_E_MAX_AGE_EXCEEDED`)로 분기 | |
| C5 초기 상태 | **Init Runnable**(부팅 후 1회) + **ComSignalInitValue** (+ 필요 시 NvM ROM 기본값) | OS의 Autostart Task / StartupHook도 같은 위치 (p.147, p.189) |
| 도어락 출력 | **Sensor-Actuator SWC → IoHwAb C/S 포트** (`Rte_Call_..._SetDoorLock(level)`) → Dio | 출력 극성/핀은 IoHwAb 몫 (p.347, p.359) |
| 대시보드 경고 | **텔테일 신호 S/R**(COM Tx로 클러스터에 송신), 고장성이면 **Dem 이벤트** 보고 | 표시는 신호, 기록은 DTC |
| 콘솔/시나리오 파일 입력 | **HIL/SIL 스티뮬러**가 CAN 또는 핀을 구동하는 것에 해당 | p.934의 TORCS 연동과 같은 발상 |
| C 테스트 (입력 시퀀스 → 출력 검증) | **Runnable 단위 테스트**(RTE 스텁) / SIL | ASW는 HW 독립이라 PC에서 돌릴 수 있다 (p.17) |
| 규칙 ID (L1-L6, W1-W5, D1-D5, C1-C5) | **SWC 요구사항 추적**(Runnable별) | |

### 7.2 구조 대응 그림
```
 [콘솔/시나리오]          [COM + RTE 역할]              [SWC Runnable]                  [IoHwAb / COM Tx]
  입력 이벤트  --->  common.c: 수신, 타임아웃,  --->  lighting_step(ctx,&out)  10ms --->  lamp out
                     invalid 판정, ctx 채움           wiper_step(ctx,&out)     50ms --->  wiper out
                     (= COM Rx + Rte_Read)            door_step(ctx,&out)            --->  lock out, warning
        ^ 주입형 ms 클럭 (= OS Counter)               (HoldTimer = Runnable 내부 상태)
```

### 7.3 "VBC를 AUTOSAR식으로 설명할 때" 용어 치트시트
| VBC에서 하는 말 | AUTOSAR식 표현 |
|---|---|
| 모듈 / 기능 | Application SWC |
| step 함수 | Runnable (TimingEvent 구동) |
| 10 ms마다 호출 | 10 ms TimingEvent, OsAlarm으로 OsTask 활성화 |
| 입력 구조체 / 컨텍스트 | R-Port 데이터 (implicit read 스냅샷) |
| 출력 구조체 | P-Port 데이터 (`Rte_Write`) |
| 입력 수신부 (common) | COM 신호 수신 + RTE 버퍼 |
| 0.5초 신호 없음 | Rx 타임아웃 (Rx deadline monitoring) |
| 마지막 값 유지 / 기본값 교체 | TimeoutAction `NONE` / `REPLACE` |
| 기본값 | `ComSignalInitValue` |
| 센서 이상값 | Invalid value (`RTE_E_INVALID`) |
| N초 유지 타이머 | Runnable 내부 상태 카운터 (tick × 주기) |
| 시간 소스 | OS Counter (tick) |
| 초기 상태 | Init Runnable + 초기값 |
| 출력 드라이버 | IoHwAb (C/S `Set...`), 아래는 Dio/Pwm MCAL |
| 경고등 | 텔테일 신호(COM Tx) / Dem 이벤트 |
| 시나리오 파일 | 시뮬레이션 입력 (HIL/SIL 스티뮬러) |

### 7.4 이번 프로젝트에서 하지 않는 것 (경계)
- **AUTOSAR 구현 아님**: ARXML, RTE 생성기, `Rte_*` API, BSW 모듈(Com/PduR/CanIf/Can/CanTp/ComM/BswM/EcuM), mobilgene 같은 도구 체인을 쓰지 않는다. 용어는 설명용이며 코드에 `Rte_` 접두어를 만들지 않는다.
- **OS/스케줄링 없음**: Task/Alarm/Schedule Table/Event/Resource/ISR/우선순위/선점 모델을 구현하지 않는다. step은 단일 스레드 루프나 테스트가 직접 호출한다. 10/50 ms는 "주입형 클럭 기준 주기"일 뿐 실시간 보장이 없다.
- **실제 CAN 통신 없음**: PDU 패킹/언패킹, DBC, CanTp 세그멘테이션, Bus-Off 복구, Nm/sleep은 범위 밖. "수신 두절"은 시나리오 입력으로 모사한다.
- **진단/메모리 없음**: Dem/DTC, Dcm/UDS, NvM/Fee/Fls(비휘발 저장), 부팅 간 상태 유지는 구현하지 않는다. 경고는 콘솔/출력 구조체 표시로 끝난다.
- **HW 접근 없음**: MCAL/IoHwAb, 핀/ADC/PWM, 보드, 플래시/디버거가 없다. 출력은 구조체 값(콘솔 표시/테스트 검증)이다.
- **안전/보안 기능 없음**: OS Protection(타이밍/메모리), WdgM, E2E, SecOC, ISO 26262 프로세스 대응은 없다. Adaptive, 멀티코어도 다루지 않는다.
- **허용되는 차이**: AUTOSAR는 정적 구성(설정 파일 → 코드 생성)이지만 VBC는 코드 안의 상수/구조체로 구성한다. 문서에서는 "구성값 = 상수 정의 위치"로만 언급한다.

---

## 8. 빠른 참조: 쪽 번호
| 주제 | 쪽 |
|---|---|
| AUTOSAR 정의/버전/Classic vs Adaptive | 8-12 |
| 개발 5단계 / 계층 구조 | 15-22 |
| mobilgene studio 소개 | 55-59 |
| OSEK OS (Task/Event/Alarm/Resource) | 101-147 |
| AUTOSAR OS (SC1-4, Schedule Table) | 148-182 |
| SWC/Port/Interface/Runnable/Event/API | 224-239 |
| RTE 설계 사례 (Timing, S/R, C/S) | 240-249 |
| IoHwAb / ECU 추상화 / MCAL 드라이버 | 343-400 |
| 통신 스택 (COM/PduR/CanTp/CanIf/Can), 콜백 | 607-654 |
| 메모리 스택 (NvM/Fee/Ea) | 779-806 |
| CC/LKAS 응용 (Runnable 코드 예) | 932-1001 |
