#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_

#include <commons/config.h>
#include <commons/log.h>

#include <utils/comunicacion.h>
#include <semaphore.h>


extern t_log *logger;
extern t_config *config;
//extern t_log_level log_level;

extern char *algoritmo_planificacion;

extern int rr_quantum;
extern int estimacion_inicial;
extern double hrrn_alfa;
extern int grado_max_multiprogramacion;

extern char *path_datos;
extern int retardo_loader;
extern char *path_reportes;

typedef enum{
    NEW,
    READY,
    EXEC,
    BLOCK,
    EXIT
}t_estado;

typedef struct{
    u_int32_t jid;
    t_registros* contexto;
    t_estado estado;

    double estimacion_proxima_rafaga;

    struct timespec tiempo_inicio_ready;
    struct timespec tiempo_inicio_exec;

}t_job;

typedef struct{
    u_int32_t id_core;
    int socket_dispatch;
    int socket_interrupt;
    bool libre;
    u_int32_t jid_actual;
}t_core;

void incializar_mutex();
void incializar_semaforos();
void incializar_listas();
void finalizar_planificador();

void obtener_config();

// LISTA DE TODOS LOS JOBS

extern t_list *lista_jobs;
extern pthread_mutex_t mutex_lista_jobs;

// LISTA CORES

extern t_list *lista_cores;
extern pthread_mutex_t mutex_lista_core;
extern sem_t sem_cores;

// COLAS DE ESTADO

extern t_list *cola_new;
extern pthread_mutex_t mutex_cola_new;
extern sem_t sem_cola_new;

extern t_list *cola_ready;
extern pthread_mutex_t mutex_cola_ready;
extern sem_t sem_cola_ready;

extern t_list *cola_block; // DEBERIAN SER DISTINTAS COLAS DE BLOQUEO SEGUN EL MOTIVO
extern pthread_mutex_t mutex_cola_block; 
extern sem_t sem_cola_block;


// MULTIPROGRAMACIÓN

extern sem_t sem_cupo_multiprogramacion;


#endif