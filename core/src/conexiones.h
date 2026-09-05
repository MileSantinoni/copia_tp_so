#ifndef CONEXIONES_H_
#define CONEXIONES_H_

#include <commons/log.h>

int conectar_con_planificador(char* ip, char* puerto, int id_core);
int conectar_con_placa(char* ip, char* puerto, int id_core);

void crear_hilo_escucha(void);
void* escuchar_servidores(void *arg);

void enviar_id(int socket_planificador, int id);

#endif