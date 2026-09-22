#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_

#include <commons/config.h>
#include <commons/log.h>

#include <utils/comunicacion.h>
#include <semaphore.h>


extern t_log *logger;
extern t_config *config;

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

// FUNCIONES GENERALES

//t_core *obtener_core_libre();
//t_job *buscar_por_jid(t_list *lista_job, u_int32_t jid);
//char *estado_a_string(t_estado estado);

//t_job *buscar_job_por_jid(t_list *lista_job, u_int32_t jid);
/*void finalizar_job(t_job *job, char *motivo);
void pasar_a_ready(t_job *job);
void encolar_new(t_job *job);*/



#endif