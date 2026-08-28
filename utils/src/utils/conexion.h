#ifndef UTILS_CONEXION_H_
#define UTILS_CONEXION_H_

#include <stdlib.h>
#include <stdio.h>
#include<signal.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netdb.h>
#include<string.h>
#include<commons/log.h>
#include <utils/protocolo.h>

/**
* @brief Imprime un saludo por consola
* @param quien Módulo desde donde se llama a la función
* @return No devuelve nada
*/

void saludar(char* quien);

extern t_log *logger;

int iniciar_servidor(char *puerto);
int crear_conexion(char *ip, char *puerto);
int esperar_cliente(int socket_servidor);
void liberar_conexion(int socket_cliente);

#endif
