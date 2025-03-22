#define PORT 8080
#define MAX_CLIENTS 5
#define SERVER_IP "127.0.0.1"
#define DATA_SEND_SLEEP_TIME 5000

typedef struct {
    int id;
    int x, y;
} Player;