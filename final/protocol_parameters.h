#define DBGMSG_L2                       1 //debug print control
#define DBGMSG_L3                       1 //debug print control

#define L3_MAXDATASIZE                  1024

// Join Phase 타이머 [R-JOIN-08][R-JOIN-13]
#define L3_WAIT_ACK_TIMEOUT_SEC         3   // WAIT_ACK 타이머 기본값 (3초)
#define L3_JOIN_RETRY_DELAY_SEC         2   // 타임아웃 후 재전송 전 대기 (3초)
#define L3_JOIN_MAX_RETRY               3   // 최대 재시도 횟수


#define L2_ARQ_MAXRETRANSMISSION        10
#define L2_ARQ_MAXWAITTIME              5
#define L2_ARQ_MINWAITTIME              2
#define L1_FREQCHANNEL					7 // x 1MHz
