#include "plani_corto_plazo.h"
#include "planificador.h"
#include "util.h"

#include <semaphore.h>
#include <pthread.h>


int quantum;


void *planificador_corto_plazo(void *arg)
{
    while (1)
    {
        // TENGO QUEE TENER ALGO EN READY Y CORES DISPONIBLES
        sem_wait(&sem_cola_ready);
        sem_wait(&sem_cores);
        despachar_proceso(); 
    }
    
    NULL;
}

void despachar_proceso()
{


    t_job *job_elegido;

    if (strcmp(algoritmo_planificacion, "RR") == 0 || strcmp(algoritmo_planificacion, "FIFO") == 0 )
    {
        pthread_mutex_lock(&mutex_cola_ready);
        job_elegido = (t_job *)list_remove(cola_ready, 0);
        pthread_mutex_unlock(&mutex_cola_ready);
    }
    else if (strcmp(algoritmo_planificacion, "HRRN"))
    {
        // Falta implementar
    }
    else
    {
        log_error(logger, "Algoritmo de planificación inválido");
    }

    pasar_a_exec(job_elegido);
    
    pthread_mutex_lock(&mutex_lista_core);
    t_core *core_elegido = obtener_core_libre();
   
    if (core_elegido == NULL)
    {

        log_error(logger, "Falló algun chequeo anterior porque no debio llegar aca");
        return;
    }

    core_elegido->libre = false;
    core_elegido->jid_actual = job_elegido->jid;
    pthread_mutex_unlock(&mutex_lista_core);

    enviar_contexto(core_elegido->socket_dispatch, DESPACHAR_JOB, job_elegido->jid, job_elegido->contexto);

    if (strcmp(algoritmo_planificacion, "RR") == 0)
    { 
        iniciar_quantum(job_elegido, core_elegido);
    }
}

void *controlar_quantum(void* arg)
{
   
    t_args_quantum* datos = (t_args_quantum*) arg;

    // Espero el quentum indicado. Multiplico poor 1000 porque usleep trabaja con microsegundo
    usleep(datos->quantum*1000);

    // Verifico que el job que esta ejecuntando el core correspondiente sea el mismo y no haya cambiado 
    // Puede ser que el tiempo de ejecucion era menor al quantuum establecido 

    if((datos->core->jid_actual != datos->job->jid) || datos->core->libre){

        free(datos);
        return NULL;
    }

    // Desalojo el proceso del core 

    t_motivo_desalojo motivo = FIN_QUANTUM;
    t_paquete * paquete = crear_paquete(INTERRUPCION);
    agregar_a_paquete(paquete,&datos->job->jid,sizeof(datos->job->jid));
    agregar_a_paquete(paquete,&motivo,sizeof(motivo));
    enviar_paquete(paquete,datos->core->socket_interrupt);
    eliminar_paquete(paquete);

    log_info(logger, "## (%u) - Desalojado por fin de quantum",datos->job->jid);
    
    free(datos);
    return NULL;
}

void iniciar_quantum(t_job *job_elegido, t_core *core_elegido)
{
    t_args_quantum* datos = malloc(sizeof(t_args_quantum));
    datos->core=core_elegido;
    datos->job=job_elegido;
    datos->quantum=config_get_int_value(config, "RR_QUANTUM");

    pthread_t hilo_quantum;
    pthread_create(&hilo_quantum,NULL, controlar_quantum, datos);
    pthread_detach(hilo_quantum);
}


