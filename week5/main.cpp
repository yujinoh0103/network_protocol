#include "mbed.h"
#include "string.h"
#include "L2_FSMmain.h"
#include "L3_FSMmain.h"

//serial port interface
Serial pc(USBTX, USBRX);

uint8_t getID() {
    char c;
    char rx_buffer[4];
    int rx_index = 0;

    while (1)
    {
        c = pc.getc();

        // 처음 들어온 개행문자는 무시
        if ((c == '\n' || c == '\r') && rx_index == 0) {
            continue;
        }

        // 엔터 입력 시 문자열 종료
        if (c == '\n' || c == '\r') {
            rx_buffer[rx_index] = '\0';
            break;
        }

        // 숫자만 저장
        if (c >= '0' && c <= '9') {
            if (rx_index < 3) {
                rx_buffer[rx_index++] = c;
                pc.putc(c);   // 입력한 숫자 에코 출력
            }
        }
    }

    pc.printf("\n");
    return (uint8_t)atoi(rx_buffer);
}


//GLOBAL variables (DO NOT TOUCH!) ------------------------------------------

//source/destination ID
uint8_t input_thisId=1;
uint8_t input_destId=0;

//FSM operation implementation ------------------------------------------------
int main(void){

    //initialization
    pc.printf("------------------ protocol stack starts! --------------------------\n");
        //source & destination ID setting
    pc.printf(":: ID for this node : ");
    input_thisId = getID();
    pc.printf(":: ID for the destination : ");
    input_destId = getID();

    pc.printf("endnode : %i, dest : %i\n", input_thisId, input_destId);
    
    

    //initialize lower layer stacks
    L2_initFSM(input_thisId);
    L3_initFSM(input_destId);
    
    while(1)
    {
        L2_FSMrun();
        L3_FSMrun();
    }
}