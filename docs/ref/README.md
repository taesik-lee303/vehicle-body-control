# 참고 지식 (docs/ref)

멀티캠퍼스 지능형 모빌리티 강의자료(PDF 17개, 중복 1개 포함)를 VBC 프로젝트 관점으로 정리한 **배경 지식 문서**다.

> **우선순위**: 요구사항의 근거는 사양서 VBC-SRS-001(`docs/spec/`)과 `docs/requirements.md`뿐이다. 이 폴더의 내용은 설명·테스트 설계·발표 질의 대비용이며, 사양서와 다르면 사양서가 맞다. 여기 있는 수치(DBC 정의, ASIL 평가 등)는 대부분 **제안/예시**다.
> 표기: `[보강]`, `[일반]`, `[일반 권장]` = 강의 PDF에 없고 일반 지식으로 보충한 부분. PDF에서 확인된 내용과 구분해서 읽는다.

## 문서 목록
| 문서 | 언제 보나 | 원 자료 |
|---|---|---|
| [sw-testing.md](sw-testing.md) | 테스트케이스 설계(동치분할·경계값·결정표), 커버리지, 추적성, 리뷰·결함 기록 형식, `verification.md` 작성 | `SW 테스팅 기초_강의교재` (366쪽) |
| [vehicle-comm.md](vehicle-comm.md) | C2~C4(0.5초 수신 중단·센서 고장)의 개념 근거, 주기 송신, 신호 유효성, BCM 역할, 입력 신호 DBC풍 정의 | 차량용 통신시스템 1~6 |
| [functional-safety.md](functional-safety.md) | 안전 우선 규칙(L5·W5·D1·C4)을 왜 그렇게 정했는지 설명, HAZOP·ASIL 개념 | `참고자료_HARA` |
| [autosar.md](autosar.md) | VBC 구조를 양산 AUTOSAR 용어로 설명할 때, 하지 않는 범위의 경계 | `autosar_mobilgen` (약 1000쪽) |
| [embedded-c.md](embedded-c.md) | MCU로 옮길 때의 대응(GPIO·타이머·UART), 이식 시 주의(volatile, tick 오버플로) | `Embedded_C_pdf` 02~08 |

## 요구사항 ↔ 참고 문서
| 사양서 항목 | 볼 곳 |
|---|---|
| C1 N초 연속(HoldTimer) | sw-testing 5.2·5.3(타이머 경계·리셋), embedded-c 7.2(tick 비교) |
| C2~C4 0.5초·범위 | vehicle-comm 4·6, sw-testing 5.3 |
| L1~L6, W1~W5 우선순위 규칙 | sw-testing 5.2(결정표, MC/DC), functional-safety 4.1 |
| W2 임계값(10/40/70%), D3(15 km/h) | sw-testing 5.3(3-value 경계값) |
| D1~D5 | functional-safety 4, sw-testing 5.2 |
| C5 초기 상태 | autosar 7.1(Init·초기값) |
| 팀 규칙(작성자 ≠ 검증자, AI 오류 기록) | sw-testing 5.5 |

## 이 정리에서 제외한 자료
| 자료 | 이유 |
|---|---|
| `final_report2.docx` (다중 센서 스푸핑 분류 미니프로젝트) | 머신러닝 기반 센서 공격 탐지로, VBC 범위(규칙 기반 차체 제어)와 무관 |
| `이태식_지능형모빌리티_교육정리.md` | 개인 포트폴리오·교육 이력 정리로, 프로젝트 요구사항 근거가 아님. 일부 개념(V-모델, ASPICE, ISO 26262)은 위 문서에 이미 반영 |
| `멀캠강의자료-….zip` | 같은 이름 폴더의 압축본(중복) |
| 강의자료 안의 `참고자료_HARA` 중복본 | 같은 내용 2부, 1부만 사용 |
| `6_실습_peak.pdf`의 장비 조작 절차 | 하드웨어(TC375, PEAK) 실습 단계, 개념·값만 vehicle-comm 5절에 요약 |

## 한계
- PDF 일부는 슬라이드 본문이 그림이라 텍스트 추출이 안 됐다(동치/경계 기준 슬라이드, ISO 26262 표, MC/DC 진리표, ASIL 표). 해당 부분은 일반 지식으로 보충하고 표시했으며, 인용 전에는 원본 PDF 해당 쪽을 확인한다.
- 쪽 번호 표기는 문서마다 다르다(슬라이드 인쇄 번호 또는 PDF 페이지). 각 문서 머리말을 따른다.
- 강의 슬라이드의 HARA 실습 값 하나(S3·E2·C3 → C)는 ISO 26262 표(B)와 다르다. functional-safety.md에 표시했다.
