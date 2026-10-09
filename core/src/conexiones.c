#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "conexiones.h"
#include "core.h"

#include <utils/conexion.h>
#include <utils/comunicacion.h>
#include <utils/protocolo.h>
#include <utils/util.h>

pthread_mutex_t mutex_interrupcion = PTHREAD_MUTEX_INITIALIZER; // defino e inicializo el mutex para proteger la variable compartida 
extern t_log * logger;
t_list *interrupciones_pendientes = NULL;
int socket_dispatch = -1;
int socket_interrupt = -1; // el valor -1 de los sockets me indican que el socket aun no fue creado o que ya se cerro
int socket_placa = -1;
pthread_t hilo_escucha;



void enviar_id(int socket, int id){

    t_paquete* paquete = crear_paquete(ENVIO_ID);

    agregar_a_paquete(paquete, &id, sizeof(int));
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);

    log_info(logger, "ID CORE <%i> enviado a socket: %i", id, socket);
}

int conectar_con_planificador(char* ip, char* puerto, int id_core, int tipo_conexion) {
    log_info(logger, "Intentando conectar con Planificador (%s:%s)...", ip, puerto);
    
    int sock = crear_conexion(ip, puerto);
    if (sock == -1) {
        log_error(logger, "No se pudo conectar con el Planificador en %s:%s", ip, puerto);
        return -1;
    }

    enviar_handshake(sock, CORE);
    enviar_id(sock,id_core);
    enviar_tipo_conexion_core(sock, tipo_conexion);
    

    log_info(logger, "## Conectado a Planificador exitosamente");
    
    return sock;
}

int conectar_con_placa(char* ip, char* puerto, int id_core) {
    log_info(logger, "Intentando conectar con Placa (%s:%s)...", ip, puerto);
    
    int sock = crear_conexion(ip, puerto);
    
    if (sock == -1) {
        log_error(logger, "No se pudo conectar con la Placa en %s:%s", ip, puerto);
        return -1;
    }

    enviar_handshake(sock, CORE);
    enviar_id(sock, id_core);
    log_info(logger, "## Conectado a Placa exitosamente");
    
    return sock;
}

// acá elimine la variable hay_interrupcion y ahora se lee del paquete el motivo y el JID
// tambien cambie el recibir buffer por recibir paquete 
void * escuchar_interrupt(void *arg){
    while(1){
        int cod_op = recibir_operacion (socket_interrupt);
        if (cod_op == -1){
            log_warning(logger, "Canal de interrupción desconectado");
            break;
        }
        switch(cod_op){
            case INTERRUPCION : {
                t_list *valores = recibir_paquete(socket_interrupt);

                if (valores == NULL || list_size(valores) < 2) {
                    log_error(
                        logger,
                        "Interrupción recibida con formato inválido"
                    );

                    if (valores != NULL) {
                        list_destroy_and_destroy_elements(valores, free);
                    }

                    break;
            }
            uint32_t jid_interrumpido =
                *(uint32_t *) list_get(valores, 0);

            t_motivo_desalojo motivo =
                *(t_motivo_desalojo *) list_get(valores, 1);

            t_interrupcion_pendiente *interrupcion =
                malloc(sizeof(t_interrupcion_pendiente));

             if (interrupcion == NULL)
                {
                    log_error(
                        logger,
                        "No se pudo reservar memoria para la interrupción"
                    );

                    list_destroy_and_destroy_elements(
                        valores,
                        free
                    );

                    break;
                }

            interrupcion->jid = jid_interrumpido;
            interrupcion->motivo = motivo;

            pthread_mutex_lock(&mutex_interrupcion);

            list_add(interrupciones_pendientes, interrupcion);

            pthread_mutex_unlock(&mutex_interrupcion);

            log_info(
                logger,
                "## Interrupción recibida - JID: %u - Motivo: %d",
                jid_interrumpido,
                motivo
            );

            list_destroy_and_destroy_elements(valores, free);

            break;
        }
        default:
            log_warning(logger, "Operación desconocida en canal interrupt: %d", cod_op);
            break;
        }
    }
    
    return NULL;
}
    




void crear_hilo_escucha(void)
{

    if (socket_interrupt == -1){
        log_error (logger, "No se puede iniciar el hilo: socket_interrupt no está conectado");
        return;
    }
    if (pthread_create(&hilo_escucha, NULL, escuchar_interrupt, NULL) != 0)
    { 
        log_error(logger, "No se pudo crear el hilo de escucha de interrupciones");
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_escucha);
    log_info(logger, "Hilo de escucha de interrupciones iniciado");

}

bool tomar_interrupcion_para_jid(uint32_t jid, t_motivo_desalojo *motivo)
{
    bool encontrada = false;

    pthread_mutex_lock(&mutex_interrupcion);

    for (int i = 0; i < list_size(interrupciones_pendientes); i++) {
        t_interrupcion_pendiente *pendiente =
            list_get(interrupciones_pendientes, i);

        if (pendiente->jid == jid) {
            *motivo = pendiente->motivo;

            list_remove(interrupciones_pendientes, i);
            free(pendiente);

            encontrada = true;
            break;
        }
    }

    pthread_mutex_unlock(&mutex_interrupcion);

    return encontrada;
}



// esta funcion lo que hace es descartar interrupciones anteriores o que no correponden con el JID actual
// TODO revisar si es necesaria 
void descartar_interrupciones_de_otros_jids(uint32_t jid_actual)
{
    pthread_mutex_lock(&mutex_interrupcion);

    for (int i = list_size(interrupciones_pendientes) - 1; i >= 0; i--) {
        t_interrupcion_pendiente *pendiente =
            list_get(interrupciones_pendientes, i);

        if (pendiente->jid != jid_actual) {
            log_warning(
                logger,
                "## Interrupción descartada - JID: %u - "
                "Job actual: %u",
                pendiente->jid,
                jid_actual
            );

            list_remove(interrupciones_pendientes, i);
            free(pendiente);
        }
    }

    pthread_mutex_unlock(&mutex_interrupcion);
}


