#include "mbed.h"
#include "L3_FSMmain_judge.h"
#include "L3_FSMevent.h"
#include "L3_LLinterface.h"
#include "L3_msg.h"
#include "protocol_parameters.h"
#include "L3_369engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern Serial pc;


// Judge state (private to this translation unit)
static uint8_t judge_state = L3_JUDGE_STATE_IDLE;
static uint8_t judge_participant_count = 0;
static char    judge_participants[L3_JUDGE_MAX_PARTICIPANTS][L3_MAX_NICKNAME_LEN];
static uint8_t current_turn_player_idx = 0;

// 송신(TX) buffer
static uint8_t judge_txBuf[L3_MAXDATASIZE];


// IDLE 상태 초기화
void L3_judge_initIDLE(void)
{
    judge_participant_count = 0;
    current_turn_player_idx = 0;
    L3_369engine_reset();

    for (int i = 0; i < L3_JUDGE_MAX_PARTICIPANTS; i++) {
        memset(judge_participants[i], 0, L3_MAX_NICKNAME_LEN);
    }

    judge_state = L3_JUDGE_STATE_IDLE;
    L3_event_clearEventFlag(L3_event_dataSendCnf); // 이전 게임의 잔류 이벤트 제거

    debug_if(DBGMSG_L3,
             "[Judge] IDLE initialized. node_id=%u, waiting for JOIN...\n",
             (unsigned)L3_JUDGE_NODE_ID);
}

// State accessors
uint8_t L3_judge_getCurrentState(void) { return judge_state; } 
uint8_t L3_judge_getParticipantCount(void) { return judge_participant_count; }

void L3_judge_setCurrentState(uint8_t newState)
{
    if (judge_state != newState) {
        debug_if(DBGMSG_L3,
                 "[Judge] State transition: %u -> %u\n",
                 (unsigned)judge_state, (unsigned)newState);
        judge_state = newState;
    }
}

const char* L3_judge_getParticipantNickname(uint8_t index)
{
    if (index >= L3_JUDGE_MAX_PARTICIPANTS) {
        return NULL;
    }
    return judge_participants[index];
}

// 닉네임 중복 체크 (1=등록됨, 0=미등록)
int L3_judge_isNicknameRegistered(const char* nickname)
{
    if (nickname == NULL || nickname[0] == '\0') {
        return 0;
    }
    for (uint8_t i = 0; i < judge_participant_count; i++) {
        if (strncmp(judge_participants[i], nickname, L3_MAX_NICKNAME_LEN) == 0) {
            return 1;
        }
    }
    return 0;
}

// 참가자 등록 (1=성공, 0=실패)
int L3_judge_addParticipant(const char* nickname)
{
    if (nickname == NULL || nickname[0] == '\0') {
        return 0;
    }
    if (L3_judge_isNicknameRegistered(nickname)) {
        return 0;
    }
    // 최대 인원 초과 시 등록 실패 
    if (judge_participant_count >= L3_JUDGE_MAX_PARTICIPANTS) {
        return 0;
    }

    strncpy(judge_participants[judge_participant_count],
            nickname,
            L3_MAX_NICKNAME_LEN - 1);
    judge_participants[judge_participant_count][L3_MAX_NICKNAME_LEN - 1] = '\0';
    judge_participant_count++;
    return 1;
}

// JOIN_ACK 메시지 빌더
void L3_judge_buildJoinAck(L3Message* out, const char* nickname, uint8_t count)
{
    if (out == NULL) return;
    memset(out, 0, sizeof(*out));
    out->type = L3_MSG_JOIN_ACK;
    if (nickname != NULL) {
        strncpy(out->body.join_ack.node_nickname, nickname, L3_MAX_NICKNAME_LEN - 1);
    }
    out->body.join_ack.registered_count = count;
}

