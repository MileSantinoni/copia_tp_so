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

    if (strcmp(algoritmo_planificacion, "RR") == 0 || strcmp(algoritmo_planificacion, "FIFO") == 0)
    {
        pthread_mutex_lock(&mutex_cola_ready);
        job_elegido = (t_job *)list_remove(cola_ready, 0);
        pthread_mutex_unlock(&mutex_cola_ready);
    }
    else if (strcmp(algoritmo_planificacion, "HRRN") == 0)
    {

        job_elegido = mayor_response_ratio();
        log_info(logger, "## (%u) - Prioridad HRRN calculada: <%2f> - Estimación próxima ráfaga: <%2f>",
                 job_elegido->jid, calcular_response_ratio(job_elegido), job_elegido->estimacion_proxima_rafaga);
    }
    else
    {
        log_error(logger, "Algoritmo de planificación inválido");
        return;
    }

    pasar_a_exec(job_elegido);

    pthread_mutex_lock(&mutex_lista_core);
    t_core *core_elegido = obtener_core_libre();

    if (core_elegido == NULL)
    {

        log_error(logger, "Falló algun chequeo anterior porque no debio llegar aca");
        pthread_mutex_unlock(&mutex_lista_core);
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

void *controlar_quantum(void *arg)
{
    t_args_quantum *datos = (t_args_quantum *)arg;
    usleep(datos->quantum * 1000);

    pthread_mutex_lock(&mutex_lista_core);
    bool sigue_el_mismo = (datos->core->jid_actual == datos->jid) && !datos->core->libre;
    pthread_mutex_unlock(&mutex_lista_core);

    if (!sigue_el_mismo)
    {
        free(datos);
        return NULL;
    }

    t_motivo_desalojo motivo = FIN_QUANTUM;
    t_paquete *paquete = crear_paquete(INTERRUPCION);
    agregar_a_paquete(paquete, &datos->jid, sizeof(u_int32_t));
    agregar_a_paquete(paquete, &motivo, sizeof(motivo));
    enviar_paquete(paquete, datos->core->socket_interrupt);
    eliminar_paquete(paquete);

    log_info(logger, "## (%u) - Desalojado por fin de quantum", datos->jid);
    free(datos);
    return NULL;
}

void iniciar_quantum(t_job *job_elegido, t_core *core_elegido)
{
    t_args_quantum *datos = malloc(sizeof(t_args_quantum));
    datos->core = core_elegido;
    datos->jid = job_elegido->jid;
    datos->quantum = rr_quantum;

    pthread_t hilo_quantum;
    pthread_create(&hilo_quantum, NULL, controlar_quantum, datos);
    pthread_detach(hilo_quantum);
}

double calcular_response_ratio(t_job *job)
{

    double s = job->estimacion_proxima_rafaga;

    struct timespec tiempo_actual;

    clock_gettime(CLOCK_MONOTONIC, &tiempo_actual);
    double segundos = tiempo_actual.tv_sec - job->tiempo_inicio_ready.tv_sec;
    double nanosegundos = tiempo_actual.tv_nsec - job->tiempo_inicio_ready.tv_nsec;
    double w = segundos * 1000.0 + nanosegundos / 1000000.0; // lo convierto en milisengudos

    return 1.0 + (w / s);
}

t_job *mayor_response_ratio()
{

    double response_ratio_mayor = -1;
    int indice_mayor = -1;
    t_job *job_mayor_rr = NULL;

    pthread_mutex_lock(&mutex_cola_ready);
    for (int i = 0; i < list_size(cola_ready); i++)
    {

        t_job *job_actual = list_get(cola_ready, i);
        double response_ratio_job_actual = calcular_response_ratio(job_actual);

        if (response_ratio_job_actual > response_ratio_mayor) // usamos > y no >= porque asi se queda con el priemro que encuentre
        {

            response_ratio_mayor = response_ratio_job_actual;
            indice_mayor = i;
        }
    }

    if (indice_mayor != -1)
    {
        job_mayor_rr = list_remove(cola_ready, indice_mayor);
    }
    pthread_mutex_unlock(&mutex_cola_ready);

    return job_mayor_rr;
}
