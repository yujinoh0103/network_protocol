#include "mbed.h"
#include "string.h"
#include "L2_FSMmain.h"
#include "L3_FSMmain.h"

//serial port interface
Serial pc(USBTX, USBRX);

//GLOBAL variables (DO NOT TOUCH!) ------------------------------------------

//source/destination ID
uint8_t input_thisId=1;

//FSM operation implementation ------------------------------------------------
int main(void){

    //initialization
    pc.printf(
        "\r\n\r\n"
        "=================================\r\n"
        "*                 369 GAME START!                 *\r\n"
        "=================================\r\n"
        "\r\n"
        "Welcome to the 369 Star Game! :D\r\n"
        "\r\n"
        "[HOW TO PLAY]\r\n"
        "Type the right answer on your turn.\r\n"
        "If the number has 3, 6, or 9, type \"*\".\r\n"
        "Examples: 3 -> *, 13 -> *, 33 -> **, 36 -> **\r\n"
        "Turn time gets shorter: 10s, 7s, then 5s.\r\n"
        "\r\n"
        "[SETTING]\r\n"
        "Node 0 is Judge. Other nodes are Players.\r\n"
        "Only one Judge is allowed.\r\n"
        "Nickname: English only, max 8 chars.\r\n"
        "\r\n"
        "[FLOW]\r\n"
        "4 players join. One elimination ends the game.\r\n"
        "Then players enter a new nickname to retry.\r\n"
        "\r\n"
        "=================================\r\n"
    );
        //source & destination ID setting
    pc.printf("\r\nNode number > ");
    pc.scanf("%d", &input_thisId);
    pc.getc();

    if (input_thisId == 0) {
        pc.printf("[Judge] Node 0 selected.\r\n");
        pc.printf("[Judge] If a Judge already exists, reset this board\r\n");
        pc.printf("[Judge] and register as a Player with another node number.\r\n");
    }

    pc.printf("Node=%i\r\n", input_thisId);
    
    

    //initialize lower layer stacks
    L2_initFSM(input_thisId);
    L3_initFSM(0);
    
    while(1)
    {
        L2_FSMrun();
        L3_FSMrun();
    }
}
