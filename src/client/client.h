#pragma once

#include "common/common.h"
#include "common/client_common.h"

#define PRINT_RECEIVED_INFO 0
#define PRINT_RECEIVED_HEADER_INFO 0
#define PRINT_SENT_PLAYER_UPDATE 0


void connect_to_server();
void receive_server_data(int client_socket);
void *client_communication(void *arg);
