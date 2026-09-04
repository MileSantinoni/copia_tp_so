#ifndef CONEXIONES_H_
#define CONEXIONES_H_

#include <commons/log.h>

int conectar_con_planificador(char* ip, char* puerto);
int conectar_con_placa(char* ip, char* puerto);

#endif