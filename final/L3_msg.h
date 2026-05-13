#ifndef FINAL_L3_MSG_H
#define FINAL_L3_MSG_H

#include "mbed.h"

#define L3_MAX_NICKNAME_LEN 16
#define L3_MAX_PLAYERS 4
#define L3_MAX_VALUE_LEN 8
#define L3_MAX_NOTICE_LEN 32

typedef enum {
    L3_MSG_JOIN = 0,
    L3_MSG_SETUP,
    L3_MSG_TURN,
    L3_MSG_ANSWER,
    L3_MSG_GAMEOVER,
    L3_MSG_UNKNOWN
} L3MsgType;

typedef enum {
    L3_REASON_NONE = 0,
    L3_REASON_OUT_OF_TURN,
    L3_REASON_WRONG_ANSWER,
    L3_REASON_TIMEOUT
} L3ElimReason;

typedef struct {
    char node_nickname[L3_MAX_NICKNAME_LEN];
} L3JoinMsg;

typedef struct {
    char judge_nickname[L3_MAX_NICKNAME_LEN];
    char player_order[L3_MAX_PLAYERS][L3_MAX_NICKNAME_LEN];
    char notice[L3_MAX_NOTICE_LEN];
} L3SetupMsg;

typedef struct {
    char player_nickname[L3_MAX_NICKNAME_LEN];
    uint8_t timeout_sec;
} L3TurnMsg;

typedef struct {
    char player_nickname[L3_MAX_NICKNAME_LEN];
    char value[L3_MAX_VALUE_LEN];
} L3AnswerMsg;

typedef struct {
    char eliminated_player_nickname[L3_MAX_NICKNAME_LEN];
    L3ElimReason reason;
} L3GameOverMsg;

typedef struct {
    L3MsgType type;

    union {
        L3JoinMsg join;
        L3SetupMsg setup;
        L3TurnMsg turn;
        L3AnswerMsg answer;
        L3GameOverMsg gameover;
    } body;
} L3Message;

const char* L3_msg_type_to_string(L3MsgType type);
const char* L3_elim_reason_to_string(L3ElimReason reason);

L3MsgType L3_string_to_msg_type(const char* type_str);
L3ElimReason L3_string_to_elim_reason(const char* reason_str);

#endif