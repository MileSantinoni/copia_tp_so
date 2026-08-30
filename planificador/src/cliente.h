#ifndef PLANIFICADOR_CLIENTE_H
#define PLANIFICADOR_CLIENTE_H

#include <pthread.h>
#include <commons/log.h>
#include <commons/config.h>
#include <utils/conexion.h>

extern int socket_placa;
extern int socket_storage;

void iniciar_conexiones(t_config* config);
void iniciar_hilo_escucha(void);
void* escuchar_placa_storage(void* arg);

#endif