#include "L3_FSMevent.h"
#include "L3_msg.h"
#include "L3_timer.h"
#include "L3_LLinterface.h"
#include "protocol_parameters.h"
#include "mbed.h"

#include <string.h>


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

//application event handler : generating SDU from keyboard input
static void L3service_processInputWord(void)
{
    char c = pc.getc();

    // Do not accept new keyboard input while a previous SDU is waiting to be sent.
    if (L3_event_checkEventFlag(L3_event_dataToSend))
        return;

    // If only Enter is pressed without any actual character,
    // ignore it so that an empty message is not sent automatically.
    if (c == '\n' || c == '\r')
    {
        if (wordLen == 0)
            return;

        // Append string terminator and notify the FSM that one SDU is ready.
        originalWord[wordLen] = '\0';
        L3_event_setEventFlag(L3_event_dataToSend);
        debug_if(DBGMSG_L3, "word is ready! ::: %s\n", originalWord);
        return;
    }

    // Store a normal character if there is still room in the buffer.
    if (wordLen < L3_MAXDATASIZE - 1)
    {
        originalWord[wordLen++] = c;
    }
    else
    {
        // If the buffer becomes full, finish the string immediately
        // and request DATA_REQ to lower layer.
        originalWord[wordLen] = '\0';
        L3_event_setEventFlag(L3_event_dataToSend);
        pc.printf("\nmax reached! word forced to be ready :::: %s\n", originalWord);
    }
}



void L3_initFSM(uint8_t destId)
{

    myDestId = destId;
    //initialize service layer
    pc.attach(&L3service_processInputWord, Serial::RxIrq);

    pc.printf("Give a word to send : ");
}

void L3_FSMrun(void)
{   
    if (prev_state != main_state)
    {
        debug_if(DBGMSG_L3, "[L3] State transition from %i to %i\n", prev_state, main_state);
        prev_state = main_state;
    }

    //FSM implementation
    switch (main_state)
    {
        case L3STATE_IDLE: //IDLE state : wait for receive / send / reconfiguration events

            //1) DATA_IND event from lower layer
            //   - retrieve received message information
            //   - print the received text on screen
            //   - clear the event flag
            if (L3_event_checkEventFlag(L3_event_msgRcvd))
            {
                uint8_t* dataPtr = L3_LLI_getMsgPtr();
                uint8_t size = L3_LLI_getSize();

                //DATA_IND handling
                // - get received SDU pointer and size from lower-layer interface
                // - print the received text message on the terminal
                // - clear the receive event flag after processing
                pc.printf("\n[RX] %.*s\n", size, dataPtr);
                L3_event_clearEventFlag(L3_event_msgRcvd);
                pc.printf("Give a word to send : ");
            }

            //2) keyboard input completed
            //   - copy SDU from input buffer to transmission buffer
            //   - request DATA_REQ service to lower layer
            //   - clear the event flag
            else if (L3_event_checkEventFlag(L3_event_dataToSend))
            {
                memcpy(sdu, originalWord, wordLen + 1);
                debug_if(DBGMSG_L3, "[L3] SDU length : %i\n", wordLen);
                L3_LLI_dataReqFunc(sdu, wordLen, myDestId);

                // Clear the local input buffer after requesting transmission.
                wordLen = 0;
                originalWord[0] = '\0';
                sdu[0] = '\0';
                L3_event_clearEventFlag(L3_event_dataToSend);
            }

            //3) DATA_CNF event from lower layer
            //   - transmission finished
            //   - print confirmation message
            //   - clear the event flag
            else if (L3_event_checkEventFlag(L3_event_dataSendCnf))
            {
                pc.printf("\n[TX DONE]\n");
                L3_event_clearEventFlag(L3_event_dataSendCnf);
                pc.printf("Give a word to send : ");
            }

            //4) source ID reconfiguration confirmation event
            //   - this is used when the lower layer finishes source ID update
            //   - clear the event flag after handling
            else if (L3_event_checkEventFlag(L3_event_recfgSrcIdCnf))
            {
                pc.printf("\n[SRC ID RECONFIG DONE]\n");
                L3_event_clearEventFlag(L3_event_recfgSrcIdCnf);
                pc.printf("Give a word to send : ");
            }
            break;

        default :
            break;
    }
}