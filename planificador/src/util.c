#include "util.h"
#include <pthread.h>

// BUSQUEDA DE LISTAS

t_core *obtener_core_libre()
{

    for (int i = 0; i < list_size(lista_cores); i++)
    {

        t_core *core_actual = (t_core *)list_get(lista_cores, i);
        if (core_actual->libre)
        {

            return core_actual;
        }
    }

    return NULL;
}

t_job *buscar_job_por_jid(t_list *lista_job, u_int32_t jid)
{

    for (int i = 0; i < list_size(lista_job); i++)
    {
        t_job *job_actual = (t_job *)list_get(lista_job, i);

        if (job_actual->jid == jid)
        {
            return job_actual;
        }
    }
    return NULL;
}

t_core *buscar_core_por_socket_dispatch(t_list *lista_core, int socket)
{

    for (int i = 0; i < list_size(lista_core); i++)
    {
        t_core *core_actual = (t_core *)list_get(lista_core, i);

        if (core_actual->socket_dispatch == socket)
        {
            return core_actual;
        }
    }
    return NULL;
}

t_core *buscar_core_por_id(t_list *lista_cores, u_int32_t id_core)
{

    for (int i = 0; i < list_size(lista_cores); i++)
    {
        t_core *core_actual = (t_core *)list_get(lista_cores, i);

        if (core_actual->id_core == id_core)
        {
            return core_actual;
        }
    }
    return NULL;
}

// LIBERAR ESTRUCTURAS

void liberar_socket_core(int socket_core)
{

    pthread_mutex_lock(&mutex_lista_core);
    t_core *core_actual = buscar_core_por_socket_dispatch(lista_cores, socket_core);
    core_actual->libre = true;
    pthread_mutex_unlock(&mutex_lista_core);

    sem_post(&sem_cores);
}

void liberar_job(t_job *job)
{
    free(job->contexto);
    free(job);
}

// MANEJO DE ESTADOS

// log obligatorio de cambio de estado
char *estado_a_string(t_estado estado)
{
    switch (estado)
    {
    case NEW:
        return "NEW";
    case READY:
        return "READY";
    case EXEC:
        return "EXEC";
    case BLOCK:
        return "BLOCK";
    case EXIT:
        return "EXIT";
    default:
        return "DESCONOCIDO";
    }
}

// log de cambio de estado
// (lo dejo aca como funcion generica por si la usan tambien desde plani_corto_plazo.c)
void log_cambio_estado(uint32_t jid, char *anterior, char *actual)
{
    log_info(logger, "## (%d) Pasa del estado %s al estado %s", jid, anterior, actual);
}

// CAMBIO DE ESTADOS

// encola el job en NEW y avisa al hilo de largo plazo que hay algo para revisar
void encolar_new(t_job *job)
{
    pthread_mutex_lock(&mutex_cola_new);
    list_add(cola_new, job);
    pthread_mutex_unlock(&mutex_cola_new);

    sem_post(&sem_cola_new);
}

// pasa un job a READY: loguea el cambio, lo mete en cola_ready y avisa a plani_corto_plazo (que espera sem_cola_ready)
void pasar_a_ready(t_job *job)
{
    log_cambio_estado(job->jid, estado_a_string(job->estado), "READY");
    job->estado = READY;
    clock_gettime(CLOCK_MONOTONIC, &job->tiempo_inicio_ready);

    pthread_mutex_lock(&mutex_cola_ready);
    list_add(cola_ready, job);
    pthread_mutex_unlock(&mutex_cola_ready);

    sem_post(&sem_cola_ready);
}

void pasar_a_exec(t_job *job)
{
    char *estado_anterior = estado_a_string(job->estado);
    log_cambio_estado(job->jid, estado_anterior, "EXEC");
    clock_gettime(CLOCK_MONOTONIC, &job->tiempo_inicio_exec);
    job->estado = EXEC;
}


// crea un job con el contexto en 0 y lo suma a la lista general de jobs
t_job *crear_job(uint32_t jid){
    t_job *job = malloc(sizeof(t_job));
    job->jid = jid;
    job->estado = NEW; 
    job->estimacion_proxima_rafaga=estimacion_inicial;

    job->contexto = malloc(sizeof(t_registros));
    memset(job->contexto, 0, sizeof(t_registros)); // todos los registros arrancan en 0

    pthread_mutex_lock(&mutex_lista_jobs);
    list_add(lista_jobs, job);
    // !!!! list_add sin mutex por ahora, ver nota de concurrencia abajo de todo !!!
    pthread_mutex_unlock(&mutex_lista_jobs);

    log_info(logger, "## (%d) Se crea el Job - Estado: NEW", jid);

    return job;
}

// pasa un job a EXIT y libera un cupo de multiprogramacion
// (esto lo tiene que llamar quien procese el FINALIZAR_JOB que manda el Core, ver nota de coordinacion abajo)
void finalizar_job(t_job *job, char *motivo, u_int32_t id_core){

    log_cambio_estado(job->jid, estado_a_string(job->estado), "EXIT");
    job->estado = EXIT;

    log_info(logger, "## (%d) Finalizó su ejecución con motivo de %s. Estadísticas: %s", job->jid, motivo, "");

    // Libero el core
    pthread_mutex_lock(&mutex_lista_core);
    t_core *core_actual = buscar_core_por_id(lista_cores, id_core);
    core_actual->libre = true;
    core_actual->jid_actual = -1; //reseteo antes de liberar mutex
    pthread_mutex_unlock(&mutex_lista_core);

    // Lo saco de la lista general ANTES de liberarlo
    pthread_mutex_lock(&mutex_lista_jobs);
    list_remove_element(lista_jobs, job);
    pthread_mutex_unlock(&mutex_lista_jobs);

    // Elimino la estructura del job
    liberar_job(job);

    sem_post(&sem_cores);
    sem_post(&sem_cupo_multiprogramacion); // libero un lugar para que entre otro job de NEW
}

void actualizar_contexto(u_int32_t jid, t_registros *registros)
{
    pthread_mutex_lock(&mutex_lista_jobs);
    t_job *job_encontrado = buscar_job_por_jid(lista_jobs, jid);

    if (job_encontrado == NULL)
    {
        log_error(logger, "No se encontro un JOB par jid : %u", jid);
        pthread_mutex_unlock(&mutex_lista_jobs);
        return;
    }

    memcpy(job_encontrado->contexto, registros, sizeof(t_registros));
    pthread_mutex_unlock(&mutex_lista_jobs);
}


void actualizar_estimacion_prox_rafaga(t_job *job){

    struct timespec tiempo_actual;
    clock_gettime(CLOCK_MONOTONIC, &tiempo_actual);

    double segundos= tiempo_actual.tv_sec - job->tiempo_inicio_ready.tv_sec;
    double nanosegundos = tiempo_actual.tv_nsec - job->tiempo_inicio_ready.tv_nsec;

    double rafaga_real = segundos *1000.0  + nanosegundos/1000000.0;
    job->estimacion_proxima_rafaga=hrrn_alfa*rafaga_real+(1.0-hrrn_alfa)*job->estimacion_proxima_rafaga;
}