// Setup 메시지 빌더
int L3_judge_buildSetup(L3Message* out)
{
    if (out == NULL) return 0;

    if (judge_participant_count != L3_JUDGE_MAX_PARTICIPANTS) {
        return 0;
    }

    memset(out, 0, sizeof(*out));
    out->type = L3_MSG_SETUP;

    strncpy(out->body.setup.judge_nickname,
            L3_JUDGE_NICKNAME_STR,
            L3_MAX_NICKNAME_LEN - 1);

    
    for (int i = 0; i < L3_MAX_PLAYERS; i++) {
        if (i < L3_JUDGE_MAX_PARTICIPANTS) {
            strncpy(out->body.setup.player_order[i],
                    judge_participants[i],
                    L3_MAX_NICKNAME_LEN - 1);
        } else {
            out->body.setup.player_order[i][0] = '\0';
        }
    }

    strncpy(out->body.setup.notice, L3_SETUP_NOTICE, L3_MAX_NOTICE_LEN - 1);
    return 1;
}

// First TURN 메시지 빌더
int L3_judge_buildFirstTurn(L3Message* out)
{
    if (out == NULL || judge_participant_count == 0) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    out->type = L3_MSG_TURN;
    strncpy(out->body.turn.player_nickname,
            judge_participants[0],
            L3_MAX_NICKNAME_LEN - 1);
    out->body.turn.timeout_sec = L3_JUDGE_FIRST_TURN_TIMEOUT_SEC;
    return 1;
}

// 메시지 송신 헬퍼
static uint8_t judge_sendMessage(const L3Message* msg, uint8_t destId)
{
    uint8_t size = L3_msg_serialize(msg, judge_txBuf, L3_MAXDATASIZE);
    if (size == 0) {
        debug_if(DBGMSG_L3, "[Judge] serialize failed for type=%d\n", (int)msg->type);
        return 0;
    }
    if (L3_LLI_dataReqFunc != NULL) {
        L3_LLI_dataReqFunc(judge_txBuf, size, destId);
    }
    return size;
}


