#ifndef FINAL_L3_FSMMAIN_JUDGE_H
#define FINAL_L3_FSMMAIN_JUDGE_H

#include "mbed.h"
#include "L3_msg.h"

// Judge FSM states
#define L3_JUDGE_STATE_IDLE             0
#define L3_JUDGE_STATE_RUNNING          1

#define L3_JUDGE_MAX_PARTICIPANTS       1
#define L3_JUDGE_NICKNAME               L3_JUDGE_NICKNAME_STR
#define L3_JUDGE_FIRST_TURN_TIMEOUT_SEC 30

// IDLE 상태 초기화
void L3_judge_initIDLE(void);
// IDLE 상태 이벤트 핸들러
void L3_judge_handleIDLE(void);
// RUNNING 상태 이벤트 핸들러
void L3_judge_handleRUNNING(void);

// 상태 접근 및 관리 함수
uint8_t L3_judge_getCurrentState(void);
void    L3_judge_setCurrentState(uint8_t newState);
uint8_t L3_judge_getParticipantCount(void);
const char* L3_judge_getParticipantNickname(uint8_t index);


int L3_judge_isNicknameRegistered(const char* nickname); // 0 = not registered, 1 = already registered 
int L3_judge_addParticipant(const char* nickname);

// Build a JOIN_ACK Msg(참가자 승인)
void L3_judge_buildJoinAck(L3Message* out, const char* nickname, uint8_t count);

// Build the SETUP Msg
int  L3_judge_buildSetup(L3Message* out);

// Build a first TURN Msg
int  L3_judge_buildFirstTurn(L3Message* out);

#endif 
