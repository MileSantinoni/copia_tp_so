#include "servidor.h"
#include "planificador.h"
#include "util.h"

#include <stdlib.h>
#include <pthread.h>

#include <commons/config.h>
#include <commons/log.h>
#include <commons/collections/list.h>

#include <utils/conexion.h>
#include <utils/comunicacion.h>
#include <utils/protocolo.h>
#include <utils/util.h>

void *iniciar_conexion_como_servidor(void *arg)
{

    // Obtenemos las configuraciones necesarias del archivo connfig
    char *puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");

    int socket_planificador = iniciar_servidor(puerto_escucha);

    if (socket_planificador < 0)
    {
        log_error(logger, "Error al iniciar servidor");
        config_destroy(config);
        log_destroy(logger);
        exit(EXIT_FAILURE);
    }

    log_info(logger, "Planificador listo, esperando clientes");

    while (1)
    {

        int socket_c = esperar_cliente(socket_planificador);

        if (socket_c < 0)
        {
            log_error(logger, "Error al aceptar cliente");
            continue;
        }

        log_info(logger, "Se conectó el cliente con socket: %i", socket_c);

        // Creamos un hilo por cada core que se conecta
        pthread_t thread;
        int *fd_ptr = malloc(sizeof(int));
        *fd_ptr = socket_c;

        pthread_create(&thread, NULL, atender_core, fd_ptr);
        pthread_detach(thread);
    }
}

void *atender_core(void *arg)
{

    int socket_core = *(int *)arg;
    free(arg);

    t_modulo modulo = recibir_handshake(socket_core);

    if (modulo != CORE)
    {
        log_error(logger, "El modulo %i no corresponde como cliente de Planificador", modulo);
        close(socket_core);
        return NULL;
    }

    int id_core = recibir_core_id(socket_core);
    log_info(logger, "ID CORE: %i", id_core);

    int tipo_conexion = recibir_tipo_conexion_core(socket_core);

    pthread_mutex_lock(&mutex_lista_core);
    t_core *core = buscar_core_por_id(lista_cores, id_core);

    if (core == NULL)
    {

        core = malloc(sizeof(t_core));

        core->id_core = id_core;
        core->jid_actual = -1;
        core->libre = false;
        core->socket_dispatch = -1;
        core->socket_interrupt = -1;

        list_add(lista_cores, core);
    }

    if (tipo_conexion == CONEXION_DISPATCH)
    {
        log_info(logger, "Core %i conectado por DISPATCH", id_core);
        core->socket_dispatch = socket_core;
    }
    else if (tipo_conexion == CONEXION_INTERRUPT)
    {
        log_info(logger, "Core %i conectado por INTERRUPT", id_core);
        core->socket_interrupt = socket_core;
    }
    else
    {
        log_error(logger, "No es un tipo de conexion válida");
    }

    if (core->socket_dispatch != -1 && core->socket_interrupt != -1)
    {
        core->libre = true;
        sem_post(&sem_cores);
    }

    pthread_mutex_unlock(&mutex_lista_core);

    if (tipo_conexion == CONEXION_INTERRUPT)
    {
        return NULL;
    }
    // Recibo operaciones de CORE
    while (1)
    {
        int codigo_operacion = recibir_operacion(socket_core);

        if (codigo_operacion == -1)
        {
            log_warning(logger, "Se desconecto CORE");

            pthread_mutex_lock(&mutex_lista_core);
            t_core *core_a_eliminar = buscar_core_por_id(lista_cores, core->id_core);
            list_remove_element(lista_cores, core_a_eliminar);
            pthread_mutex_unlock(&mutex_lista_core);

            if (!core_a_eliminar->libre)
            {
                pthread_mutex_lock(&mutex_lista_jobs);
                t_job *job_actual = buscar_job_por_jid(lista_jobs, core_a_eliminar->jid_actual);
                pthread_mutex_unlock(&mutex_lista_jobs);

                if (job_actual != NULL)
                {
                    pasar_a_ready(job_actual);
                }
                // No hago sem wait aca porque ya se hizo en el plani de corto plazo cuando se paso el job a exc
            }
            else
            {
                sem_wait(&sem_cores);
            }

            liberar_conexion(core->socket_dispatch);
            free(core_a_eliminar);
            return NULL;
        }

        switch (codigo_operacion)
        {
        case MENSAJE:
        {
            int size;
            char *buffer = recibir_buffer(&size, core->socket_dispatch);
            log_info(logger, "Me llego el mensaje: %s", buffer);
            free(buffer);

            // Confirmación simple de vuelta al cliente
            // enviar_mensaje("CONFIRMACION_RECIBIDA", socket_core);
            break;
        }
        case DESALOJAR_JOB:
        {
            u_int32_t jid;
            t_registros registros;
            recibir_contexto(core->socket_dispatch, &jid, &registros);

            actualizar_contexto(jid, &registros);
            pthread_mutex_lock(&mutex_lista_jobs);
            t_job *job_encontrado = buscar_job_por_jid(lista_jobs, jid);
            
            pthread_mutex_unlock(&mutex_lista_jobs);

            // FALTA RECIBIR EL MOTIVO DE DESALOJAR JOB
            op_code motivo;
            switch (motivo)
            {
            case SYSCALL_BLOQUEANTE:
            {
                actualizar_estimacion_prox_rafaga(job_encontrado);
                // falta completar
                break;
            }
            case FIN_QUANTUM:
            {
                pasar_a_ready(job_encontrado);

                // Libero el core
                pthread_mutex_lock(&mutex_lista_core);
                t_core *core_actual = buscar_core_por_id(lista_cores, id_core);
                core_actual->libre = true;
                pthread_mutex_unlock(&mutex_lista_core);

                sem_post(&sem_cores);
            }
            default:
                log_error(logger, "El motivo %i no es válido", motivo);
                break;
            }

            break;
        }
        case FINALIZAR_JOB:
        {

            u_int32_t jid;
            t_registros registros;

            recibir_contexto(core->socket_dispatch, &jid, &registros);

            actualizar_contexto(jid, &registros);

            pthread_mutex_lock(&mutex_lista_jobs);
            t_job *job_encontrado = buscar_job_por_jid(lista_jobs, jid);
            pthread_mutex_unlock(&mutex_lista_jobs);

            finalizar_job(job_encontrado, "EXIT", id_core);

            break;
        }

        default:
            log_warning(logger, "Operación desconocido desde CORE: %d", codigo_operacion);
            close(core->socket_dispatch);
            return NULL;
        }
    }

    return NULL;
}
