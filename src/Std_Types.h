/*
 * Std_Types.h - AUTOSAR 표준 타입 (도어 SWC에 필요한 최소한만)
 * 담당: D (김의현)
 *
 * 실제 AUTOSAR의 Std_Types.h / Platform_Types.h 중 쓰는 것만 옮겼다.
 * boolean은 AUTOSAR에서 unsigned char지만, 기존 구조체(DoorState, VbcOutputs)가 bool이라 bool로 맞춘다.
 */

#ifndef STD_TYPES_H
#define STD_TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef bool boolean;
typedef int32_t sint32;
typedef uint8_t uint8;

#ifndef TRUE
#define TRUE true
#endif
#ifndef FALSE
#define FALSE false
#endif

typedef uint8 Std_ReturnType;
#define E_OK ((Std_ReturnType)0u)
#define E_NOT_OK ((Std_ReturnType)1u)

/* RTE: 받은 데이터가 무효화됨 (AUTOSAR RTE 오류 코드) */
#define RTE_E_INVALID ((Std_ReturnType)1u)

#endif /* STD_TYPES_H */
