#ifndef PLACA_H_
#define PLACA_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>

#include <utils/protocolo.h>


// Variables compartidas
extern t_log *logger;
extern t_config *config;
extern t_config_placa config_valores;

// Configuración
typedef struct
{
    char *puerto_escucha;
    char *log_level;
    int tam_memoria;
    int tam_pagina;
    int retardo_memoria;
    char *algoritmo_reemplazo;
    char *path_instrucciones;
    char *path_offload;
    int tam_offload;
    int retardo_offload;
} t_config_placa;

// Entrada de tabla de páginas
typedef struct
{
    uint32_t nro_pagina;
    int marco;
    bool presencia;
    bool modificado;
    bool uso;
    uint64_t timestamp_lru;
    bool lockeada;
    uint32_t posicion_offload;
} t_entrada_pagina;

// Job
typedef struct
{
    uint32_t jid;
    char **instrucciones;
    int cantidad_instrucciones;
    t_list *tabla_de_paginas;
    uint32_t pagina_siguiente;
    uint32_t tamanio_alloc;
} t_job;

// Marco físico
typedef struct
{
    int nro_marco;
    bool libre;
    uint32_t jid;
    uint32_t nro_pagina;
} t_marco;


extern t_dictionary *diccionario_jobs;
extern pthread_mutex_t mutex_jobs;

// Inicialización del módulo
void inicializar_placa(char *path_config);

#endif