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

void *atender_core(void *arg)
{

    int socket_core = *(int *)arg;
    free(arg);

    int codigo_conexion = recibir_operacion(socket_core);

    if (codigo_conexion != CONEXION)
    {
        log_error(logger, "Se envio una operación NO esperada");
        exit(EXIT_FAILURE);
    }

    t_list *paquete_recibido = recibir_paquete(socket_core);

    if (paquete_recibido == NULL)
    {
        log_error(logger, "Error al recibir el paquete");
        list_destroy_and_destroy_elements(paquete_recibido, free);
        return NULL;
    }

    t_modulo modulo = *(t_modulo *)list_get(paquete_recibido, 0);

    if (modulo != CORE)
    {
        log_error(logger, "El modulo %i no corresponde como cliente de Planificador", modulo);
        list_destroy_and_destroy_elements(paquete_recibido, free);
        close(socket_core);
        return NULL;
    }

    char* modulo_s = modulo_a_string(modulo);

    log_info(logger,"## Módulo: %s", modulo_s);

    list_destroy_and_destroy_elements(paquete_recibido, free);

    int codigo_operacion = recibir_operacion(socket_core);

    switch (codigo_operacion)
    {
    case MENSAJE:
        /* code */
        break;

    default:
        break;
    }

    return NULL;
}
