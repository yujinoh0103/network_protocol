#include "mbed.h"
#include "L3_FSMmain_player.h"
#include "L3_FSMmain.h"
#include "L3_FSMmain_judge.h"
#include "L3_369engine.h"
#include "L3_msg.h"
#include "L3_FSMevent.h"
#include "L3_LLinterface.h"
#include "L3_timer.h"
#include "protocol_parameters.h"
#include <string.h>

extern Serial pc;

// -------------------------------------------------------
// 내부 상태 변수
// -------------------------------------------------------
static PlayerState playerState  = PLAYER_STATE_IDLE;
static char myNickname[L3_MAX_NICKNAME_LEN];
static uint8_t isJudgeKnown     = 0;
static uint8_t lastRegisteredCount = 0;
static uint8_t joiningWaitPrinted  = 0;

// WAIT_ACK 재전송 관리
static uint8_t waitAckRetryPending = 0;
static uint8_t joinRetryCount      = 0;
static Timeout retryTimer;
static Timer judgeWaitTimer;
static uint8_t judgeWaitTimerActive = 0;
static uint8_t joiningProbePending = 0;

// -------------------------------------------------------
// Forward declarations
// -------------------------------------------------------
static void stateIdle(void);
static void stateWaitAck(void);
static void stateJoining(void);
static void statePlaying(void);
static void sendJoin(void);
static void sendAnswer(const char* answer);
static void retryJoinCallback(void);
static void printJoiningWaitStatus(void);
static void resetPlayerAfterJudgeLost(void);
static void handleJoinAbort(void);
static uint8_t enterPlayingFromSetup(const L3Message* msg);
static void trimNickname(char* nickname);
static uint8_t isMyNickname(const char* nickname);

// -------------------------------------------------------
// 초기화
// 키보드 RxIrq 핸들러는 L3_FSMmain.cpp의 L3service_processInputWord가 담당.
// 여기서는 상태 변수만 초기화하고 입력 프롬프트를 출력.
// -------------------------------------------------------
void L3_player_initFSM(void)
{
    playerState         = PLAYER_STATE_IDLE;
    myNickname[0]       = '\0';
    isJudgeKnown        = 0;
    waitAckRetryPending = 0;
    joinRetryCount      = 0;
    judgeWaitTimerActive = 0;
    joiningProbePending = 0;
    judgeWaitTimer.stop();

    L3_clearInputWord(); // 혹시 남아있는 입력 버퍼 클리어

    pc.printf("[Player] Enter your nickname and press Enter:\r\n> ");
}

// -------------------------------------------------------
// 메인 루프에서 매번 호출
// -------------------------------------------------------
void L3_player_runFSM(void)
{
    switch (playerState) {
        case PLAYER_STATE_IDLE:     stateIdle();     break;
        case PLAYER_STATE_WAIT_ACK: stateWaitAck();  break;
        case PLAYER_STATE_JOINING:  stateJoining();  break;
        case PLAYER_STATE_PLAYING:  statePlaying();  break;
        case PLAYER_STATE_HALTED:   L3_clearInputWord(); break;
        default: break;
    }
}

// -------------------------------------------------------
// State: IDLE
// 역할: L3service_processInputWord가 Enter 감지 시 L3_event_dataToSend를 set.
//       이 플래그를 보고 닉네임을 확정한 뒤 JOIN 전송 → WAIT_ACK
// [R-JOIN-01][R-JOIN-02][R-JOIN-07]
// -------------------------------------------------------
static void stateIdle(void)
{
    if (!L3_event_checkEventFlag(L3_event_dataToSend)) return; // 아직 Enter 안 눌림

    // 닉네임 확정
    const char* word = L3_getInputWord();
    uint8_t     len  = L3_getInputWordLen();

    if (len == 0) {
        // 빈 입력 → 다시 요청
        L3_clearInputWord();
        pc.printf("[Player] Nickname cannot be empty. Try again:\r\n> ");
        return;
    }

    strncpy(myNickname, word, L3_MAX_NICKNAME_LEN - 1);
    myNickname[L3_MAX_NICKNAME_LEN - 1] = '\0';
    trimNickname(myNickname);
    L3_clearInputWord();

    pc.printf("[Player] Nickname set: %s. Sending JOIN...\r\n", myNickname);

    // JOIN 전송 + WAIT_ACK 타이머 시작 [R-JOIN-07]
    sendJoin();
    joinRetryCount = 0;
    L3_timer_startTimer();

    playerState = PLAYER_STATE_WAIT_ACK;
}

