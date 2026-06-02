#include "mbed.h"
#include "L3_FSMmain_player.h"
#include "L3_369engine.h"
#include "L3_msg.h"
#include "protocol_parameters.h"
#include <string.h>
#include <assert.h>

extern Serial pc;

// -------------------------------------------------------
// Mock: WAIT_ACK 연동이 됐다고 가정
// JOINING 상태부터 시작하는 시나리오
// -------------------------------------------------------

// 테스트 1: SETUP 수신 후 PLAYING 상태로 전이되는지
static void test_setup_received()
{
    // 369 엔진 초기화
    L3_369engine_init("alice");

    // player_order 세팅 (SETUP 수신했다고 가정)
    char order[L3_MAX_PLAYERS][L3_MAX_NICKNAME_LEN] = {
        "alice", "bob", "carol", "dave"
    };
    L3_369engine_setPlayerOrder(order, L3_MAX_PLAYERS);

    // 내부 숫자 초기화 확인 [R-SETUP-03]
    assert(L3_369engine_getCurrentNumber() == 0);

    pc.printf("[PASS] test_setup_received\n");
}

// 테스트 2: TURN 수신 후 내부 숫자가 올바르게 증가하는지
static void test_turn_number_increment()
{
    L3_369engine_init("alice");

    L3TurnMsg turn;

    // 1번째 TURN → currentNumber = 1
    strncpy(turn.player_nickname, "alice", L3_MAX_NICKNAME_LEN);
    L3_369engine_onTurnReceived(&turn);
    assert(L3_369engine_getCurrentNumber() == 1);

    // 2번째 TURN → currentNumber = 2
    strncpy(turn.player_nickname, "bob", L3_MAX_NICKNAME_LEN);
    L3_369engine_onTurnReceived(&turn);
    assert(L3_369engine_getCurrentNumber() == 2);

    // 3번째 TURN → currentNumber = 3
    strncpy(turn.player_nickname, "carol", L3_MAX_NICKNAME_LEN);
    L3_369engine_onTurnReceived(&turn);
    assert(L3_369engine_getCurrentNumber() == 3);

    pc.printf("[PASS] test_turn_number_increment\n");
}

// 테스트 3: 내 차례일 때만 ANSWER를 보내야 하는지 판단
static void test_my_turn_detection()
{
    L3_369engine_init("alice");

    L3TurnMsg turn;

    // alice 차례
    strncpy(turn.player_nickname, "alice", L3_MAX_NICKNAME_LEN);
    L3_369engine_onTurnReceived(&turn);
    assert(L3_369engine_isMyTurnNow() == 1);  // 내 차례
    pc.printf("[Player] currentNumber=%lu, isMyTurn=%d\n",
        (unsigned long)L3_369engine_getCurrentNumber(),
        L3_369engine_isMyTurnNow()
    );

    // bob 차례
    strncpy(turn.player_nickname, "bob", L3_MAX_NICKNAME_LEN);
    L3_369engine_onTurnReceived(&turn);
    assert(L3_369engine_isMyTurnNow() == 0);  // 내 차례 아님

    pc.printf("[PASS] test_my_turn_detection\n");
}

// 테스트 4: 내 차례일 때 ANSWER 값이 올바른지
static void test_answer_value()
{
    L3_369engine_init("alice");
    char buf[L3_MAX_VALUE_LEN];
    L3TurnMsg turn;

    // 턴 1: alice → currentNumber=1 → "1"
    strncpy(turn.player_nickname, "alice", L3_MAX_NICKNAME_LEN);
    L3_369engine_onTurnReceived(&turn);
    L3_369engine_computeExpectedAnswerForCurrentTurn(buf, sizeof(buf));
    pc.printf("[Player] turn=1, answer=%s\n", buf);
    assert(strcmp(buf, "1") == 0);

    // 턴 2: bob → currentNumber=2 → "2"
    strncpy(turn.player_nickname, "bob", L3_MAX_NICKNAME_LEN);
    L3_369engine_onTurnReceived(&turn);
    L3_369engine_computeExpectedAnswerForCurrentTurn(buf, sizeof(buf));
    pc.printf("[Player] turn=2, answer=%s\n", buf);
    assert(strcmp(buf, "2") == 0);

    // 턴 3: carol → currentNumber=3 → "*"
    strncpy(turn.player_nickname, "carol", L3_MAX_NICKNAME_LEN);
    L3_369engine_onTurnReceived(&turn);
    L3_369engine_computeExpectedAnswerForCurrentTurn(buf, sizeof(buf));
    pc.printf("[Player] turn=3, answer=%s\n", buf);
    assert(strcmp(buf, "*") == 0);

    // 턴 33: → currentNumber=33 → "**"
    L3_369engine_reset();
    L3_369engine_init("alice");
    for (int i = 0; i < 33; i++) {
        strncpy(turn.player_nickname, "alice", L3_MAX_NICKNAME_LEN);
        L3_369engine_onTurnReceived(&turn);
    }
    L3_369engine_computeExpectedAnswerForCurrentTurn(buf, sizeof(buf));
    pc.printf("[Player] turn=33, answer=%s\n", buf);
    assert(strcmp(buf, "**") == 0);

    pc.printf("[PASS] test_answer_value\n");
}

// 테스트 5: GAMEOVER 수신 후 리셋 확인
static void test_gameover_reset()
{
    L3_369engine_init("alice");

    L3TurnMsg turn;
    strncpy(turn.player_nickname, "alice", L3_MAX_NICKNAME_LEN);
    L3_369engine_onTurnReceived(&turn);
    assert(L3_369engine_getCurrentNumber() == 1);

    // GAMEOVER 수신 → reset
    L3_369engine_reset();
    assert(L3_369engine_getCurrentNumber() == 0);

    pc.printf("[PASS] test_gameover_reset\n");
}

// -------------------------------------------------------
// 전체 실행
// -------------------------------------------------------
void run_test_player()
{
    pc.printf("\n=== Player FSM Test Start ===\n");
    pc.printf("(WAIT_ACK 연동 완료 가정, JOINING 이후 시나리오)\n\n");

    test_setup_received();
    test_turn_number_increment();
    test_my_turn_detection();
    test_answer_value();
    test_gameover_reset();

    pc.printf("\n=== Player FSM 전체 통과 ===\n");
}