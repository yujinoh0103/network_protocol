#include "L3_FSMevent.h"
#include "L3_msg.h"
#include "L3_timer.h"
#include "L3_LLinterface.h"
#include "L3_FSMmain_judge.h"
#include "L3_FSMmain_player.h"
#include "protocol_parameters.h"
#include "mbed.h"


//FSM state -------------------------------------------------
#define L3STATE_IDLE                0


//state variables
static uint8_t main_state = L3STATE_IDLE; //protocol state
static uint8_t prev_state = main_state;

//SDU (input)
static uint8_t originalWord[1030];
static uint8_t wordLen=0;

static uint8_t sdu[1030];

//serial port interface
static Serial pc(USBTX, USBRX);
static uint8_t myDestId;
static uint8_t isJudgeNode = 0;

extern uint8_t input_thisId;

//application event handler : generating SDU from keyboard input
static void L3service_processInputWord(void)
{
    char c = pc.getc();
    if (!L3_event_checkEventFlag(L3_event_dataToSend))
    {
        if (c == 0x7f || c == 0x08) {
            if (wordLen > 0) {
                wordLen--;
            }
            return;
        }
        if ((unsigned char)c < 0x20 && c != '\n' && c != '\r') {
            return;
        }
        if (c == '\n' || c == '\r')
        {
            originalWord[wordLen++] = '\0';
            L3_event_setEventFlag(L3_event_dataToSend);
            debug_if(DBGMSG_L3,"word is ready! ::: %s\n", originalWord);
        }
        else
        {
            originalWord[wordLen++] = c;
            if (wordLen >= L3_MAXDATASIZE-1)
            {
                originalWord[wordLen++] = '\0';
                L3_event_setEventFlag(L3_event_dataToSend);
                pc.printf("\n max reached! word forced to be ready :::: %s\n", originalWord);
            }
        }
    }
}



void L3_initFSM(uint8_t destId)
{

    myDestId = destId;
    isJudgeNode = (input_thisId == L3_JUDGE_NODE_ID);
    if (isJudgeNode) {
        L3_judge_initIDLE();
        return;
    }

    //initialize service layer
    pc.attach(&L3service_processInputWord, Serial::RxIrq);

    L3_player_initFSM(); // player FSM 초기화 (닉네임 입력 프롬프트 출력)
}

void L3_FSMrun(void)
{   
    if (prev_state != main_state)
    {
        debug_if(DBGMSG_L3, "[L3] State transition from %i to %i\n", prev_state, main_state);
        prev_state = main_state;
    }

    if (isJudgeNode) {
        if (L3_judge_getCurrentState() == L3_JUDGE_STATE_IDLE) {
            L3_judge_handleIDLE();
        }
        return;
    }

    // player FSM 실행 — L3_FSMmain_player.cpp가 모든 상태 처리
    L3_player_runFSM();
}

// -------------------------------------------------------
// player FSM이 읽어갈 키보드 입력 접근자
// -------------------------------------------------------
const char* L3_getInputWord(void)
{
    return (const char*)originalWord;
}

uint8_t L3_getInputWordLen(void)
{
    return (wordLen > 0) ? (wordLen - 1) : 0; // null terminator 제외
}

void L3_clearInputWord(void)
{
    wordLen = 0;
    L3_event_clearEventFlag(L3_event_dataToSend);
}