// -------------------------------------------------------
// State: WAIT_ACK
// 역할: JOIN_ACK 대기. 타임아웃 시 최대 3회 재전송 [R-JOIN-08][R-JOIN-13]
// -------------------------------------------------------
static void stateWaitAck(void)
{
    if (waitAckRetryPending) return; // retryTimer 대기 중

    if (L3_event_checkEventFlag(L3_event_dataSendCnf)) {
        L3_event_clearEventFlag(L3_event_dataSendCnf);
        if (!L3_LLI_getLastDataCnfResult()) {
            pc.printf("[Player] Wireless link failed. L2 ACK was not received.\r\n");
            pc.printf("[Player] Check radio/link, then reset this board and try again.\r\n");
            L3_timer_stopTimer();
            resetPlayerAfterJudgeLost();
            return;
        }
    }

    // JOIN_ACK 수신 확인
    if (L3_event_checkEventFlag(L3_event_msgRcvd)) {
        L3_event_clearEventFlag(L3_event_msgRcvd);

        uint8_t* dataPtr = L3_LLI_getMsgPtr();
        uint8_t  size    = L3_LLI_getSize();
        L3MsgType type    = L3_msg_peekType(dataPtr, size);

        if (type != L3_MSG_JOIN_ACK &&
            type != L3_MSG_SETUP &&
            type != L3_MSG_JOIN_ABORT) {
            return;
        }

        L3Message msg;
        if (!L3_msg_deserialize(dataPtr, size, &msg)) return;

        if (type == L3_MSG_JOIN_ABORT) {
            handleJoinAbort();
            return;
        }

        // JOIN_ACK를 놓친 노드도 SETUP broadcast에 포함되어 있으면 게임에 합류한다.
        if (type == L3_MSG_SETUP) {
            if (enterPlayingFromSetup(&msg)) {
                L3_timer_stopTimer();
            }
            return;
        }

        // [R-JOIN-10] 닉네임 일치 확인
        if (strncmp(msg.body.join_ack.node_nickname,
                    myNickname, L3_MAX_NICKNAME_LEN) != 0) {
            return; // 내 ACK 아님 → 무시
        }

        // [R-JOIN-11] 첫 JOIN_ACK 송신 노드를 Judge로 인지
        if (!isJudgeKnown) {
            isJudgeKnown = 1;
            pc.printf("[Player] Judge identified. srcId=%d\r\n", L3_LLI_getSrcId());
        }

        L3_timer_stopTimer();
        if (msg.body.join_ack.registered_count == L3_JOIN_REJECT_NODE_ID) {
            pc.printf("[Player] Node number is already registered.\r\n");
            pc.printf("[Player] Reset this board and enter another node number.\r\n");
            myNickname[0] = '\0';
            joinRetryCount = 0;
            waitAckRetryPending = 0;
            L3_clearInputWord();
            playerState = PLAYER_STATE_HALTED;
            return;
        }
        if (msg.body.join_ack.registered_count == L3_JOIN_REJECT_NICKNAME) {
            pc.printf("[Player] Nickname is already registered.\r\n");
            pc.printf("[Player] Enter another nickname:\r\n> ");
            myNickname[0] = '\0';
            joinRetryCount = 0;
            waitAckRetryPending = 0;
            playerState = PLAYER_STATE_IDLE;
            L3_clearInputWord();
            return;
        }

        pc.printf("[Player] JOIN_ACK received. registered_count=%d -> JOINING\r\n",
                  msg.body.join_ack.registered_count);
        lastRegisteredCount = msg.body.join_ack.registered_count;
        joiningWaitPrinted = 0;
        joiningProbePending = 0;
        judgeWaitTimer.reset();
        judgeWaitTimer.start();
        judgeWaitTimerActive = 1;
        playerState = PLAYER_STATE_JOINING;
        return;
    }

    // WAIT_ACK 타임아웃 [R-JOIN-08][R-JOIN-13] (최대 3회)
    if (L3_event_checkEventFlag(L3_event_arqTimeout)) {
        L3_event_clearEventFlag(L3_event_arqTimeout);

        joinRetryCount++;
        if (joinRetryCount >= L3_JOIN_MAX_RETRY) {
            pc.printf("[Player] Judge is not responding during join/rejoin.\r\n");
            pc.printf("[Player] Reset this board and wait for the Judge to restart.\r\n");
            L3_timer_stopTimer();
            resetPlayerAfterJudgeLost();
            return;
        }

        pc.printf("[Player] Judge is not responding yet. Retrying JOIN (%d/3)...\r\n",
                  joinRetryCount);
        waitAckRetryPending = 1;
        retryTimer.attach(&retryJoinCallback, (float)L3_JOIN_RETRY_DELAY_SEC); // [R-JOIN-08] 3초 대기 후 재전송
    }
}