// IDLE 상태 이벤트 핸들러
void L3_judge_handleIDLE(void)
{
    static uint8_t init_step = 0;

    if (init_step == 1) {
        // JOIN_ACK 전송 완료 대기
        if (L3_event_checkEventFlag(L3_event_dataSendCnf)) {
            L3_event_clearEventFlag(L3_event_dataSendCnf);

            // 이제 SETUP 브로드캐스트 송신
            L3Message setupMsg;
            if (L3_judge_buildSetup(&setupMsg)) {
                judge_sendMessage(&setupMsg, L3_BROADCAST_ID);
                debug_if(DBGMSG_L3,
                         "[Judge] SETUP broadcast: %s,%s,%s,%s\n",
                         setupMsg.body.setup.player_order[0],
                         setupMsg.body.setup.player_order[1],
                         setupMsg.body.setup.player_order[2],
                         setupMsg.body.setup.player_order[3]);
            }
            init_step = 2;
        }
        return;
    } else if (init_step == 2) {
        // SETUP 전송 완료 대기
        if (L3_event_checkEventFlag(L3_event_dataSendCnf)) {
            L3_event_clearEventFlag(L3_event_dataSendCnf);

            // 이제 첫 번째 TURN 브로드캐스트 송신
            L3Message turnMsg;
            if (L3_judge_buildFirstTurn(&turnMsg)) {
                judge_sendMessage(&turnMsg, L3_BROADCAST_ID);
                
                // Judge 내부의 369엔진도 턴을 추적하도록 동기화
                L3_369engine_onTurnReceived(&turnMsg.body.turn);

                debug_if(DBGMSG_L3,
                         "[Judge] first TURN -> '%s' (timeout=%us)\n",
                         turnMsg.body.turn.player_nickname,
                         (unsigned)turnMsg.body.turn.timeout_sec);
            }

            // 모든 초기화 전송 완료 -> RUNNING 상태 전환
            L3_judge_setCurrentState(L3_JUDGE_STATE_RUNNING);
            init_step = 0;
            debug_if(DBGMSG_L3,
                     "[Judge] IDLE -> RUNNING. Game Phase entered.\n");
        }
        return;
    }

    // 메시지 수신 이벤트가 없으면 무시
    if (!L3_event_checkEventFlag(L3_event_msgRcvd)) {
        return;
    }

    // 수신된 프레임의 Raw 데이터와 메타데이터를 가져옴
    uint8_t* rxPtr  = L3_LLI_getMsgPtr();
    uint8_t  rxSize = L3_LLI_getSize();
    uint8_t  srcId  = L3_LLI_getSrcId();

    L3_event_clearEventFlag(L3_event_msgRcvd);

    if (rxPtr == NULL || rxSize == 0) {
        return;
    }

    // 수신된 데이터를 L3Message 구조체로 역직렬화
    L3Message inMsg;
    if (!L3_msg_deserialize(rxPtr, rxSize, &inMsg)) {
        debug_if(DBGMSG_L3, "[L3_Judge] IDLE: malformed frame (size=%u) — drop\n", rxSize);
        return;
    }

    // JOIN 메시지만 처리
    if (inMsg.type != L3_MSG_JOIN) {
        debug_if(DBGMSG_L3,
                 "[Judge] IDLE: non-JOIN type=%s — drop\n",
                 L3_msg_type_to_string(inMsg.type));
        return; // 다른 메시지 타입 -> 드롭
    }

    const char* nickname = inMsg.body.join.node_nickname;

    // 유효한 닉네임 체크
    if (nickname[0] == '\0') {
        debug_if(DBGMSG_L3, "[Judge] empty nickname — drop\n");
        return; // 빈 닉네임 -> 드롭
    }

    // 중복 닉네임 체크
    if (L3_judge_isNicknameRegistered(nickname)) {
        debug_if(DBGMSG_L3,
                 "[Judge] duplicate nickname '%s' — silently drop\n",
                 nickname);
        return; // 중복 닉네임 -> 드롭
    }

    // 참가자 수 초과 체크
    if (judge_participant_count >= L3_JUDGE_MAX_PARTICIPANTS) {
        debug_if(DBGMSG_L3,
                 "[Judge] table full — drop JOIN from '%s'\n",
                 nickname);
        return; // 최대 참가자 수 초과 -> 드롭
    }

    // 모든 검증 통과 — 참가자 등록 진행
    if (!L3_judge_addParticipant(nickname)) {
        debug_if(DBGMSG_L3, "[Judge] addParticipant failed for '%s'\n", nickname);
        return;
    }

    debug_if(DBGMSG_L3,
             "[Judge] registered '%s' (#%u)\n",
             nickname, (unsigned)judge_participant_count);

    {
        // JOIN_ACK 메시지 송신
        L3Message ack;
        L3_judge_buildJoinAck(&ack, nickname, judge_participant_count);
        judge_sendMessage(&ack, srcId);
        debug_if(DBGMSG_L3,
                 "[Judge] JOIN_ACK -> '%s' (count=%u, destL2=%u)\n",
                 nickname, (unsigned)judge_participant_count, (unsigned)srcId);
    }

    if (judge_participant_count < L3_JUDGE_MAX_PARTICIPANTS) {
        return; // 참가자 수가 아직 4명 미만 -> 계속 JOIN 대기
    }

    // 참가자 모두 등록 완료 — JOIN 단계 종료, Game Phase 단계 진입
    debug_if(DBGMSG_L3,
             "[Judge] Join Phase complete (%d participants).\n",
             L3_JUDGE_MAX_PARTICIPANTS);

    // 바로 SETUP을 보내면 L2 버퍼에서 방금 보낸 JOIN_ACK를 덮어쓰게 되므로,
    // 보조 변수 init_step을 1로 바꾸어 다음 전송 완료(dataSendCnf)를 대기
    init_step = 1;
}

