#include "mbed.h"
#include "string.h"

#include "ARQ_FSMevent.h"
#include "ARQ_msg.h"
#include "ARQ_timer.h"
#include "ARQ_LLinterface.h"
#include "ARQ_parameters.h"

//FSM state -------------------------------------------------
#define MAINSTATE_IDLE              0
#define MAINSTATE_TX                1
#define MAINSTATE_ACK               2

//GLOBAL variables (DO NOT TOUCH!) ------------------------------------------
//serial port interface
Serial pc(USBTX, USBRX);

//state variables
uint8_t main_state = MAINSTATE_IDLE; //protocol state

//source/destination ID
uint8_t endNode_ID=1;
uint8_t dest_ID=0;

//PDU context/size
uint8_t arqPdu[200];
uint8_t pduSize;

//SDU (input)
uint8_t originalWord[200];
uint8_t wordLen=0;



//ARQ parameters -------------------------------------------------------------
uint8_t seqNum = 0;     //ARQ sequence number
uint8_t retxCnt = 0;    //ARQ retransmission counter
uint8_t arqAck[5];      //ARQ ACK PDU


//application event handler : generating SDU from keyboard input
void arqMain_processInputWord(void)
{
    char c = pc.getc();
    if (main_state == MAINSTATE_IDLE &&
        !arqEvent_checkEventFlag(arqEvent_dataToSend))
    {
        if (c == '\n' || c == '\r')
        {
            originalWord[wordLen++] = '\0';
            arqEvent_setEventFlag(arqEvent_dataToSend);
            pc.printf("word is ready! ::: %s\n", originalWord);
        }
        else
        {
            originalWord[wordLen++] = c;
            if (wordLen >= ARQMSG_MAXDATASIZE-1)
            {
                originalWord[wordLen++] = '\0';
                arqEvent_setEventFlag(arqEvent_dataToSend);
                pc.printf("\n max reached! word forced to be ready :::: %s\n", originalWord);
            }
        }
    }
}




