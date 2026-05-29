#include "L3_369engine.h"
#include "mbed.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

static char myNickname[L3_MAX_NICKNAME_LEN];
static char currentTurnPlayer[L3_MAX_NICKNAME_LEN];
static char playerOrder[L3_MAX_PLAYERS][L3_MAX_NICKNAME_LEN];
static uint8_t  playerCount  = 0;
static uint32_t currentNumber = 0;
static uint8_t  turnCount     = 0;
static Timer turnTimer;
static uint8_t currentTurnTimeoutSec = 0;
static uint8_t turnTimerActive = 0;

static void copyNickname(char* dest, const char* src)
{
    if (dest == NULL) return;
    if (src  == NULL) { dest[0] = '\0'; return; }
    strncpy(dest, src, L3_MAX_NICKNAME_LEN - 1);
    dest[L3_MAX_NICKNAME_LEN - 1] = '\0';
}

void L3_369engine_init(const char* myNicknameIn)
{
    copyNickname(myNickname, myNicknameIn);
    L3_369engine_reset();
}

void L3_369engine_reset(void)
{
    currentNumber = 0;
    turnCount     = 0;
    currentTurnPlayer[0] = '\0';
    playerCount = 0;
    memset(playerOrder, 0, sizeof(playerOrder));

    currentTurnTimeoutSec = 0;
    turnTimerActive = 0;
    turnTimer.stop();
    turnTimer.reset();
}

void L3_369engine_setPlayerOrder(
    const char playerOrderIn[L3_MAX_PLAYERS][L3_MAX_NICKNAME_LEN],
    uint8_t count)
{
    if (count > L3_MAX_PLAYERS) count = L3_MAX_PLAYERS;
    playerCount = count;
    for (uint8_t i = 0; i < playerCount; i++) {
        copyNickname(playerOrder[i], playerOrderIn[i]);
    }
}

uint32_t L3_369engine_getCurrentNumber(void) { return currentNumber; }
uint8_t  L3_369engine_getTurnCount(void)     { return turnCount; }

const char* L3_369engine_getCurrentTurnPlayer(void) { return currentTurnPlayer; }

uint8_t L3_369engine_isMyTurn(const char* playerNickname)
{
    if (playerNickname == NULL) return 0;
    return (strncmp(playerNickname, currentTurnPlayer, L3_MAX_NICKNAME_LEN) == 0) ? 1 : 0;
}

uint8_t L3_369engine_isMyTurnNow(void)
{
    return L3_369engine_isMyTurn(myNickname);
}

uint8_t L3_369engine_onTurnReceived(const L3TurnMsg* turn)
{
    if (turn == NULL) return 0;

    turnCount++;
    currentNumber = turnCount;
    copyNickname(currentTurnPlayer, turn->player_nickname);

    currentTurnTimeoutSec = turn->timeout_sec;
    turnTimer.reset();
    turnTimer.start();
    turnTimerActive = 1;

    return 1;
}

// 숫자의 각 자릿수 중 3·6·9 개수를 반환
static uint8_t count369Digits(uint32_t number)
{
    if (number == 0) return 0;
    uint8_t count = 0;
    while (number > 0) {
        uint8_t digit = number % 10;
        if (digit == 3 || digit == 6 || digit == 9) {
            count++;
        }
        number /= 10;
    }
    return count;
}

const char* L3_369engine_computeExpectedAnswer(
    uint32_t number, char* outValue, uint8_t outValueLen)
{
    if (outValue == NULL || outValueLen == 0) return NULL;

    uint8_t count = count369Digits(number);
    if (count == 0) {
        // N=0: 숫자 그대로 출력
        int written = snprintf(outValue, outValueLen, "%lu", (unsigned long)number);
        if (written < 0 || (uint8_t)written >= outValueLen) outValue[0] = '\0';
    } else {
        // N>=1: '*' × N
        if (count >= outValueLen) count = outValueLen - 1;
        for (uint8_t i = 0; i < count; i++) outValue[i] = '*';
        outValue[count] = '\0';
    }
    return outValue;
}

const char* L3_369engine_computeExpectedAnswerForCurrentTurn(
    char* outValue, uint8_t outValueLen)
{
    return L3_369engine_computeExpectedAnswer(currentNumber, outValue, outValueLen);
}

uint8_t L3_369engine_isAnswerCorrect(const char* value)
{
    if (value == NULL) return 0;
    char expected[L3_MAX_VALUE_LEN];
    L3_369engine_computeExpectedAnswerForCurrentTurn(expected, sizeof(expected));
    return (strncmp(expected, value, L3_MAX_VALUE_LEN) == 0) ? 1 : 0;
}

uint8_t L3_369engine_getTurnTimeout(uint32_t turnNumber)
{
    if (turnNumber <= 10) return 10;
    if (turnNumber <= 20) return 7;
    return 5;
}

uint8_t L3_369engine_isTurnTimedOut(void)
{
    if (!turnTimerActive) {
        return 0;
    }

    if (currentTurnTimeoutSec == 0) {
        return 0;
    }

    uint32_t elapsedMs = turnTimer.read_ms();
    uint32_t timeoutMs = (uint32_t)currentTurnTimeoutSec * 1000U;

    return (elapsedMs >= timeoutMs) ? 1 : 0;
}
