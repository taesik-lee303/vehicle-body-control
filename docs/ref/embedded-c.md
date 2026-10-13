# Embedded C 참고 (STM32 NUCLEO-L073RZ, Cortex-M0+) — VBC 배경 자료

> 출처: 세타랩 "Embedded C Programming 기초" 시리즈. 인용 `[04 p.N]` = 04.txt(개발환경) 슬라이드 N쪽 (슬라이드 하단 인쇄 번호 기준).
> 05=Digital Output, 06=HAL Driver, 07=Digital Input, 08=인터페이스, 09=UART, 10=Clock/Timer.
> 표기: **[PDF]** = PDF에 있는 내용, **[일반 권장]** = PDF에 없고 일반 지식으로 보충한 내용 (구분 주의).
> 목적: VBC(PC용 가상 차체제어, gcc+make)가 나중에 MCU로 옮겨질 때의 대응 관계 + 그대로 쓸 수 있는 C 관용구. 하드웨어 실습 자체는 VBC 범위 밖.

---

## 1. 개발환경·빌드 흐름 요약 [04]

- Cross-development: HOST(Windows PC)에서 타겟용 **교차 컴파일러(toolchain)** 로 빌드, JTAG/ST-Link로 다운로드·디버깅 [04 p.4]. VBC는 host gcc로 PC에서 그대로 실행하므로 이 단계가 없음.
- 흐름: 컴파일 -> 링크(링커 스크립트 `.sct`가 ROM/RAM 시작주소·크기 지정) -> `.hex/.bin` 이미지 -> flash [04 p.5-10].
- 산출물: `.lst`(소스별 리스팅), `.map`(코드 크기/주소 맵), `.hex/.bin`. 이미지 구성 = 벡터 테이블, Reset handler/startup, C startup, 앱 코드, C 런타임 라이브러리 [04 p.10].
- 부팅: Reset -> 벡터 테이블에서 SP/Reset_Handler 로드(`startup_stm32l073xx.s`) -> C startup(.data 복사, .bss 0 초기화는 [일반 권장]) -> `main` [04 p.11-15]. 도구: Keil MDK-ARM + STM32CubeMX(코드 생성) [04 p.17-24].
- 최적화 실습: `-O3`에서는 쓰이지 않는 `counter` 변수 코드가 사라져 디버거에서 값이 안 보이고, `-O0`에서는 보임 -> `volatile` 필요성 [04 p.25-33].

## 2. GPIO 입출력

### 2.1 출력 [05]
- I/O 접근 3가지: Special instruction I/O, **Memory-mapped I/O**, DMA [05 p.4]. Cortex-M은 MMIO: 레지스터 = 고정 주소의 `volatile` 32비트 값.
- GPIO 레지스터(포트당 32비트): `MODER`(핀당 2비트, 01=출력), `OTYPER`(push-pull/open-drain), `OSPEEDR`, `PUPDR`(풀업/풀다운), `IDR`(입력), `ODR`(출력), `BSRR`(set/reset), `LCKR`, `AFR[2]`, `BRR` [05 p.8, 24-34, 52].
- 사용 전 **RCC에서 포트 클록 enable** 필수 (GPIOA = `RCC_IOPENR` bit0, 주소 0x4002102C) [05 p.16-18, 41].
- PA5 LED 설정 순서: 클록 enable -> MODER=출력 -> OTYPER=push-pull -> OSPEEDR -> PUPDR=none -> ODR/BSRR로 값 쓰기 [05 p.38-45].
- **BSRR**: 하위 16비트=set, 상위 16비트=reset. ODR을 read-modify-write 하지 않고 한 번의 쓰기로 개별 비트를 변경(원자적) [05 p.30, 69]. BRR=reset 전용 [05 p.69].
- 레지스터 전체를 상수로 덮어쓰면 다른 핀이 망가짐 -> **read-modify-write 마스킹**(`reg &= ~(mask<<pos); reg |= val<<pos`)으로 해당 비트만 변경 [05 p.47-51].
- Push-Pull vs Open-Drain [05 p.12], 외부 LED는 저항값 계산 필요 [05 p.63-65].

