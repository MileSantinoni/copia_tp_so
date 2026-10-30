#ifndef BLOQUEOS_H_
#define BLOQUEOS_H_

#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <commons/collections/list.h>

#include "planificador.h"

typedef struct
{
    t_job *job;
    t_motivo_desalojo motivo;

    uint32_t nro_pagina;    // PAGE_FAULT
    char *nombre_archivo;   // LOAD_BATCH (NULL en el resto)
    uint32_t tamanio;       // syscalls de memoria
    t_list *direcciones;    // syscalls de memoria: t_direccion_fisica*, una por pagina
} t_peticion_bloqueo;

typedef bool (*t_atencion)(int socket_placa, t_peticion_bloqueo *peticion);

typedef struct
{
    char *nombre;
    bool es_servicio;    // los servicios loguean inicio/fin y deslockean al terminar (page fault no)
    t_list *cola;
    pthread_mutex_t mutex;
    sem_t sem_pendientes;
    int socket_placa;    // conexion propia, solo la usa el hilo de esta cola
    t_atencion atender;
} t_cola_bloqueo;

void iniciar_bloqueos(void);
bool es_motivo_de_bloqueo(t_motivo_desalojo motivo);
void bloquear_job(t_job *job, t_motivo_desalojo motivo, t_list *valores);
void deslockear_paginas_de_job(uint32_t jid);

#endif