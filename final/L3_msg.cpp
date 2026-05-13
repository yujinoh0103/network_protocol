#include "mbed.h"
#include "L3_msg.h"

#include <string.h>

const char* L3_msg_type_to_string(L3MsgType type)
{
    switch (type) {
        case L3_MSG_JOIN:
            return "JOIN";
        case L3_MSG_SETUP:
            return "SETUP";
        case L3_MSG_TURN:
            return "TURN";
        case L3_MSG_ANSWER:
            return "ANSWER";
        case L3_MSG_GAMEOVER:
            return "GAMEOVER";
        default:
            return "UNKNOWN";
    }
}

const char* L3_elim_reason_to_string(L3ElimReason reason)
{
    switch (reason) {
        case L3_REASON_OUT_OF_TURN:
            return "OUT_OF_TURN";
        case L3_REASON_WRONG_ANSWER:
            return "WRONG_ANSWER";
        case L3_REASON_TIMEOUT:
            return "TIMEOUT";
        case L3_REASON_NONE:
        default:
            return "NONE";
    }
}

L3MsgType L3_string_to_msg_type(const char* type_str)
{
    if (type_str == NULL) {
        return L3_MSG_UNKNOWN;
    }

    if (strcmp(type_str, "JOIN") == 0) {
        return L3_MSG_JOIN;
    }

    if (strcmp(type_str, "SETUP") == 0) {
        return L3_MSG_SETUP;
    }

    if (strcmp(type_str, "TURN") == 0) {
        return L3_MSG_TURN;
    }

    if (strcmp(type_str, "ANSWER") == 0) {
        return L3_MSG_ANSWER;
    }

    if (strcmp(type_str, "GAMEOVER") == 0) {
        return L3_MSG_GAMEOVER;
    }

    return L3_MSG_UNKNOWN;
}

L3ElimReason L3_string_to_elim_reason(const char* reason_str)
{
    if (reason_str == NULL) {
        return L3_REASON_NONE;
    }

    if (strcmp(reason_str, "OUT_OF_TURN") == 0) {
        return L3_REASON_OUT_OF_TURN;
    }

    if (strcmp(reason_str, "WRONG_ANSWER") == 0) {
        return L3_REASON_WRONG_ANSWER;
    }

    if (strcmp(reason_str, "TIMEOUT") == 0) {
        return L3_REASON_TIMEOUT;
    }

    return L3_REASON_NONE;
}
