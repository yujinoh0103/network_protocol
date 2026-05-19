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
static char judgeNickname[L3_MAX_NICKNAME_LEN];
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
// 수신 메시지 type 파싱 헬퍼
// C팀 디코딩 함수 오면 교체 예정
// -------------------------------------------------------
static int parseReceivedMsg(L3Message* outMsg)
{
    uint8_t* dataPtr = L3_LLI_getMsgPtr();
    uint8_t size = L3_LLI_getSize();

    if (dataPtr == NULL || size == 0) {
        return 0;
    }

    char* typeStart = strstr((char*)dataPtr, "\"type\":\"");
    if (typeStart == NULL) return 0;

    typeStart += 8;
    char typeStr[16] = {0};
    int i = 0;
    while (typeStart[i] != '"' && typeStart[i] != '\0' && i < 15) {
        typeStr[i] = typeStart[i];
        i++;
    }
    outMsg->type = L3_string_to_msg_type(typeStr);

    return 1;
}

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

        L3Message msg;
        if (!parseReceivedMsg(&msg)) return;

        if (msg.type == L3_MSG_JOIN_ACK) {
            // [R-JOIN-11] 첫 JOIN_ACK 송신자를 Judge로 인지
            if (!isJudgeKnown) {
                isJudgeKnown = 1;
                pc.printf("[Player] Judge identified. srcId=%d\n", L3_LLI_getSrcId());
            }

            L3_timer_stopTimer();
            pc.printf("[Player] JOIN_ACK received -> JOINING\n");
            playerState = PLAYER_STATE_JOINING;
            return;
        }
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

    L3Message msg;
    if (!parseReceivedMsg(&msg)) return;

    if (msg.type == L3_MSG_SETUP) {
        // C팀 디코딩 함수 오면 player_order 파싱 후 setPlayerOrder 호출
        L3_369engine_reset();
        L3_369engine_init(myNickname);

        pc.printf("[Player] SETUP received -> PLAYING\n");
        playerState = PLAYER_STATE_PLAYING;
        return;
    }

    // [R-SETUP-06] SETUP 전 TURN 무시
    if (msg.type == L3_MSG_TURN) {
        pc.printf("[Player] JOINING: ignoring TURN before SETUP\n");
        return;
    }
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

    L3Message msg;
    if (!parseReceivedMsg(&msg)) return;

    if (msg.type == L3_MSG_TURN) {
        // player_nickname 파싱
        // C팀 디코딩 함수 오면 교체
        uint8_t* dataPtr = L3_LLI_getMsgPtr();
        char* nickStart = strstr((char*)dataPtr, "\"player_nickname\":\"");
        char turnNickname[L3_MAX_NICKNAME_LEN] = {0};

        if (nickStart != NULL) {
            nickStart += 19;
            int i = 0;
            while (nickStart[i] != '"' && nickStart[i] != '\0' && i < L3_MAX_NICKNAME_LEN - 1) {
                turnNickname[i] = nickStart[i];
                i++;
            }
        }

        L3TurnMsg turnMsg;
        strncpy(turnMsg.player_nickname, turnNickname, L3_MAX_NICKNAME_LEN - 1);
        turnMsg.player_nickname[L3_MAX_NICKNAME_LEN - 1] = '\0';
        L3_369engine_onTurnReceived(&turnMsg);

        pc.printf("[Player] TURN. player=%s currentNumber=%lu\n",
            turnNickname,
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
    if (msg.type == L3_MSG_GAMEOVER) {
        pc.printf("[Player] GAMEOVER received -> IDLE\n");
        L3_369engine_reset();
        isJudgeKnown = 0;
        playerState = PLAYER_STATE_IDLE;
        return;
    }
}

// -------------------------------------------------------
// Send JOIN (broadcast) [R-JOIN-01]
// C팀 인코딩 함수 오면 교체
// -------------------------------------------------------
static void sendJoin(void)
{
    char buf[64];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"JOIN\",\"node_nickname\":\"%s\"}",
        myNickname
    );
    L3_LLI_dataReqFunc((uint8_t*)buf, strlen(buf) + 1, L3_BROADCAST_ID);
    pc.printf("[Player] JOIN sent. nickname=%s\n", myNickname);
}

// -------------------------------------------------------
// Send ANSWER (브로드캐스트)
// C팀 인코딩 함수 오면 교체
// -------------------------------------------------------
static void sendAnswer(void)
{
    char answer[L3_MAX_VALUE_LEN];
    L3_369engine_computeExpectedAnswerForCurrentTurn(answer, sizeof(answer));

    char buf[80];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"ANSWER\",\"player_nickname\":\"%s\",\"value\":\"%s\"}",
        myNickname, answer
    );
    L3_LLI_dataReqFunc((uint8_t*)buf, strlen(buf) + 1, L3_BROADCAST_ID);
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
