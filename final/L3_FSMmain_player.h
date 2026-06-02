#ifndef L3_FSMMAIN_PLAYER_H
#define L3_FSMMAIN_PLAYER_H

#include <stdint.h>
#include "protocol_parameters.h"

#ifdef __cplusplus
extern "C" {
#endif

// Player FSM States (spec 10.2 기준)
typedef enum {
    PLAYER_STATE_IDLE     = 0,  // 닉네임 입력 대기 → JOIN 전송 트리거
    PLAYER_STATE_WAIT_ACK = 1,  // JOIN_ACK 대기
    PLAYER_STATE_JOINING  = 2,  // SETUP 대기 (순번 확정 전)
    PLAYER_STATE_PLAYING  = 3,  // 게임 진행 중
    PLAYER_STATE_HALTED   = 4   // reset 전까지 입력 무시
} PlayerState;

// Player FSM 진입점
void L3_player_initFSM(void);
void L3_player_runFSM(void);

// 상태 조회 (디버그/테스트용)
PlayerState L3_player_getState(void);
const char* L3_player_getStateString(void);

#ifdef __cplusplus
}
#endif

#endif // L3_FSMMAIN_PLAYER_H
