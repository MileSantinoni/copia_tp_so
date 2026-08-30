#ifndef STORAGE_SERVER_H_
#define STORAGE_SERVER_H_
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <commons/log.h>
#include "config.h"

int iniciar_servidor_storage(void);
void esperar_cliente_storage( int server_fd); // descritpor de archivo, para el socket que es un numero. server_fd representa un puerto de escucha del servidor
void * atender_cliente_storage(void * arg); // puntero el cual apunta a cualquier tipo de dato

#endif