// 3초 대기 후 IDLE 복귀 → 기존 닉네임으로 즉시 재전송
static void retryJoinCallback(void)
{
    waitAckRetryPending = 0;
    pc.printf("[Player] Retrying JOIN with nickname: %s\r\n", myNickname);

    // L3_event_dataToSend를 수동으로 set하여 IDLE에서 바로 닉네임 재사용
    // (originalWord는 이미 cleared 상태이므로 myNickname을 직접 사용)
    sendJoin();
    L3_timer_startTimer();
    // WAIT_ACK 상태 유지 (IDLE 거치지 않고 바로 재시도)
    playerState = PLAYER_STATE_WAIT_ACK;
}

// -------------------------------------------------------
// State: JOINING
// 역할: SETUP 대기. TURN은 SETUP 전에 무시 [R-SETUP-06]
// -------------------------------------------------------
static void stateJoining(void)
{
    if (!joiningWaitPrinted) {
        printJoiningWaitStatus();
        joiningWaitPrinted = 1;
    }

    if (!L3_event_checkEventFlag(L3_event_msgRcvd)) {
        if (judgeWaitTimerActive &&
            judgeWaitTimer.read_ms() >= (L3_SETUP_WAIT_TIMEOUT_SEC * 1000)) {
            if (lastRegisteredCount < L3_JUDGE_MAX_PARTICIPANTS) {
                if (joiningProbePending) {
                    pc.printf("[Player] Judge is not responding while waiting for players.\r\n");
                    pc.printf("[Player] Reset this board and wait for the Judge to restart.\r\n");
                    resetPlayerAfterJudgeLost();
                } else {
                    pc.printf("[Player] Checking Judge status while waiting for players...\r\n");
                    sendJoin();
                    joiningProbePending = 1;
                    judgeWaitTimer.reset();
                    judgeWaitTimer.start();
                }
            } else {
                pc.printf("[Player] Judge is not responding before game setup.\r\n");
                pc.printf("[Player] Reset this board and wait for the Judge to restart.\r\n");
                resetPlayerAfterJudgeLost();
            }
        }
        return;
    }
    L3_event_clearEventFlag(L3_event_msgRcvd);

    uint8_t*  dataPtr = L3_LLI_getMsgPtr();
    uint8_t   size    = L3_LLI_getSize();
    L3MsgType type    = L3_msg_peekType(dataPtr, size);

    // [R-SETUP-06] SETUP 이전 TURN 무시
    if (type == L3_MSG_TURN) {
        pc.printf("[Player] JOINING: ignoring TURN before SETUP\r\n");
        return;
    }

    if (type == L3_MSG_JOIN_ABORT) {
        handleJoinAbort();
        return;
    }

    if (type == L3_MSG_JOIN_ACK) {
        L3Message msg;
        if (!L3_msg_deserialize(dataPtr, size, &msg)) return;
        if (strncmp(msg.body.join_ack.node_nickname,
                    myNickname, L3_MAX_NICKNAME_LEN) != 0) {
            return;
        }

        lastRegisteredCount = msg.body.join_ack.registered_count;
        joiningProbePending = 0;
        joiningWaitPrinted = 0;
        judgeWaitTimer.reset();
        judgeWaitTimer.start();
        judgeWaitTimerActive = 1;

        pc.printf("[Player] Judge status updated. registered_count=%d\r\n",
                  lastRegisteredCount);
        printJoiningWaitStatus();
        joiningWaitPrinted = 1;
        return;
    }

    if (type != L3_MSG_SETUP) return;

    L3Message msg;
    if (!L3_msg_deserialize(dataPtr, size, &msg)) return;

    if (enterPlayingFromSetup(&msg)) {
        judgeWaitTimerActive = 0;
        judgeWaitTimer.stop();
    }
}

