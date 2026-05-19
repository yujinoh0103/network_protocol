
#include "mbed.h"
#include "L3_369engine.h"
#include <stdio.h>
#include <assert.h>
// Serial pc(USBTX, USBRX);  ← 이 줄 삭제
extern Serial pc;              // ← 이걸로 교체

static void test_computeExpectedAnswer()
{
    char buf[16];
    assert(strcmp(L3_369engine_computeExpectedAnswer(1,   buf, sizeof(buf)), "1")   == 0);
    assert(strcmp(L3_369engine_computeExpectedAnswer(3,   buf, sizeof(buf)), "*")   == 0);
    assert(strcmp(L3_369engine_computeExpectedAnswer(13,  buf, sizeof(buf)), "*")   == 0);
    assert(strcmp(L3_369engine_computeExpectedAnswer(33,  buf, sizeof(buf)), "**")  == 0);
    assert(strcmp(L3_369engine_computeExpectedAnswer(96,  buf, sizeof(buf)), "**")  == 0);
    assert(strcmp(L3_369engine_computeExpectedAnswer(369, buf, sizeof(buf)), "***") == 0);
    pc.printf("[PASS] computeExpectedAnswer\n");
}

static void test_turnCount()
{
    L3_369engine_init("player1");

    L3TurnMsg turn;
    strncpy(turn.player_nickname, "player1", L3_MAX_NICKNAME_LEN);

    L3_369engine_onTurnReceived(&turn);
    L3_369engine_onTurnReceived(&turn);
    L3_369engine_onTurnReceived(&turn);  // currentNumber = 3

    assert(L3_369engine_getCurrentNumber() == 3);

    char buf[16];
    assert(strcmp(L3_369engine_computeExpectedAnswerForCurrentTurn(buf, sizeof(buf)), "*") == 0);
    pc.printf("[PASS] turnCount\n");
}

static void test_isMyTurn()
{
    L3_369engine_init("alice");

    L3TurnMsg turn;
    strncpy(turn.player_nickname, "alice", L3_MAX_NICKNAME_LEN);
    L3_369engine_onTurnReceived(&turn);
    assert(L3_369engine_isMyTurnNow() == 1);

    strncpy(turn.player_nickname, "bob", L3_MAX_NICKNAME_LEN);
    L3_369engine_onTurnReceived(&turn);
    assert(L3_369engine_isMyTurnNow() == 0);

    pc.printf("[PASS] isMyTurn\n");
}

static void test_timeout()
{
    assert(L3_369engine_getTurnTimeout(0)  == 5);
    assert(L3_369engine_getTurnTimeout(4)  == 5);
    assert(L3_369engine_getTurnTimeout(5)  == 3);
    assert(L3_369engine_getTurnTimeout(12) == 3);
    assert(L3_369engine_getTurnTimeout(13) == 2);
    pc.printf("[PASS] getTurnTimeout\n");
}

void run_test_369engine()
{
    pc.printf("\n=== 369 Engine Test Start ===\n");
    test_computeExpectedAnswer();
    test_turnCount();
    test_isMyTurn();
    test_timeout();
    pc.printf("=== 전체 통과 ===\n");
}
