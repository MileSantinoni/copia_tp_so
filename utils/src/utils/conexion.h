#ifndef UTILS_CONEXION_H_
#define UTILS_CONEXION_H_

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <commons/log.h>
#include <utils/protocolo.h>
#include <commons/config.h>

/**
* @brief Imprime un saludo por consola
* @param quien Módulo desde donde se llama a la función
* @return No devuelve nada
*/

extern t_log *logger;
extern t_config *config;

void saludar(char* quien);

int iniciar_servidor(char *puerto);
int crear_conexion(char *ip, char *puerto);
int esperar_cliente(int socket_servidor);
void liberar_conexion(int socket_cliente);

#endif