static void printJoiningWaitStatus(void)
{
    if (lastRegisteredCount < L3_JUDGE_MAX_PARTICIPANTS) {
        uint8_t remainingPlayers = L3_JUDGE_MAX_PARTICIPANTS - lastRegisteredCount;
        pc.printf("[Player] Waiting for other players... (%d/%d joined, %d more)\r\n",
                  lastRegisteredCount,
                  L3_JUDGE_MAX_PARTICIPANTS,
                  remainingPlayers);
    } else {
        pc.printf("[Player] All players joined. Waiting for game setup...\r\n");
    }
}

static uint8_t enterPlayingFromSetup(const L3Message* msg)
{
    if (msg == NULL || msg->type != L3_MSG_SETUP) {
        return 0;
    }

    uint8_t foundMe = 0;
    for (uint8_t i = 0; i < L3_MAX_PLAYERS; i++) {
        if (strncmp(msg->body.setup.player_order[i],
                    myNickname,
                    L3_MAX_NICKNAME_LEN) == 0) {
            foundMe = 1;
            break;
        }
    }

    if (!foundMe) {
        pc.printf("[Player] SETUP received, but my nickname is not in order. Ignoring.\r\n");
        return 0;
    }

    // [R-SETUP-03] 내부 숫자 초기화 / [R-SETUP-04] 순번 확정
    L3_369engine_reset();
    L3_369engine_init(myNickname);
    L3_369engine_setPlayerOrder(
        (const char (*)[L3_MAX_NICKNAME_LEN])msg->body.setup.player_order,
        L3_MAX_PLAYERS
    );

    pc.printf("[Player] SETUP received. judge=%s\r\n", msg->body.setup.judge_nickname);
    pc.printf("[Player] notice: %s\r\n", msg->body.setup.notice);
    pc.printf("[Player] order: %s / %s / %s / %s\r\n",
              msg->body.setup.player_order[0],
              msg->body.setup.player_order[1],
              msg->body.setup.player_order[2],
              msg->body.setup.player_order[3]);

    L3_clearInputWord(); // PLAYING에서 답변 입력받을 준비

    playerState = PLAYER_STATE_PLAYING;
    return 1;
}

static void trimNickname(char* nickname)
{
    if (nickname == NULL) return;

    uint8_t len = strlen(nickname);
    while (len > 0 &&
           (nickname[len - 1] == ' ' ||
            nickname[len - 1] == '\t' ||
            nickname[len - 1] == '\r' ||
            nickname[len - 1] == '\n')) {
        nickname[len - 1] = '\0';
        len--;
    }
}

static uint8_t isMyNickname(const char* nickname)
{
    if (nickname == NULL) return 0;
    return (strncmp(nickname, myNickname, L3_MAX_NICKNAME_LEN) == 0) ? 1 : 0;
}

// -------------------------------------------------------
// State: PLAYING
// 역할: TURN 수신 → 키보드 입력 시 ANSWER 전송
//       GAMEOVER 수신 → IDLE 복귀
// [R-TURN-04][R-TURN-05][R-GAMEOVER-04]
// -------------------------------------------------------
static void statePlaying(void)
{
    // PLAYING 중 입력이 들어오면 Judge에게 전송한다.
    // 내 턴이 아니면 Judge가 OUT_OF_TURN으로 판정한다.
    if (L3_event_checkEventFlag(L3_event_dataToSend)) {
        const char* answer = L3_getInputWord();
        if (!isMyNickname(L3_369engine_getCurrentTurnPlayer())) {
            pc.printf("[Player] Out-of-turn input. Sending ANSWER for Judge validation.\r\n");
        }
        sendAnswer(answer);
        L3_clearInputWord();
    }

    if (!L3_event_checkEventFlag(L3_event_msgRcvd)) return;
    L3_event_clearEventFlag(L3_event_msgRcvd);

    uint8_t*  dataPtr = L3_LLI_getMsgPtr();
    uint8_t   size    = L3_LLI_getSize();
    L3MsgType type    = L3_msg_peekType(dataPtr, size);

    if (type == L3_MSG_TURN) {
        L3Message msg;
        if (!L3_msg_deserialize(dataPtr, size, &msg)) return;

        // 369 엔진에 TURN 전달 → 내부 숫자 증가, 현재 턴 플레이어 갱신
        L3_369engine_onTurnReceived(&msg.body.turn);

        if (isMyNickname(msg.body.turn.player_nickname)) {
            L3_clearInputWord(); // 이전 입력 클리어
            pc.printf("[Player] *** MY TURN! *** Enter answer and press Enter:\r\n> ");
        } else {
            pc.printf("[Player] Not my turn. Waiting...\r\n");
        }
        return;
    }

    // [R-GAMEOVER-04] GAMEOVER 수신 → IDLE 복귀
    if (type == L3_MSG_GAMEOVER) {
        L3Message msg;
        if (!L3_msg_deserialize(dataPtr, size, &msg)) return;

        pc.printf(
            "=================================\r\n"
            "*                   GAME OVER!                   *\r\n"
            "=================================\r\n"
            "\r\n"
            "        Oh no! A player has been eliminated.\r\n"
            "        Let's take a tiny deep breath... :')\r\n"
            "\r\n"
            "---------------------------------\r\n"
            "* RESULT\r\n"
            "---------------------------------\r\n"
            "\r\n"
            "  Eliminated player : %s\r\n"
            "  Reason            : %s\r\n"
            "\r\n"
            "---------------------------------\r\n"
            "\r\n"
            "        * Moving to retry mode... *\r\n"
            "          New nickname, new chance! :D\r\n"
            "\r\n"
            "=================================\r\n",
            msg.body.gameover.eliminated_player_nickname,
            L3_elim_reason_to_string(msg.body.gameover.reason));

        L3_369engine_reset();
        isJudgeKnown        = 0;
        joinRetryCount      = 0;
        waitAckRetryPending = 0;
        judgeWaitTimerActive = 0;
        judgeWaitTimer.stop();
        myNickname[0]       = '\0';
        L3_clearInputWord();

        playerState = PLAYER_STATE_IDLE;
        pc.printf("[Player] Enter your nickname to rejoin:\r\n> ");
        return;
    }
}

