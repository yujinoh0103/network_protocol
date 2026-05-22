void L3_initFSM(uint8_t);
void L3_FSMrun(void);

// player FSM용 키보드 입력 접근자
const char* L3_getInputWord(void);
uint8_t     L3_getInputWordLen(void);
void        L3_clearInputWord(void);