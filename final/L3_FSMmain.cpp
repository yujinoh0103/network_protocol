#include "mbed.h"
#include "L3_FSMmain_player.h"
#include "L3_369engine.h"
#include "L3_msg.h"
#include "L3_FSMevent.h"
#include "L3_LLinterface.h"
#include "L3_timer.h"
#include "protocol_parameters.h"
#include <string.h>

extern Serial pc;

// -------------------------------------------------------
// Internal state
// -------------------------------------------------------
static PlayerState playerState = PLAYER_STATE_IDLE;
static char myNickname[L3_MAX_NICKNAME_LEN];
static uint8_t isJudgeKnown = 0;

// WAIT_ACK 재전송 관련
static uint8_t waitAckRetryPending = 0;
static Timeout retryTimer;

// -------------------------------------------------------
// Forward declarations
// -------------------------------------------------------
static void stateIdle(void);
static void stateWaitAck(void);
static void stateJoining(void);
static void statePlaying(void);
static void sendJoin(void);
static void sendAnswer(void);
static void retryJoinCallback(void);

// -------------------------------------------------------
// Init
// -------------------------------------------------------
void L3_player_initFSM(const char* nickname)
{
    strncpy(myNickname, nickname, L3_MAX_NICKNAME_LEN - 1);
    myNickname[L3_MAX_NICKNAME_LEN - 1] = '\0';

    L3_369engine_init(myNickname);

    playerState = PLAYER_STATE_IDLE;
    isJudgeKnown = 0;
    waitAckRetryPending = 0;

    pc.printf("[Player] FSM init. nickname=%s\n", myNickname);
}

// -------------------------------------------------------
// Run (called every loop from L3_FSMrun)
// -------------------------------------------------------
void L3_player_runFSM(void)
{
    switch (playerState) {
        case PLAYER_STATE_IDLE:     stateIdle();    break;
        case PLAYER_STATE_WAIT_ACK: stateWaitAck(); break;
        case PLAYER_STATE_JOINING:  stateJoining(); break;
        case PLAYER_STATE_PLAYING:  statePlaying(); break;
        default: break;
    }
}

// -------------------------------------------------------
// State: IDLE
// 탈출: JOIN 전송 → WAIT_ACK
// -------------------------------------------------------
static void stateIdle(void)
{
    pc.printf("[Player] IDLE: sending JOIN\n");
    sendJoin();
    L3_timer_startTimer();
    playerState = PLAYER_STATE_WAIT_ACK;
}

// -------------------------------------------------------
// State: WAIT_ACK
// 탈출: JOIN_ACK 수신 → JOINING
//       타임아웃 → 3초 대기 → IDLE (재전송)
// -------------------------------------------------------
static void stateWaitAck(void)
{
    if (waitAckRetryPending) {
        return;
    }

    if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
        L3_event_clearEventFlag(L3_event_msgRcvd);

        uint8_t* dataPtr = L3_LLI_getMsgPtr();
        uint8_t size = L3_LLI_getSize();

        // type만 빠르게 확인
        if (L3_msg_peekType(dataPtr, size) != L3_MSG_JOIN_ACK) {
            return;
        }

        // 역직렬화
        L3Message msg;
        if (!L3_msg_deserialize(dataPtr, size, &msg)) {
            return;
        }

        // [R-JOIN-11] 첫 JOIN_ACK 송신자를 Judge로 인지
        if (!isJudgeKnown) {
            isJudgeKnown = 1;
            pc.printf("[Player] Judge identified. srcId=%d\n", L3_LLI_getSrcId());
        }

        L3_timer_stopTimer();
        pc.printf("[Player] JOIN_ACK received. count=%d -> JOINING\n",
            msg.body.join_ack.registered_count);
        playerState = PLAYER_STATE_JOINING;
        return;
    }

    // WAIT_ACK 타임아웃 [R-JOIN-08]
    if (L3_event_checkEventFlag(L3_event_arqTimeout)) {
        L3_event_clearEventFlag(L3_event_arqTimeout);

        pc.printf("[Player] WAIT_ACK timeout. waiting 3s...\n");
        waitAckRetryPending = 1;
        retryTimer.attach(retryJoinCallback, 3.0f);
    }
}

static void retryJoinCallback(void)
{
    waitAckRetryPending = 0;
    pc.printf("[Player] retrying JOIN...\n");
    playerState = PLAYER_STATE_IDLE;
}

