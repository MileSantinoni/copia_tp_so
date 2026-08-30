#include "storage_server.h"
#include <utils/util.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>

int iniciar_servidor_storage(Void){
    int socket_servidor;
    struct addrinfo  hints, *servinfo;
    // inicializo en 0 los criterios para direccion de red
    memset (&hints, 0, sizeof(hints)); // hint es una variable de tipo estructura, actua como filtro de preferencias
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // asigna la ip de la maquina local

    getaddrinfo (NULL, storage_config -> PUERTO_ESCUCHA, &hints, &servinfo); // puerto escucha a una red valida


  // Creamos el socket con los parametros obtenidos 
    socket_servidor = socket(servinfo -> ai_family, servinfo -> ai_socktype, servinfo-> ai_protocol); // ai_protocol define capa de transporte del socket
    if (socket_servidor == -1){
        log_error(storage_logger, " Error al crear el socket de storage");
        freeaddrinfo(servinfo);
        return -1;
    } 
    // funcion para que no se bloquee el puerto durante un determinado tiempo si cerramos el sistema
    // permitimos reutilizar el puerto inmediatamente al reiniciar
    int optval = 1; // habilitar opcion  (true)
    setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    // enlazamos el socket al puerto configurado
    if(bind(socket_servidor, servinfo -> ai_addr, servinfo-> ai_addrlen)== -1){
        log_error(storage_logger, "Eror en bind al puerto %s", storage_config -> PUERTO_ESCUCHA);
        close(socket_servidor);
        freeaddrinfo(servinfo);
        return -1;
    }

// 

}