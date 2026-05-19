#ifndef L3_FSMMAIN_PLAYER_H
#define L3_FSMMAIN_PLAYER_H

#include <stdint.h>
#include "protocol_parameters.h"

#ifdef __cplusplus
extern "C" {
#endif

// Player FSM States
typedef enum {
    PLAYER_STATE_IDLE = 0,
    PLAYER_STATE_WAIT_ACK,
    PLAYER_STATE_JOINING,
    PLAYER_STATE_PLAYING
} PlayerState;

// Player FSM init & run
void L3_player_initFSM(const char* nickname);
void L3_player_runFSM(void);

// State getter (for debug/test)
PlayerState L3_player_getState(void);
const char* L3_player_getStateString(void);

#ifdef __cplusplus
}
#endif

#endif // L3_FSMMAIN_PLAYER_H