### 2.2 입력 [05, 07]
- 핀 레벨은 `IDR`(HAL: `HAL_GPIO_ReadPin`)로 읽음. 풀업: 스위치 OFF=High, ON=Low / 풀다운은 반대 [05 p.10-11]. Nucleo User 버튼(PC13)은 눌림=`GPIO_PIN_RESET`(active-low) [07 p.15].
- **Polling vs Interrupt** [07 p.3-16]: polling = 메인 루프에서 `BSP_PB_GetState()` 반복 확인. Interrupt = `BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI)` 후 EXTI 라인 -> NVIC -> `EXTI4_15_IRQHandler` -> `HAL_GPIO_EXTI_IRQHandler` -> `HAL_GPIO_EXTI_Callback(pin)` [07 p.5-11, 16-17].
- 진동센서(SW-420, PB5/EXTI5) 예: 콜백에서 `volatile uint32_t nVibCnt`를 세다 임계값(300) 초과 시 `volatile uint8_t bVib=1` 플래그만 세우고, 실제 동작(LED 토글)은 main 루프에서 수행 [07 p.20-24] -> **ISR은 플래그만, 처리는 main** 패턴.
- 디바운스: **PDF에 직접 언급 없음.** [일반 권장] 기계식 스위치는 수 ms~수십 ms 채터링 -> "N ms 동안 같은 값이 유지돼야 확정"하는 카운터/타이머 방식(VBC HoldTimer와 같은 구조). EXTI만으로는 디바운스가 안 되므로 EXTI는 변화 트리거로 쓰고 tick에서 재확인.

## 3. HAL vs 레지스터 직접 제어 [05, 06]

| 구분 | 레지스터 직접 | HAL / BSP |
|---|---|---|
| 예 | `GPIOA->ODR`, `BSRR`, 마스킹 | `HAL_GPIO_Init/WritePin/ReadPin/TogglePin`, `BSP_LED_*`, `BSP_PB_*` |
| 장점 | 작고 동작이 투명, 빠름 | 이식성, 가독성, 인터럽트 콜백 구조 제공 |
| 단점 | 비트 실수, MCU 종속 | 오버헤드, 숨은 동작 |

- PDF의 가독성 개선 단계 [05 p.46-61]: 매직 주소 -> `#define` 베이스+오프셋 -> `GPIO_TypeDef` 구조체(`volatile` 멤버, 오프셋 순서 = 레지스터 맵) -> `GPIO_Init()/GPIO_Write_Pin()` 함수화. HAL이 하는 일의 축소판.
- HAL 구조: CMSIS(코어/디바이스 헤더) -> HAL 드라이버(`stm32l0xx_hal_*.c`) -> BSP(`stm32l0xx_nucleo.c`, LED/버튼을 enum+배열로 관리) -> 사용자 앱 [06 p.3-14, 44-47].
- 초기화 분리: `MX_xxx_Init`(CubeMX 생성) -> `HAL_xxx_Init` -> `HAL_xxx_MspInit`(핀/클록/IRQ, `*_hal_msp.c`) [06 p.19, 10 p.35]. 사용자 코드는 `/* USER CODE BEGIN/END */` 영역 안에만 [06 p.31].
- 인터럽트 모델: IRQHandler(`*_it.c`) -> `HAL_xxx_IRQHandler` -> **weak 콜백**(`HAL_GPIO_EXTI_Callback`, `HAL_UART_RxCpltCallback`, `HAL_TIM_PeriodElapsedCallback`)을 사용자가 재정의 [06 p.16-19]. 콜백에서 `htim->Instance == TIM7`처럼 **인스턴스 확인** 필수 [10 p.21, 37].

## 4. UART [08, 09]

- 비동기 직렬: data rate(baud), data bits, start/stop bit, parity. 바이트는 **LSB first** 전송 [08 p.12-14, 09 p.3]. 예시 설정: 115200 8N1, 오버샘플링 16, 흐름제어 없음 [09 p.26].
- Nucleo: USART2 (PA2=TX, PA3=RX)가 ST-Link VCOM으로 PC 터미널(PuTTY)에 연결 [09 p.13-15].
- 사용 방식 3가지 [09 p.7]:
  - **Polling**: `HAL_UART_Transmit(&huart2, buf, len, timeout)` — 블로킹, 부팅 메시지/디버그용 [09 p.17].
  - **Interrupt**: `HAL_UART_Receive_IT(&huart2, &b, 1)` -> `HAL_UART_RxCpltCallback`에서 `volatile` 플래그 set -> main에서 처리 후 **다시 Receive_IT 호출(재무장)** [09 p.19-23].
  - **DMA**: CPU 개입 없이 메모리<->주변장치 블록 전송 [09 p.8-9] (PDF는 개념 소개 수준, 코드 없음).
- printf 리디렉션: PDF는 외부 `hw_vcom` 라이브러리(github sjmun96/stm32_lib)로 `Printf()` 구현, `HW_VCOM_Init(&huart2)` 호출, RxCpltCallback을 위임 [09 p.23-26]. 표준 `printf` retarget(`_write`/`fputc`)은 PDF에 없음 -> [일반 권장].
- 기타 인터페이스: I2C(SCL/SDA, 슬레이브 주소), SPI(SCK/MOSI/MISO/SS, 고속), ADC(표본화->양자화->부호화; LM35 온도센서 실습) [08 p.5-19, 09 p.11].

## 5. Clock / Timer [10]