//FSM operation implementation ------------------------------------------------
int main(void){
    uint8_t flag_needPrint=1;
    uint8_t prev_state = 0;

    //initialization
    pc.printf("------------------ ARQ protocol starts! --------------------------\n");
    arqEvent_clearAllEventFlag();
    
    //source & destination ID setting
    pc.printf(":: ID for this node : ");
    pc.scanf("%d", &endNode_ID);
    pc.printf(":: ID for the destination : ");
    pc.scanf("%d", &dest_ID);
    pc.getc();

    pc.printf("endnode : %i, dest : %i\n", endNode_ID, dest_ID);

    arqLLI_initLowLayer(endNode_ID);
    pc.attach(&arqMain_processInputWord, Serial::RxIrq);





    while(1)
    {
        //debug message
        if (prev_state != main_state)
        {
            debug_if(DBGMSG_ARQ, "[ARQ] State transition from %i to %i\n", prev_state, main_state);
            prev_state = main_state;
        }


        //FSM should be implemented here! ---->>>>
        switch (main_state)
        {
            case MAINSTATE_IDLE: //IDLE state description
                
                // 이벤트 A (SDU 수신) 처리
                if (arqEvent_checkEventFlag(arqEvent_dataToSend)) //if data needs to be sent (keyboard input)
                {
                    // msg header setting ( PDU 생성 및 전송 )
                    pduSize = arqMsg_encodeData(arqPdu, originalWord, seqNum, wordLen);
                    arqLLI_sendData(arqPdu, pduSize, dest_ID);

                    pc.printf("[MAIN] sending to %i (seq:%i)\n", dest_ID, seqNum%ARQMSSG_MAX_SEQNUM);
                    
                    seqNum++;
                    retxCnt = 0;

                    main_state = MAINSTATE_TX;
                    flag_needPrint = 1;

                    wordLen = 0;
                    arqEvent_clearEventFlag(arqEvent_dataToSend);
                }
                // 이벤트 D (PDU 수신) 처리
                else if (arqEvent_checkEventFlag(arqEvent_dataRcvd)) //if data reception event happens
                {
                    //Retrieving data info.
                    uint8_t srcId = arqLLI_getSrcId();
                    uint8_t* dataPtr = arqLLI_getRcvdDataPtr();
                    uint8_t size = arqLLI_getSize();
                    uint8_t rcvdSn = arqMsg_getSeq(dataPtr);

                    pc.printf("\n -------------------------------------------------\nRCVD from %i : %s (length:%i, seq:%i)\n -------------------------------------------------\n", 
                                srcId, arqMsg_getWord(dataPtr), size, arqMsg_getSeq(dataPtr));

                    // ACK PDU 생성 및 전송
                    pduSize = arqMsg_encodeAck(arqAck, rcvdSn);
                    arqLLI_sendData(arqAck, pduSize, srcId);

                    main_state = MAINSTATE_TX;
                    flag_needPrint = 1;

                    arqEvent_clearEventFlag(arqEvent_dataRcvd);
                }
                else if (arqEvent_checkEventFlag(arqEvent_dataTxDone)) //if data needs to be sent (keyboard input)
                {
                    arqEvent_clearEventFlag(arqEvent_dataTxDone);
                }
                else if (arqEvent_checkEventFlag(arqEvent_ackRcvd)) //if data needs to be sent (keyboard input)
                {
                    arqEvent_clearEventFlag(arqEvent_ackRcvd);
                }
                else if (arqEvent_checkEventFlag(arqEvent_ackTxDone)) //if data needs to be sent (keyboard input)
                {
                    arqEvent_clearEventFlag(arqEvent_ackTxDone);
                }
                else if (arqEvent_checkEventFlag(arqEvent_arqTimeout)) //if data needs to be sent (keyboard input)
                {
                    arqEvent_clearEventFlag(arqEvent_arqTimeout);
                }
                else if (flag_needPrint == 1)
                {
                    pc.printf("Give a word to send : ");
                    flag_needPrint = 0;
                }     

                break;

            case MAINSTATE_TX: //IDLE state description
                // 이벤트 B (data TX 완료) 처리
                if (arqEvent_checkEventFlag(arqEvent_dataTxDone)) 
                {
                    arqTimer_startTimer(); //ARQ 타이머 시작 (ACK 수신 대기)

                    main_state = MAINSTATE_ACK;
                    arqEvent_clearEventFlag(arqEvent_dataTxDone);
                }
                // 이벤트 E (ACK TX 완료) 처리
                if (arqEvent_checkEventFlag(arqEvent_ackTxDone)) //data TX finished
                {
                    if (arqTimer_getTimerStatus())  // 조건C2: ACK 수신 대기 중 ACK 전송 완료 -> ACK 수신 대기 계속 
                    {
                        main_state = MAINSTATE_ACK;
                    }
                    else
                    {
                        main_state = MAINSTATE_IDLE;
                    }
                    
                    arqEvent_clearEventFlag(arqEvent_ackTxDone);
                }
                else if (arqEvent_checkEventFlag(arqEvent_dataRcvd)) //if data needs to be sent (keyboard input)
                {
                    arqEvent_clearEventFlag(arqEvent_dataRcvd);
                }
                else if (arqEvent_checkEventFlag(arqEvent_ackRcvd)) //if data needs to be sent (keyboard input)
                {
                    arqEvent_clearEventFlag(arqEvent_ackRcvd);
                }
                
                break;

            // 2->0 (c 3 추가)
            case MAINSTATE_ACK:

                if (arqEvent_checkEventFlag(arqEvent_ackRcvd))  // c : ACK 수신
                {
                    // 1. 받은 ACK의 seqNum 확인 (내가 보낸 것과 일치하는지)
                    uint8_t* dataPtr = arqLLI_getRcvdDataPtr();
                    uint8_t rcvdAckSn = arqMsg_getSeq(dataPtr);
                    if (rcvdAckSn == (seqNum - 1) % ARQMSSG_MAX_SEQNUM)  // 내 seq와 맞으면
                    {
                        // 2. timer 정지
                        arqTimer_stopTimer();
                        
                        // 3. 상태 전이
                        main_state = MAINSTATE_IDLE; //다음 루프 iteration에서 IDLE case로 들어가게 해줌
                        flag_needPrint = 1;
                        
                        pc.printf("[MAIN] ACK confirmed (seq:%i) → IDLE\n", rcvdAckSn);
                    }
                    // seq 불일치면 무시 (오래된 ACK)
                    
                    arqEvent_clearEventFlag(arqEvent_ackRcvd);
                }
                
                break;

            default :
                break;
        }


    }
}