static void resetPlayerAfterJudgeLost(void)
{
    L3_369engine_reset();
    L3_timer_stopTimer();
    isJudgeKnown         = 0;
    joinRetryCount       = 0;
    waitAckRetryPending  = 0;
    judgeWaitTimerActive = 0;
    judgeWaitTimer.stop();
    joiningProbePending = 0;
    myNickname[0]        = '\0';
    L3_clearInputWord();
    playerState = PLAYER_STATE_IDLE;
}

static void handleJoinAbort(void)
{
    pc.printf("[Player] Not enough players joined in time.\r\n");
    pc.printf("[Player] Reset this board and wait for a new game setup.\r\n");
    L3_timer_stopTimer();
    L3_369engine_reset();
    isJudgeKnown         = 0;
    joinRetryCount       = 0;
    waitAckRetryPending  = 0;
    judgeWaitTimerActive = 0;
    joiningProbePending  = 0;
    judgeWaitTimer.stop();
    myNickname[0]        = '\0';
    L3_clearInputWord();
    playerState = PLAYER_STATE_IDLE;
}

// -------------------------------------------------------
// JOIN 전송 (Judge unicast) [R-JOIN-01][R-JOIN-02]
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

    L3_LLI_dataReqFunc(buf, len, L3_JUDGE_NODE_ID);
    pc.printf("[Player] JOIN sent. nickname=%s\r\n", myNickname);
}

// -------------------------------------------------------
// ANSWER 전송 (Judge unicast) [R-TURN-04]
// -------------------------------------------------------
static void sendAnswer(const char* answer)
{
    L3Message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = L3_MSG_ANSWER;
    strncpy(msg.body.answer.player_nickname, myNickname, L3_MAX_NICKNAME_LEN - 1);
    strncpy(msg.body.answer.value, answer, L3_MAX_VALUE_LEN - 1);

    uint8_t buf[L3_MSG_MAX_SERIAL_LEN];
    uint8_t len = L3_msg_serialize(&msg, buf, sizeof(buf));
    if (len == 0) return;

    L3_LLI_dataReqFunc(buf, len, L3_JUDGE_NODE_ID);
    pc.printf("[Player] ANSWER sent. value=%s\r\n", answer);
}

// -------------------------------------------------------
// Getter
// -------------------------------------------------------
PlayerState L3_player_getState(void) { return playerState; }

const char* L3_player_getStateString(void)
{
    switch (playerState) {
        case PLAYER_STATE_IDLE:     return "IDLE";
        case PLAYER_STATE_WAIT_ACK: return "WAIT_ACK";
        case PLAYER_STATE_JOINING:  return "JOINING";
        case PLAYER_STATE_PLAYING:  return "PLAYING";
        case PLAYER_STATE_HALTED:   return "HALTED";
        default:                    return "UNKNOWN";
    }
}