- 클록 소스: HSE/LSE 등 -> 클록 트리(CubeMX Clock Configuration)에서 SYSCLK -> AHB/APB -> 타이머 클록 [10 p.3-7, 32-33].
- 레지스터: `PSC`(prescaler), `CNT`, `ARR`(auto reload), `CCR`(capture/compare) [10 p.18].
- **주기 계산**: `f_event = f_timer_clk / ((PSC+1) x (ARR+1))`. PSC·ARR는 타이머 비트폭(16bit: 65535, PDF의 24bit 예는 2^24-1) 이내, 설정값은 "-1" 형태 [10 p.10, 19].
  - 예: 8 MHz에서 1 ms(1 kHz) = PSC 8-1 / ARR 1000-1 (또는 4-1/2000-1, 1-1/8000-1); 1 s = PSC 8000-1 / ARR 1000-1 [10 p.10, 19].
  - 다른 예: 84 MHz, PSC 8400-1, ARR 10000-1 -> 1 Hz [10 p.21]; 8 MHz, PSC 800-1, ARR 10000-1 -> 1 Hz [10 p.34]. 슬라이드마다 클록 가정이 다르므로 항상 실제 타이머 클록으로 재계산.
- 모드 [10 p.21-30]: Counter(주기 인터럽트: `HAL_TIM_Base_Start_IT` + PeriodElapsed 콜백), External clock counter, **Input Capture**(엣지 시점의 CNT 기록 -> 펄스폭/주파수 측정), **Output Compare**(CNT==CCR일 때 인터럽트/핀 Active/Inactive/Toggle), **PWM**(`Duty = Pulse/Period x 100`, `HAL_TIM_PWM_Start`, `__HAL_TIM_SetCompare`), One-pulse.
- **SysTick**: Cortex-M 코어의 24bit 다운카운터, 기본 1 ms 주기 인터럽트 [10 p.11-13]. `HAL_Init()`이 SysTick을 초기화 [06 p.31]. `HAL_GetTick()` 형태의 ms tick 활용은 PDF에 직접 없음 -> [일반 권장].
- 실습 패턴: 1초 TIM7 인터럽트 -> `volatile it_1sec` 플래그 -> main에서 LED 토글; PWM(1 kHz, TIM2 CH2) 밝기 4단계를 `Pulse += (Period+1)/4`로 순환 [10 p.35-39].
- `volatile int counter` 바쁜 대기 `delay(0x20000)` [05 p.45-47, 06 p.31]는 교육용. 제품 코드는 tick 기반으로.

## 6. 재사용 가치 있는 코드 패턴

```c
/* (a) 비트 필드 read-modify-write: 해당 비트만 변경 [05 p.48] */
reg &= ~(MASK << pos);  reg |= (val << pos);
/* (b) 레지스터 구조체 + volatile [05 p.52] */
typedef struct { volatile uint32_t MODER, OTYPER /* ... */; } GPIO_TypeDef;
/* (c) ISR/콜백은 플래그만, 처리는 main 루프 [07 p.23, 09 p.22, 10 p.37] */
volatile uint8_t flag;  void cb(void){ flag = 1; }  /* main: if(flag){flag=0; ...} */
/* (d) BSRR 토글: 쓰기 1회로 set/reset 동시 [05 p.69] */
BSRR = ((odr & pin) << 16) | (~odr & pin);
```
- (e) 핀/LED를 enum + 테이블 배열로 관리(BSP 방식) [06 p.44-47]; `GPIO_PinState {RESET=0, SET}` 열거형과 핀 마스크 `1U << n` [05 p.57, 69].

## 7. VBC 적용

### 7.1 개념 매핑

| VBC 개념 | MCU 대응 | 비고 |
|---|---|---|
| `out.headlight_on` 등 이진 출력 | GPIO 출력 (`BSRR`/`HAL_GPIO_WritePin`), 부하는 릴레이/드라이버 경유 | 모듈은 `out` 구조체만 채우고 실제 핀 쓰기는 별도 HAL 계층 1곳에서 |
| 램프 단계/와이퍼 단계(저속·고속·간헐) | PWM duty(`__HAL_TIM_SetCompare`) 또는 릴레이 다중 접점 | 단계 -> duty 테이블화 (10 PWM 실습 패턴) |
| 스위치/레버 입력 | 디바운스된 GPIO 입력(active-low 주의), 필요 시 EXTI 트리거 | 디바운스 = N ms 안정 확인 |
| `ctx->now` (주입 ms 시계) | 1 ms tick 카운터 (SysTick/TIMx 인터럽트가 증가시키는 `volatile uint32_t`) | VBC 테스트는 이 값을 주입 -> 하드웨어 독립 유지 |
| 콘솔 입력/출력 | UART(USART2 VCOM) RX 인터럽트 + 링버퍼, TX printf | PC판 stdin과 같은 "수신 바이트열" 인터페이스로 추상화 |
| 50 ms 주기 재전송 | 타이머 주기 태스크 (플래그 또는 `now - last >= 50`) | ISR이 아닌 main/태스크 루프에서 실행 |
| C1 HoldTimer(N초 연속 유지) | tick 카운트/타임스탬프 차 비교 | 입력이 끊기면 리셋 — 디바운스와 같은 구조 |
| C2~C4 센서 이상/수신 두절 판정 | "마지막 수신 시각과 now 차이 > 타임아웃" | 타임스탬프 차 비교 필수 (아래 3번) |
| const context struct, 모듈별 state struct | MCU에선 정적 영역에 1개씩 배치 | 레지스터 접근 코드만 `volatile`, 로직 모듈은 순수 C 유지 |

