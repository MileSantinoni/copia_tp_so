#include "servidor.h"
#include "planificador.h"

#include <stdlib.h>
#include <pthread.h>

#include <commons/config.h>
#include <commons/log.h>
#include <commons/collections/list.h>

#include <utils/conexion.h>
#include <utils/comunicacion.h>
#include <utils/protocolo.h>
#include <utils/util.h>

int socket_core;

void iniciar_conexion_como_servidor()
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

        int socket_core = esperar_cliente(socket_planificador);

        if (socket_core < 0)
        {
            log_error(logger, "Error al aceptar cliente");
            continue;
        }

        log_info(logger, "Se conectó un cliente");

        // Creamos un hilo por cada core que se conecta
        pthread_t thread;
        int *fd_ptr = malloc(sizeof(int));
        *fd_ptr = socket_core;

        pthread_create(&thread, NULL, atender_core, fd_ptr);
        pthread_detach(thread);
    }
}
bool comparar_por_socket(void *ptr)
{

    t_core *core = (t_core *)ptr;

    return core->socket == socket_core;
}

void *atender_core(void *arg)
{

    socket_core = *(int *)arg;
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

    t_core *nuevo_core = malloc(sizeof(t_core));

    nuevo_core->id_core = id_core;
    nuevo_core->jid_actual = -1;
    nuevo_core->libre = true;
    nuevo_core->socket = socket_core;

    pthread_mutex_lock(&mutex_lista_core);
    list_add(lista_cores, nuevo_core);
    pthread_mutex_unlock(&mutex_lista_core);
    sem_post(&sem_cores);

    // Recibo operaciones de CORE

    int codigo_operacion = recibir_operacion(socket_core);

    if (codigo_operacion == -1)
    {
        log_warning(logger, "Se desconecto CORE");

        pthread_mutex_lock(&mutex_lista_core);
        t_core *core_a_eliminar = (t_core *)list_remove_by_condition(lista_cores, comparar_por_socket);
        pthread_mutex_unlock(&mutex_lista_core);

        free(core_a_eliminar);

        sem_wait(&sem_cores);

        return NULL;
    }

    switch (codigo_operacion)
    {
    case MENSAJE:
    {
        int size;
        char *buffer = recibir_buffer(&size, socket_core);
        log_info(logger, "Me llego el mensaje: %s", buffer);
        free(buffer);

        // Confirmación simple de vuelta al cliente
        enviar_mensaje("CONFIRMACION_RECIBIDA", socket_core);
        break;
    }
    case DEVOLVER_CONTEXTO:
    {
        u_int32_t jid;
        t_registros registros;
        recibir_contexto(socket_core, &jid, &registros);

        t_job *job_encontrado = buscar_por_jid(lista_jobs, jid);

        if (job_encontrado == NULL)
        {
            log_error(logger, "No se encontro un JOB par jid :u%u", jid);
            break;
        }

        job_encontrado->contexto=&registros;


        // Liberar el core
        // M
    }

    default:
        log_warning(logger, "## Código de operación desconocido: %d", codigo_operacion);
        break;
    }

    liberar_conexion(socket_core);
    return NULL;
}