void L3_judge_handleRUNNING(void)
{
    // 데이터 전송 완료 이벤트는 무시
    if (L3_event_checkEventFlag(L3_event_dataSendCnf)) {
        L3_event_clearEventFlag(L3_event_dataSendCnf);
    }

    // 현재 턴 timeout 확인
    if (L3_369engine_isTurnTimedOut()) {
        const char* expectedPlayer = L3_369engine_getCurrentTurnPlayer();

        debug_if(DBGMSG_L3,
                 "[L3_Judge] TIMEOUT! Player '%s'. Game Over.\n",
                 expectedPlayer);

        L3Message go;
        memset(&go, 0, sizeof(go));
        go.type = L3_MSG_GAMEOVER;
        strncpy(go.body.gameover.eliminated_player_nickname,
                expectedPlayer,
                L3_MAX_NICKNAME_LEN - 1);
        go.body.gameover.reason = L3_REASON_TIMEOUT;

        judge_sendMessage(&go, L3_BROADCAST_ID);
        L3_judge_initIDLE();
        return;
    }

    // 메시지 수신 이벤트 확인
    if (!L3_event_checkEventFlag(L3_event_msgRcvd)) {
        return;
    }

    uint8_t* rxPtr  = L3_LLI_getMsgPtr();
    uint8_t  rxSize = L3_LLI_getSize();
    L3_event_clearEventFlag(L3_event_msgRcvd);

    L3Message inMsg;
    if (!L3_msg_deserialize(rxPtr, rxSize, &inMsg)) return;

    if (inMsg.type != L3_MSG_ANSWER) return;

    const char* sender = inMsg.body.answer.player_nickname;
    const char* value = inMsg.body.answer.value;
    const char* expectedPlayer = L3_369engine_getCurrentTurnPlayer();

    debug_if(DBGMSG_L3, "[Judge] ANSWER received from '%s', value='%s'\n", sender, value);

    // 1. 차례 확인
    if (strncmp(sender, expectedPlayer, L3_MAX_NICKNAME_LEN) != 0) {
        debug_if(DBGMSG_L3, "[Judge] OUT_OF_TURN! Expected '%s'. Game Over.\n", expectedPlayer);
        L3Message go;
        memset(&go, 0, sizeof(go));
        go.type = L3_MSG_GAMEOVER;
        strncpy(go.body.gameover.eliminated_player_nickname, sender, L3_MAX_NICKNAME_LEN - 1);
        go.body.gameover.reason = L3_REASON_OUT_OF_TURN;
        judge_sendMessage(&go, L3_BROADCAST_ID);
        L3_judge_initIDLE();
        return;
    }

    // 2. 정답 확인
    if (!L3_369engine_isAnswerCorrect(value)) {
        debug_if(DBGMSG_L3, "[Judge] WRONG_ANSWER! Game Over.\n");
        L3Message go;
        memset(&go, 0, sizeof(go));
        go.type = L3_MSG_GAMEOVER;
        strncpy(go.body.gameover.eliminated_player_nickname, sender, L3_MAX_NICKNAME_LEN - 1);
        go.body.gameover.reason = L3_REASON_WRONG_ANSWER;
        judge_sendMessage(&go, L3_BROADCAST_ID);
        L3_judge_initIDLE();
        return;
    }

    // 3. 정답일 경우: 다음 턴 진행
    debug_if(DBGMSG_L3, "[Judge] CORRECT! Advancing turn...\n");
    current_turn_player_idx = (current_turn_player_idx + 1) % judge_participant_count;

    L3Message turnMsg;
    memset(&turnMsg, 0, sizeof(turnMsg));
    turnMsg.type = L3_MSG_TURN;
    strncpy(turnMsg.body.turn.player_nickname, judge_participants[current_turn_player_idx], L3_MAX_NICKNAME_LEN - 1);
    turnMsg.body.turn.timeout_sec = L3_369engine_getTurnTimeout(L3_369engine_getTurnCount() + 1);

    // Judge의 엔진 동기화
    L3_369engine_onTurnReceived(&turnMsg.body.turn);

    judge_sendMessage(&turnMsg, L3_BROADCAST_ID);
}
