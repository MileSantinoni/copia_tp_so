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
#include <commons/string.h>

#include <commons/collections/dictionary.h>

#include <utils/protocolo.h>

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

// Estructura para guardar las instrucciones de cada Job

typedef struct
{
    uint32_t jid;
    char** instrucciones; // arrray de strings
    int cantidad_instrucciones;
} t_job_instrucciones;

// Diccionario/lista de Jobs cargafos.
extern t_dictionary* diccionario_jobs;
extern pthread_mutex_t mutex_jobs;


// Variables Globales
extern t_log* logger;
extern t_config* config;
extern t_config_placa config_valores;

// Prototipos de funciones
void inicializar_placa(char* path_config);
void esperar_conexiones(int socket_servidor);
void* atender_cliente(void* arg);
void finalizar_job_en_placa(uint32_t jid);
// void terminar_placa(void); --> la pase de forma general al util  porque nos sirve a todos los modulos

//Prototipos CHECK 2
void crear_job_en_placa(uint32_t jid, char* nombre_archivo);
char* obtener_instruccion(uint32_t jid, uint32_t pc);

#endif 