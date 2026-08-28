    #include <utils/conexion.h>


    int iniciar_servidor(char *puerto) {
        // Quitar esta línea cuando hayamos terminado de implementar la funcion
        // assert(!"no implementado!"); lo quitamos

        int socket_servidor;

        struct addrinfo hints, *servinfo /*, *p*/;
        // el  *p hay que usarlo para la lista enlazada, comento solo  para probar conexion

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        getaddrinfo(NULL, puerto, &hints, &servinfo); // nos guarda en serverino un puntero que apunta hacia los datos necesarios para la creación del socket...

        //  Creamos el socket de escucha del servidor
        socket_servidor = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

        // Permite reutilizar el puerto inmediatamente después de terminar la ejecución
        int activador = 1;
        if (activador)
        {
            setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &activador, sizeof(activador));
        }

        // Asociamos el socket a un puerto
        bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);
        // Escuchamos las conexiones entrantes
        listen(socket_servidor, SOMAXCONN); // listen(FD_ESCUCHA, PARÁMETRO DEL KERNEL DE LINUX establece el núm
        // max de conexiones que puedn estar en cola para un socket);

        freeaddrinfo(servinfo);
        log_trace(logger, "Listo para escuchar a mi cliente");

        return socket_servidor;
    }

    int crear_conexion(char *ip, char *puerto){
        struct addrinfo hints;
        struct addrinfo *server_info;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        getaddrinfo(ip, puerto, &hints, &server_info);

        // Ahora vamos a crear el socket.
        int socket_cliente = socket(server_info->ai_family,
                                    server_info->ai_socktype,
                                    server_info->ai_protocol);

        // Ahora que tenemos el socket, vamos a conectarlo //es decir asociamos el socket a un puerto
        if (connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen) == -1)
        {
            // SI falla liberamos memoria y retornamos -1
            freeaddrinfo(server_info);
            close(socket_cliente); // Cerramos el socket que no se pudo usar
            return -1;
        }

        freeaddrinfo(server_info); // liberamos la memoria?

        return socket_cliente; // Si no la funcion connect no retorna -1, devuelve un numero de socket.
    }

    int esperar_cliente(int socket_servidor){
        // Quitar esta línea cuando hayamos terminado de implementar la funcion
        // assert(!"no implementado!");

        // Aceptamos un nuevo cliente
        // int socket_cliente= accept(socket_servidor, NULL,NULL); //no va?
        // int fd_conexion=accept(fd_escucha,NULL,NULL);//los NULL son informacion como ip o puerto?
        int socket_cliente = accept(socket_servidor, NULL, NULL);
        log_info(logger, "Se conecto un cliente!");

        return socket_cliente;
    }

    void liberar_conexion(int socket_cliente)
    {
        close(socket_cliente);
    }
