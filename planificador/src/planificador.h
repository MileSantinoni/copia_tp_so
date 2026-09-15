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
    int socket;
    bool libre;
    u_int32_t jid_actual;

}t_core;


// MUTEX DE COLAS DE ESTADO

extern pthread_mutex_t mutex_cola_ready;
extern pthread_mutex_t mutex_cola_block; // DEBERIAN SER DISTINTAS COLAS DE BLOQUEO SEGUN EL MOTIVO

// SEMAFORO 
//// DE COLAS DE ESTADO

extern sem_t sem_cola_ready;
extern sem_t sem_cola_block; // DEBERIAN SER DISTINTAS COLAS DE BLOQUEO SEGUN EL MOTIVO

//// SEMAFORO PARA VER SI HAY CORES DISPONIBLES

extern sem_t sem_cores;

// LISTAS

extern t_list *cola_new;
extern t_list *cola_ready;
extern t_list *cola_block;


void* planificador_corto_plazo(void* args);

void despachar_proceso();
t_core *obtener_core_libre();
void iniciar_quantum(t_job* job, t_core* core);

#endif