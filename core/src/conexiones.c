#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "conexiones.h"
#include "core.h"

#include <utils/conexion.h>
#include <utils/comunicacion.h>
#include <utils/protocolo.h>
#include <utils/util.h>

extern t_log * logger;
extern int hay_interrupcion;
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

int conectar_con_planificador(char* ip, char* puerto, int id_core) {
    log_info(logger, "Intentando conectar con Planificador (%s:%s)...", ip, puerto);
    
    int sock = crear_conexion(ip, puerto);
    if (sock == -1) {
        log_error(logger, "No se pudo conectar con el Planificador en %s:%s", ip, puerto);
        return -1;
    }

    enviar_handshake(sock, CORE);
    enviar_id(sock,id_core);
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

void * escuchar_interrupt(void *arg){
    while(1){
        int cod_op = recibir_operacion (socket_interrupt);
        if (cod_op == -1){
            log_warning(logger, "Canal de interrupción desconectado");
            break;
        }
        switch(cod_op){
            case INTERRUPCION : {
                int size = 0;
                void * buffer = recibir_buffer(&size, socket_interrupt);
                free(buffer);
                
                log_info(logger, "## Interrupción recibida");
                hay_interrupcion = 1;
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


