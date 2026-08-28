#ifndef PLACA_H_
#define PLACA_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include <commons/log.h>
#include <commons/config.h>

#include "protocolo.h"

// Estructura para guardar la configuracion
typedef struct 
{
    char* puerto_escucha;
    char* log_level;
    int tam_memoria;
    int tam_pagina;
    int retardo_memoria;
    char* algoritmo_reemplazo;
    char* path_instrucciones;
    char* path_offload;
    int tam_offload;
    int retardo_offload;
} t_config_placa;

// Variables Globales
extern t_log* logger;
extern t_config* config_archivo;
extern t_config_placa config_valores;

// Prototipos de funciones
void inicializar_placa(char* path_config);
int inicia_servidor_placa(char* puerto);
void esperar_conexiones(int socket_servidor);
void* atender_cliente(void* arg);
void terminar_placa(void);

#endif /* PLACA_H_ */