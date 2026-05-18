#include "mbed.h"
#include "L3_msg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* L3_msg_type_to_string(L3MsgType type)
{
    switch (type) {
        case L3_MSG_JOIN:
            return "JOIN";
        case L3_MSG_JOIN_ACK:
            return "JOIN_ACK";
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

    if (strcmp(type_str, "JOIN_ACK") == 0) {
        return L3_MSG_JOIN_ACK;
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

// Wire serialization helpers

static int safe_copy_field(char* dst, size_t dstCap, const char* src, size_t srcLen)
{
    if (dst == NULL || dstCap == 0 || src == NULL) {
        return 0;
    }
    if (srcLen >= dstCap) {
        srcLen = dstCap - 1;
    }
    memcpy(dst, src, srcLen);
    dst[srcLen] = '\0';
    return 1;
}

uint8_t L3_msg_serialize(const L3Message* msg, uint8_t* buffer, uint8_t bufSize)
{
    if (msg == NULL || buffer == NULL || bufSize == 0) {
        return 0;
    }

    int written = 0;
    char* out = (char*)buffer;

    switch (msg->type) {
        case L3_MSG_JOIN:
            written = snprintf(out, bufSize, "JOIN:%s", msg->body.join.node_nickname);
            break;
        case L3_MSG_JOIN_ACK:
            written = snprintf(out, bufSize, "JOIN_ACK:%s:%u",
                               msg->body.join_ack.node_nickname,
                               (unsigned)msg->body.join_ack.registered_count);
            break;
        case L3_MSG_SETUP:
            written = snprintf(out, bufSize, "SETUP:%s:%s:%s:%s:%s:%s",
                               msg->body.setup.judge_nickname,
                               msg->body.setup.player_order[0],
                               msg->body.setup.player_order[1],
                               msg->body.setup.player_order[2],
                               msg->body.setup.player_order[3],
                               msg->body.setup.notice);
            break;
        case L3_MSG_TURN:
            written = snprintf(out, bufSize, "TURN:%s:%u",
                               msg->body.turn.player_nickname,
                               (unsigned)msg->body.turn.timeout_sec);
            break;
        case L3_MSG_ANSWER:
            written = snprintf(out, bufSize, "ANSWER:%s:%s",
                               msg->body.answer.player_nickname,
                               msg->body.answer.value);
            break;
        case L3_MSG_GAMEOVER:
            written = snprintf(out, bufSize, "GAMEOVER:%s:%s",
                               msg->body.gameover.eliminated_player_nickname,
                               L3_elim_reason_to_string(msg->body.gameover.reason));
            break;
        default:
            return 0;
    }

    if (written <= 0 || written >= (int)bufSize) {
        return 0;
    }
    return (uint8_t)written;
}

L3MsgType L3_msg_peekType(const uint8_t* buffer, uint8_t size)
{
    if (buffer == NULL || size == 0) {
        return L3_MSG_UNKNOWN;
    }

    char head[16];
    size_t n = 0;
    while (n < size && n < sizeof(head) - 1 && buffer[n] != ':') {
        head[n] = (char)buffer[n];
        n++;
    }
    head[n] = '\0';

    return L3_string_to_msg_type(head);
}

int L3_msg_deserialize(const uint8_t* buffer, uint8_t size, L3Message* msg)
{
    if (buffer == NULL || size == 0 || msg == NULL) {
        return 0;
    }

    char tmp[L3_MSG_MAX_SERIAL_LEN + 1];
    if (size >= sizeof(tmp)) {
        return 0;
    }
    memcpy(tmp, buffer, size);
    tmp[size] = '\0';

    memset(msg, 0, sizeof(*msg));

    char* save = NULL;
    char* tok = strtok_r(tmp, ":", &save);
    if (tok == NULL) {
        msg->type = L3_MSG_UNKNOWN;
        return 0;
    }

    msg->type = L3_string_to_msg_type(tok);

    switch (msg->type) {
        case L3_MSG_JOIN: {
            tok = strtok_r(NULL, ":", &save);
            if (tok == NULL) return 0;
            return safe_copy_field(msg->body.join.node_nickname,
                                   L3_MAX_NICKNAME_LEN, tok, strlen(tok));
        }
        case L3_MSG_JOIN_ACK: {
            tok = strtok_r(NULL, ":", &save);
            if (tok == NULL) return 0;
            safe_copy_field(msg->body.join_ack.node_nickname,
                            L3_MAX_NICKNAME_LEN, tok, strlen(tok));
            tok = strtok_r(NULL, ":", &save);
            if (tok == NULL) return 0;
            msg->body.join_ack.registered_count = (uint8_t)atoi(tok);
            return 1;
        }
        case L3_MSG_SETUP: {
            tok = strtok_r(NULL, ":", &save);
            if (tok == NULL) return 0;
            safe_copy_field(msg->body.setup.judge_nickname,
                            L3_MAX_NICKNAME_LEN, tok, strlen(tok));
            for (int i = 0; i < L3_MAX_PLAYERS; i++) {
                tok = strtok_r(NULL, ":", &save);
                if (tok == NULL) return 0;
                safe_copy_field(msg->body.setup.player_order[i],
                                L3_MAX_NICKNAME_LEN, tok, strlen(tok));
            }
 
            tok = strtok_r(NULL, "", &save);
            if (tok == NULL) return 0;
            return safe_copy_field(msg->body.setup.notice,
                                   L3_MAX_NOTICE_LEN, tok, strlen(tok));
        }
        case L3_MSG_TURN: {
            tok = strtok_r(NULL, ":", &save);
            if (tok == NULL) return 0;
            safe_copy_field(msg->body.turn.player_nickname,
                            L3_MAX_NICKNAME_LEN, tok, strlen(tok));
            tok = strtok_r(NULL, ":", &save);
            if (tok == NULL) return 0;
            msg->body.turn.timeout_sec = (uint8_t)atoi(tok);
            return 1;
        }
        case L3_MSG_ANSWER: {
            tok = strtok_r(NULL, ":", &save);
            if (tok == NULL) return 0;
            safe_copy_field(msg->body.answer.player_nickname,
                            L3_MAX_NICKNAME_LEN, tok, strlen(tok));
            tok = strtok_r(NULL, ":", &save);
            if (tok == NULL) return 0;
            return safe_copy_field(msg->body.answer.value,
                                   L3_MAX_VALUE_LEN, tok, strlen(tok));
        }
        case L3_MSG_GAMEOVER: {
            tok = strtok_r(NULL, ":", &save);
            if (tok == NULL) return 0;
            safe_copy_field(msg->body.gameover.eliminated_player_nickname,
                            L3_MAX_NICKNAME_LEN, tok, strlen(tok));
            tok = strtok_r(NULL, ":", &save);
            if (tok == NULL) return 0;
            msg->body.gameover.reason = L3_string_to_elim_reason(tok);
            return 1;
        }
        default:
            return 0;
    }
}
