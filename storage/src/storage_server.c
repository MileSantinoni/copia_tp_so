#include "storage_server.h"
#include <utils/util.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>

int iniciar_servidor_storage(void){
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

// ponemos al socket en modo de escucha pasiva
if (listen(socket_servidor, SOMAXCONN)== -1){ // somaxconn define un tamaño maximo en la cola de espera de conexiones pendientes
 log_error(storage_logger, "error en listen en el puerto %s", storage_config -> PUERTO_ESCUCHA);
 close(socket_servidor);
 freeaddrinfo(servinfo);
 return -1;
}

 // libero la memoria reservada para getaddrinfo
  freeaddrinfo(servinfo);

  log_info(storage_logger, "servidor listo y escuchando al puerto % s", storage_config -> PUERTO_ESCUCHA);
  return socket_servidor;

}

void esperar_cliente_storage (int server_fd){
    while(1){ // Bucle que se ejecuta siempre 
    int * socket_cliente= malloc(sizeof(int));
    *socket_cliente = accept (server_fd, NULL, NULL);

    if (*socket_cliente == -1){
        log_error(storage_logger, "Error al aceptar un nuevo cliente: ");
        free (socket_cliente);
        continue;
    }
    pthread_t hilo_cliente; // identificador del hilo creado
    pthread_create (&hilo_cliente, NULL, atender_cliente_storage, socket_cliente); //crear y arrancar el hilo en 2do plano
    pthread_detach(hilo_cliente); // cuando este hilo termine de atendere al cliente hacer return y liberar memoria


    } 
}