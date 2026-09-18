#include "plani_largo_plazo.h"
#include "cliente.h" // para usar el extern socket_placa

#include <stdlib.h>
#include <string.h>

#include <commons/log.h>
#include <commons/collections/list.h>

#include <utils/comunicacion.h>
#include <utils/protocolo.h>



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

    log_info(logger,"Enviando CREAR_JOB a Placa - JID: %u - Archivo: %s",jid,nombre_archivo);
    t_paquete *paquete = crear_paquete(CREAR_JOB);

    agregar_a_paquete(paquete, &jid, sizeof(uint32_t));
    agregar_a_paquete(paquete, nombre_archivo, strlen(nombre_archivo) + 1);

    enviar_paquete(paquete, socket_placa);
    eliminar_paquete(paquete);
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