// -------------------------------------------------------
// State: JOINING
// 탈출: SETUP 수신 → PLAYING
//       TURN 수신 → 무시 [R-SETUP-06]
// -------------------------------------------------------
static void stateJoining(void)
{
    if (!L3_event_checkEventFlag(L3_event_msgRcvd)) {
        return;
    }
    L3_event_clearEventFlag(L3_event_msgRcvd);

    uint8_t* dataPtr = L3_LLI_getMsgPtr();
    uint8_t size = L3_LLI_getSize();

    L3MsgType type = L3_msg_peekType(dataPtr, size);

    // [R-SETUP-06] SETUP 전 TURN 무시
    if (type == L3_MSG_TURN) {
        pc.printf("[Player] JOINING: ignoring TURN before SETUP\n");
        return;
    }

    if (type != L3_MSG_SETUP) {
        return;
    }

    L3Message msg;
    if (!L3_msg_deserialize(dataPtr, size, &msg)) {
        return;
    }

    // [R-SETUP-03] 내부 숫자 초기화
    L3_369engine_reset();
    L3_369engine_init(myNickname);

    // [R-SETUP-04] player_order 확정
    L3_369engine_setPlayerOrder(
        (const char (*)[L3_MAX_NICKNAME_LEN])msg.body.setup.player_order,
        L3_MAX_PLAYERS
    );

    pc.printf("[Player] SETUP received. judge=%s -> PLAYING\n",
        msg.body.setup.judge_nickname);
    pc.printf("[Player] order: %s %s %s %s\n",
        msg.body.setup.player_order[0],
        msg.body.setup.player_order[1],
        msg.body.setup.player_order[2],
        msg.body.setup.player_order[3]
    );

    playerState = PLAYER_STATE_PLAYING;
}

// -------------------------------------------------------
// State: PLAYING
// 탈출: GAMEOVER 수신 → IDLE
// -------------------------------------------------------
static void statePlaying(void)
{
    if (!L3_event_checkEventFlag(L3_event_msgRcvd)) {
        return;
    }
    L3_event_clearEventFlag(L3_event_msgRcvd);

    uint8_t* dataPtr = L3_LLI_getMsgPtr();
    uint8_t size = L3_LLI_getSize();

    L3MsgType type = L3_msg_peekType(dataPtr, size);

    if (type == L3_MSG_TURN) {
        L3Message msg;
        if (!L3_msg_deserialize(dataPtr, size, &msg)) return;

        // 369 엔진에 TURN 전달
        L3_369engine_onTurnReceived(&msg.body.turn);

        pc.printf("[Player] TURN. player=%s currentNumber=%lu\n",
            msg.body.turn.player_nickname,
            (unsigned long)L3_369engine_getCurrentNumber()
        );

        // [R-TURN-04] 내 차례면 ANSWER 전송
        if (L3_369engine_isMyTurnNow()) {
            sendAnswer();
        } else {
            pc.printf("[Player] Not my turn. waiting...\n");
        }
        return;
    }

    // [R-GAMEOVER-04] GAMEOVER 수신 → IDLE
    if (type == L3_MSG_GAMEOVER) {
        L3Message msg;
        if (!L3_msg_deserialize(dataPtr, size, &msg)) return;

        pc.printf("[Player] GAMEOVER. eliminated=%s reason=%s -> IDLE\n",
            msg.body.gameover.eliminated_player_nickname,
            L3_elim_reason_to_string(msg.body.gameover.reason)
        );

        L3_369engine_reset();
        isJudgeKnown = 0;
        playerState = PLAYER_STATE_IDLE;
        return;
    }
}

// -------------------------------------------------------
// Send JOIN (broadcast) [R-JOIN-01]
// -------------------------------------------------------
static void sendJoin(void)
{
    L3Message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = L3_MSG_JOIN;
    strncpy(msg.body.join.node_nickname, myNickname, L3_MAX_NICKNAME_LEN - 1);

    uint8_t buf[L3_MSG_MAX_SERIAL_LEN];
    uint8_t len = L3_msg_serialize(&msg, buf, sizeof(buf));
    if (len == 0) return;

    L3_LLI_dataReqFunc(buf, len, L3_BROADCAST_ID);
    pc.printf("[Player] JOIN sent. nickname=%s\n", myNickname);
}

// -------------------------------------------------------
// Send ANSWER (브로드캐스트)
// -------------------------------------------------------
static void sendAnswer(void)
{
    char answer[L3_MAX_VALUE_LEN];
    L3_369engine_computeExpectedAnswerForCurrentTurn(answer, sizeof(answer));

    L3Message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = L3_MSG_ANSWER;
    strncpy(msg.body.answer.player_nickname, myNickname, L3_MAX_NICKNAME_LEN - 1);
    strncpy(msg.body.answer.value, answer, L3_MAX_VALUE_LEN - 1);

    uint8_t buf[L3_MSG_MAX_SERIAL_LEN];
    uint8_t len = L3_msg_serialize(&msg, buf, sizeof(buf));
    if (len == 0) return;

    L3_LLI_dataReqFunc(buf, len, L3_BROADCAST_ID);
    pc.printf("[Player] ANSWER sent. value=%s\n", answer);
}

// -------------------------------------------------------
// Getters
// -------------------------------------------------------
PlayerState L3_player_getState(void)
{
    return playerState;
}

const char* L3_player_getStateString(void)
{
    switch (playerState) {
        case PLAYER_STATE_IDLE:     return "IDLE";
        case PLAYER_STATE_WAIT_ACK: return "WAIT_ACK";
        case PLAYER_STATE_JOINING:  return "JOINING";
        case PLAYER_STATE_PLAYING:  return "PLAYING";
        default:                    return "UNKNOWN";
    }
}
