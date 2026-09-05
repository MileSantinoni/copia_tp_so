#include "conexiones.h"
#include "core.h"

#include <utils/conexion.h>
#include <utils/comunicacion.h>
#include <utils/protocolo.h>
#include <utils/util.h>

#include <sys/select.h>
#include <errno.h>
#include <pthread.h>


int socket_placa;
int socket_planificador;

int conectar_con_planificador(char* ip, char* puerto, int id_core) {
    log_info(logger, "Intentando conectar con Planificador (%s:%s)...", ip, puerto);
    
    socket_planificador = crear_conexion(ip, puerto);
    if (socket_planificador == -1) {
        log_error(logger, "No se pudo conectar con el Planificador en %s:%s", ip, puerto);
        return -1;
    }

    enviar_handshake(socket_planificador, CORE);
    enviar_id(socket_planificador,id_core);
    log_info(logger, "## Conectado a Planificador exitosamente");
    
    return socket_planificador;
}

int conectar_con_placa(char* ip, char* puerto, int id_core) {
    log_info(logger, "Intentando conectar con Placa (%s:%s)...", ip, puerto);
    
    socket_placa = crear_conexion(ip, puerto);
    
    if (socket_placa == -1) {
        log_error(logger, "No se pudo conectar con la Placa en %s:%s", ip, puerto);
        return -1;
    }

    enviar_handshake(socket_placa, CORE);
    enviar_id(socket_placa, id_core);
    log_info(logger, "## Conectado a Placa exitosamente");
    
    return socket_placa;
}

void enviar_id(int socket, int id){

    t_paquete* paquete = crear_paquete(ENVIO_ID);

    agregar_a_paquete(paquete, &id, sizeof(int));
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);

    log_info(logger, "ID CORE <%i> enviado a socket: %i", id, socket);
}

pthread_t hilo_escucha;

void crear_hilo_escucha(void)
{

    if (pthread_create(&hilo_escucha, NULL, escuchar_servidores, NULL) != 0) // chequear si el hilo se creo correctamente
    { 
        log_error(logger, "No se pudo crear el hilo de escucha");
        exit(EXIT_FAILURE);
    }

    log_info(logger, "Hilo de escucha de Placa y Planificador iniciado");
}

void* escuchar_servidores(void *arg){

    while (1) // TODO: recibir y procesar mensajes de Placa y Planificador
    {
        fd_set sockets; // Creo una lista de sockets que voy controlar de forma simultanea

        FD_ZERO(&sockets); // Incializa la lista de sockets

        FD_SET(socket_placa, &sockets); // Agrego los sockets a la lista 
        FD_SET(socket_planificador, &sockets); // Agrego los sockets a la lista 

        // La funcion select necesita saber cual es el numero de socket mas grande 

        int max_socket = socket_placa;

        if (socket_planificador > max_socket)
        {
            max_socket = socket_planificador;
        }

        // Se queda esperando a que alguno de los sockets tenga datos listos para leer
        int resultado = select(max_socket + 1, &sockets, NULL, NULL, NULL); // Devuelve la cantidad de sockets que tiene algo para leer

        if (resultado == -1) //  Chequeo que select sea valido  
        {
            if (errno == EINTR)
            {
                continue;
            }

            log_error(logger, "Error en select()");
            break;
        }

        if (FD_ISSET(socket_placa, &sockets)) //  Me fijo si el socket de PLACA fue el que se activo 
        {

            int cod_op = recibir_operacion(socket_placa);

            if (cod_op == -1)
            {
                log_warning(logger, "Se desconecto PLACA");
                exit(EXIT_FAILURE);
            }

            switch (cod_op)
            {
            case PAQUETE:
            {
                t_list *paquete = recibir_paquete(socket_placa);

                // procesar paquete

                list_destroy_and_destroy_elements(paquete, free);
                break;
            }

            default:
                log_warning(logger, "Operacion desconocida desde PLACA: %d", cod_op);
                break;
            }
        }

        if (FD_ISSET(socket_planificador, &sockets)) //  Me fijo si el socket de PLANIFICADOR fue el que se activo 
        {

            int cod_op = recibir_operacion(socket_planificador);

            if (cod_op == -1)
            {
                log_warning(logger, "Se desconecto PLANIFICADOR");
                exit(EXIT_FAILURE);
            }

            switch (cod_op)
            {
            case MENSAJE:
            {
                int size;
                char *buffer = recibir_buffer(&size, socket_planificador);
                log_info(logger, "Me llego el mensaje: %s", buffer);
                free(buffer);

                // Confirmación simple de vuelta al cliente
                enviar_mensaje("CONFIRMACION_RECIBIDA", socket_planificador);
                break;
            }

            default:
                log_warning(logger, "Operacion desconocida desde PLANIFICADOR: %d", cod_op);
                break;
            }
        }
    }
    return NULL;
}
