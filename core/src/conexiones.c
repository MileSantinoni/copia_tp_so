#include "conexiones.h"
#include "core.h"
#include <utils/conexion.h>
#include <utils/comunicacion.h>
#include <utils/protocolo.h>


void enviar_handshake_core(int socket_destino) {
    t_paquete* paquete = crear_paquete(CONEXION);
    t_modulo modulo = CORE;
    agregar_a_paquete(paquete, &modulo, sizeof(t_modulo));
    enviar_paquete(paquete, socket_destino);
    eliminar_paquete(paquete);
}

int conectar_con_planificador(char* ip, char* puerto) {
    log_info(logger, "Intentando conectar con Planificador (%s:%s)...", ip, puerto);
    
    int socket_fd = crear_conexion(ip, puerto);
    if (socket_fd == -1) {
        log_error(logger, "No se pudo conectar con el Planificador en %s:%s", ip, puerto);
        return -1;
    }

    enviar_handshake_core(socket_fd);
    
    log_info(logger, "## Conectado a Planificador exitosamente");
    
    return socket_fd;
}

int conectar_con_placa(char* ip, char* puerto) {
    log_info(logger, "Intentando conectar con Placa (%s:%s)...", ip, puerto);
    
    int socket_fd = crear_conexion(ip, puerto);
    if (socket_fd == -1) {
        log_error(logger, "No se pudo conectar con la Placa en %s:%s", ip, puerto);
        return -1;
    }

    enviar_handshake_core(socket_fd);
    
    log_info(logger, "## Conectado a Placa exitosamente");
    
    return socket_fd;
}