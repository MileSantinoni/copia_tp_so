#include "plani_largo_plazo.h"
#include "cliente.h" // para usar el extern socket_placa

#include <stdlib.h>
#include <string.h>

#include <commons/log.h>
#include <commons/collections/list.h>

#include <utils/comunicacion.h>
#include <utils/protocolo.h>

//log obligatorio de cambio de estado
char *estado_a_string(t_estado estado){
    switch (estado){
    case NEW:   return "NEW";
    case READY: return "READY";
    case EXEC:  return "EXEC";
    case BLOCK: return "BLOCK";
    case EXIT:  return "EXIT";
    default:    return "DESCONOCIDO";
    }
}

// log de cambio de estado
// (lo dejo aca como funcion generica por si la usan tambien desde plani_corto_plazo.c)
void log_cambio_estado(uint32_t jid, char *anterior, char *actual){
    log_info(logger, "## (%d) Pasa del estado %s al estado %s", jid, anterior, actual);
}

// crea un job con el contexto en 0 y lo suma a la lista general de jobs
t_job *crear_job(uint32_t jid){
    t_job *job = malloc(sizeof(t_job));
    job->jid = jid;
    job->estado = NEW;

    job->contexto = malloc(sizeof(t_registros));
    memset(job->contexto, 0, sizeof(t_registros)); // todos los registros arrancan en 0

    list_add(lista_jobs, job);
    // !!!! list_add sin mutex por ahora, ver nota de concurrencia abajo de todo !!!

    log_info(logger, "## (%d) Se crea el Job - Estado: NEW", jid);

    return job;
}

// le avisa a la Placa que existe este job, para que pueda leer las instrucciones y responder despues a los FETCH_INSTRUCCION del Core
void enviar_crear_job_a_placa(uint32_t jid, char *nombre_archivo){
    t_paquete *paquete = crear_paquete(CREAR_JOB);

    agregar_a_paquete(paquete, &jid, sizeof(uint32_t));
    agregar_a_paquete(paquete, nombre_archivo, strlen(nombre_archivo) + 1);

    enviar_paquete(paquete, socket_placa);
    eliminar_paquete(paquete);
}

// encola el job en NEW y avisa al hilo de largo plazo que hay algo para revisar
void encolar_new(t_job *job){
    pthread_mutex_lock(&mutex_cola_new);
    list_add(cola_new, job);
    pthread_mutex_unlock(&mutex_cola_new);

    sem_post(&sem_cola_new);
}

// pasa un job a READY: loguea el cambio, lo mete en cola_ready y avisa a plani_corto_plazo (que espera sem_cola_ready)
void pasar_a_ready(t_job *job){
    log_cambio_estado(job->jid, estado_a_string(job->estado), "READY");
    job->estado = READY;

    pthread_mutex_lock(&mutex_cola_ready);
    list_add(cola_ready, job);
    pthread_mutex_unlock(&mutex_cola_ready);

    sem_post(&sem_cola_ready);
}

// pasa un job a EXIT y libera un cupo de multiprogramacion
// (esto lo tiene que llamar quien procese el FINALIZAR_JOB que manda el Core, ver nota de coordinacion abajo)
void finalizar_job(t_job *job, char *motivo){
    log_cambio_estado(job->jid, estado_a_string(job->estado), "EXIT");
    job->estado = EXIT;

    log_info(logger, "## (%d) finalizó su ejecución con motivo de %s. Estadísticas: %s", job->jid, motivo, "");

    sem_post(&sem_cupo_multiprogramacion); // libero un lugar para que entre otro job de NEW
}

// hilo de planificacion de largo plazo: pasa jobs de NEW a READY
// respetando el grado maximo de multiprogramacion (semaforo contador)
void *planificador_largo_plazo(void *arg){
    while (1)
    {
        sem_wait(&sem_cola_new);               // espera que haya algun job en NEW
        sem_wait(&sem_cupo_multiprogramacion); // espera que haya un cupo libre

        pthread_mutex_lock(&mutex_cola_new);
        t_job *job = (t_job *)list_remove(cola_new, 0);
        pthread_mutex_unlock(&mutex_cola_new);

        if (job == NULL)
        {
            sem_post(&sem_cupo_multiprogramacion); // no habia nada, devuelvo el cupo
            continue;
        }

        pasar_a_ready(job);
    }
    return NULL;
}