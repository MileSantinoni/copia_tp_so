#ifndef CONEXIONES_H_
#define CONEXIONES_H_

#include <commons/log.h>


void enviar_handshake_core(int socket_destino);

int conectar_con_planificador(char* ip, char* puerto);
int conectar_con_placa(char* ip, char* puerto);

#endif