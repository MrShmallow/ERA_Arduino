//protocol for transferring messages between the arduino and the app

#define CONN_REQ 11000
#define CONN_ACK 21100
#define CONN_ERR 21200

#define BALL_TOUCH 22000
#define BALL_TOUCH_ACK 12100
#define BALL_TOUCH_ERR 12200

#define REG_TOUCH 23000
#define REG_TOUCH_ACK 13100
#define REG_TOUCH_ERR 13200

#define NET_TOUCH 24000
#define NET_TOUCH_ACK 14100
#define NET_TOUCH_ERR 14200

#define KEEP_ALIVE 25000
#define KEEP_ALIVE_ACK 15100
#define KEEP_ALIVE_ERR 15200

#define ERR_APP 18000
#define ERR_ARD 28000

#define DISCONN 19000

#define NET_RIGHT 1
#define NET_LEFT 2

#define ERR_PROTO 1
#define ERR_ACK 2
#define ERR_CONN 3

#define ACK 100
#define ERR 200
