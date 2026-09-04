#include "cliente.h"

int socket_placa;
int socket_storage;

void iniciar_conexiones(t_config* config)
{
    char* ip_placa = config_get_string_value(config, "IP_PLACA");

    char* puerto_placa = config_get_string_value(config, "PUERTO_PLACA");

    char* ip_storage = config_get_string_value(config, "IP_STORAGE");

    char* puerto_storage = config_get_string_value(config, "PUERTO_STORAGE");


    socket_placa = crear_conexion(ip_placa, puerto_placa);

    if (socket_placa == -1){
        log_error(logger, "No se pudo conectar con Placa");
        exit(EXIT_FAILURE);
    }

    log_info(logger, "Conectado a Placa");


    socket_storage = crear_conexion(ip_storage, puerto_storage);

    if (socket_storage == -1){
        log_error(logger, "No se pudo conectar con Storage");
        exit(EXIT_FAILURE);
    }

    log_info(logger, "Conectado a Storage");
}

pthread_t hilo_escucha;

void iniciar_hilo_escucha(void){

    pthread_create(&hilo_escucha, NULL, escuchar_placa_storage, NULL);

    if (pthread_create(&hilo_escucha, NULL, escuchar_placa_storage, NULL) != 0) //chequear si el hilo se creo correctamente
    { 
        log_error(logger, "No se pudo crear el hilo de escucha");
        exit(EXIT_FAILURE);
    }
}

void* escuchar_placa_storage(void* arg){ //args no se usan, pero son necesarios para la firma de la función
    //hilo que escuche a la placa y al storage, y que procese los mensajes que lleguen de ellos. Por ahora, solo vamos a escuchar y loguear los mensajes que lleguen.
    //select() o poll() para escuchar a ambos sockets, y luego loguear los mensajes que lleguen. 
    while (1)
    {
        // TODO: recibir y procesar mensajes de Placa y Storage (falta implementar)
    }

    return NULL;
}