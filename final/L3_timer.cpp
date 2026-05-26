#include "mbed.h"
#include "L3_FSMevent.h"
#include "protocol_parameters.h"


//ARQ retransmission timer
static Timeout timer;                       
static uint8_t timerStatus = 0;


//timer event : ARQ timeout
void L3_timer_timeoutHandler(void) 
{
    timerStatus = 0;
    L3_event_setEventFlag(L3_event_arqTimeout); // WAIT_ACK 타이머 만료 → 이벤트 발생
}

//timer related functions ---------------------------
void L3_timer_startTimer()
{
    uint8_t waitTime = L3_WAIT_ACK_TIMEOUT_SEC; // [R-JOIN-13] WAIT_ACK 타이머 = 3초
    timer.attach(L3_timer_timeoutHandler, waitTime);
    timerStatus = 1;
}

void L3_timer_stopTimer()
{
    timer.detach();
    timerStatus = 0;
}

uint8_t L3_timer_getTimerStatus()
{
    return timerStatus;
}