### 7.2 이식 시 주의
1. **volatile**: ISR/콜백과 main이 공유하는 변수, 메모리 맵 레지스터는 `volatile` (없으면 -O2/-O3에서 읽기 생략) [04 p.30-32]. 단 `volatile`은 원자성을 보장하지 않음 [일반 권장].
2. **ISR<->main 공유**: ISR은 플래그/카운터만 갱신, 무거운 처리·긴 printf 금지 [07, 09 패턴]. 32비트 단일 변수 읽기는 M0+에서 원자적이나 다중 필드 구조체는 인터럽트 금지 구간으로 보호 [일반 권장]. 모듈은 ISR이 채운 값을 **스냅샷으로 복사한 const ctx**만 본다.
3. **32비트 ms tick 오버플로**: `uint32_t` ms는 2^32 ms ~ 49.7일 후 0으로 wrap [일반 권장]. 비교는 `now >= deadline` 대신 **부호 없는 뺄셈** `(uint32_t)(now - start) >= timeout` (wrap 후에도 정확, timeout < 2^31 ms 조건). 절대 시각보다 "시작 시각 + 경과 비교"를 저장.
4. **정수 폭**: `<stdint.h>` `uint8_t/uint16_t/uint32_t` 사용, 레지스터 값은 `uint32_t`, 상수에 `U/UL` 접미사 [05 p.46, 04 p.34-35 데이터형 크기 실습]. 시프트는 `1U << n`, 부호 있는 정수 시프트 금지 [일반 권장]. PC gcc(`int`=32비트, `long`은 OS별 상이)와 MCU 간 폭 차이 주의.
5. **엔디언**: Cortex-M은 little-endian [04 p.37-38]. 바이트 단위 프로토콜은 구조체 memcpy 대신 명시적 shift/mask로 직렬화 [일반 권장].
6. **바쁜 대기 금지**: `sleep`/`delay` 루프 대신 tick 비교(VBC의 "time()/sleep() 금지"와 동일 방향). 최적화로 delay 루프가 사라지기도 함 [04 p.30].
7. **극성/채터링/초기상태**: 입력 극성(active-low)은 HAL 계층에서 정규화(눌림=1), 부팅 직후 첫 샘플 처리 정책을 정함 [일반 권장].
8. **재무장/에러 처리**: UART RX 인터럽트는 수신 완료마다 `Receive_IT` 재호출 필요 [09 p.23]; HAL 반환값(`HAL_StatusTypeDef`) 확인 [일반 권장].

### 7.3 C 코딩 규약
- PDF에 **MISRA 언급 없음** -> 일반 권장과 PDF 코드에서 관찰되는 관례를 구분.
- PDF 관례: 하드웨어 상수는 `#define` + `U/UL` 접미사 [05 p.46, 54], 레지스터는 `volatile` 구조체 [05 p.52], 열거형으로 상태 표현 [05 p.57], 사용자 코드는 지정 영역에만 [06 p.31], 한 핀만 바꿀 땐 마스킹 [05 p.48], 의미 있는 이름(`LED2_Pin`, `USER_VIB_Pin`) [06 p.37, 07 p.23].
- 일반 권장(MISRA-like 기본): 고정폭 정수형, 매직 넘버 대신 이름 있는 상수, 암묵 형변환 최소화·명시적 cast, 비트 연산은 unsigned만, 모든 `switch`에 `default`/`break`, 헤더 include guard, 전역 최소화(VBC는 전역 금지), 함수 반환값 확인, 포인터 NULL 체크, 매크로 인자 괄호.
- 주의: PDF 코드는 교육용이라 `GPIO_Init`에서 `GPIO_TypeDef`를 초기값 컨테이너로 재사용하고 `#define GPIOA (GPIO_TypeDef *) GPIOA_BASE`처럼 괄호가 없는 매크로 등 거친 부분이 있으므로 그대로 복사하지 말 것 [05 p.54, 57-61].
