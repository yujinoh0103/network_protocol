#ifndef FINAL_L3_369ENGINE_H
#define FINAL_L3_369ENGINE_H

#include <stdint.h>
#include "L3_msg.h"
#include "protocol_parameters.h"

#define L3_369ENGINE_MAX_VALUE_LEN L3_MAX_VALUE_LEN

#ifdef __cplusplus
extern "C" {
#endif

void L3_369engine_init(const char* myNickname);
void L3_369engine_reset(void);
void L3_369engine_setPlayerOrder(const char playerOrder[L3_MAX_PLAYERS][L3_MAX_NICKNAME_LEN], uint8_t playerCount);

uint32_t L3_369engine_getCurrentNumber(void);
uint8_t  L3_369engine_getTurnCount(void);
const char* L3_369engine_getCurrentTurnPlayer(void);
uint8_t  L3_369engine_isMyTurn(const char* playerNickname);
uint8_t  L3_369engine_isMyTurnNow(void);

uint8_t     L3_369engine_onTurnReceived(const L3TurnMsg* turn);
const char* L3_369engine_computeExpectedAnswer(uint32_t number, char* outValue, uint8_t outValueLen);
const char* L3_369engine_computeExpectedAnswerForCurrentTurn(char* outValue, uint8_t outValueLen);
uint8_t     L3_369engine_isAnswerCorrect(const char* value);
uint8_t     L3_369engine_getTurnTimeout(uint32_t turnNumber);
uint8_t     L3_369engine_isTurnTimedOut(void);

#ifdef __cplusplus
}
#endif

#endif
