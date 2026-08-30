#include "storage_server.h"
#include <utils/util.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>

int iniciar_servidor_storage(Void){
    int socket_servidor;
    struct addrinfo  hints, *servinfo;
}