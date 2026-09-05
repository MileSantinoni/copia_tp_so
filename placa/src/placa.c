#include "placa.h"

#include <utils/conexion.h>
#include <utils/comunicacion.h>
#include <utils/protocolo.h>
#include <utils/util.h>

// Definición de las variables globales declaradas como extern en el .h
t_log *logger;
t_config *config;
t_config_placa config_valores;

int socket_core;
int socket_planificador;

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Uso correcto: ./bin/placa [Archivo Config] \n");
        exit(EXIT_FAILURE);
    }

    char *path_config = (argc > 1) ? argv[1] : "placa.config";

    inicializar_placa(path_config);

    // int socket_servidor = inicia_servidor_placa(config_valores.puerto_escucha); --> puse iniciar servviddor para no repetir codigo al pedo
    int socket_servidor = iniciar_servidor(config_valores.puerto_escucha);

    if (socket_servidor < 0)
    {
        log_error(logger, "Error al iniciar servidor");
        config_destroy(config);
        log_destroy(logger);
        exit(EXIT_FAILURE);
    }

    log_info(logger, "Placa lista para recibir conexiones - Puerto: %s", config_valores.puerto_escucha);

    esperar_conexiones(socket_servidor);

    terminar_modulo();

    return 0;
}

void inicializar_placa(char *path_config)
{
    config = config_create(path_config);

    if (config == NULL)
    {
        printf("No se pudo abrir el archivo de configuración: %s\n", path_config);
        exit(EXIT_FAILURE);
    }

    // Cargamos los valores del config a nuestra estructura
    config_valores.puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");
    config_valores.log_level = config_get_string_value(config, "LOG_LEVEL");
    config_valores.tam_memoria = config_get_int_value(config, "TAM_MEMORIA");
    config_valores.tam_pagina = config_get_int_value(config, "TAM_PAGINA");
    config_valores.retardo_memoria = config_get_int_value(config, "RETARDO_MEMORIA");
    config_valores.algoritmo_reemplazo = config_get_string_value(config, "ALGORITMO_REEMPLAZO");
    config_valores.path_instrucciones = config_get_string_value(config, "PATH_INSTRUCCIONES");
    config_valores.path_offload = config_get_string_value(config, "PATH_OFFLOAD");
    config_valores.tam_offload = config_get_int_value(config, "TAM_OFFLOAD");
    config_valores.retardo_offload = config_get_int_value(config, "RETARDO_OFFLOAD");

    logger = log_create("placa.log", "PLACA", true, log_level_from_string(config_valores.log_level));
    if (logger == NULL)
    {
        printf("No se pudo crear el logger\n");
        exit(EXIT_FAILURE);
    }
}

/*int inicia_servidor_placa(char* puerto)
{
    struct addrinfo hints, *server_info;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // Para usar la propia IP

    getaddrinfo(NULL, puerto, &hints, &server_info);

    int socket_servidor = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);

    // Evita el error "Address already in use" al reiniciar rápido
    int yes = 1;
    setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    if (bind(socket_servidor, server_info->ai_addr, server_info->ai_addrlen) != 0)
    {
        log_error(logger, "## Error al bindear el socket en el puerto %s", puerto);
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(server_info);

    if (listen(socket_servidor, SOMAXCONN) != 0)
    {
        log_error(logger, "## Error al escuchar en el socket");
        exit(EXIT_FAILURE);
    }

    return socket_servidor;
}*/

void esperar_conexiones(int socket_servidor)
{
    while (1)
    {
        int socket_cliente = accept(socket_servidor, NULL, NULL);

        if (socket_cliente == -1)
            continue;

        int *socket_ptr = malloc(sizeof(int));
        *socket_ptr = socket_cliente;

        pthread_t hilo_cliente;
        pthread_create(&hilo_cliente, NULL, atender_cliente, socket_ptr);
        pthread_detach(hilo_cliente);
    }
}

void *atender_cliente(void *arg)
{
    int socket_cliente = *(int *)arg;
    free(arg);

    t_modulo modulo = recibir_handshake(socket_cliente);
    char *modulo_s = modulo_a_string(modulo);

    switch (modulo)
    {
    case CORE:
        socket_core = socket_cliente;
        int id_core = recibir_core_id(socket_core);
        log_info(logger, "ID CORE: %i", id_core);
        break;
    case PLANIFICADOR:

        socket_planificador = socket_cliente;
        break;

    default:
        log_warning(logger, "## Módulo desconocido: %s", modulo_s);
        close(socket_cliente);
        return NULL;
    }

        log_info(logger, "Esperando peticiones de %s", modulo_s);

    while (1)
    {
        int codigo_operacion = recibir_operacion(socket_cliente); // Reccibimos el codigo del paquete aca, dentro del switch hay que recibir el paquete

        if (codigo_operacion == -1)
        {
            log_warning(logger, "Se desconecto %s", modulo_s);
            return NULL;
        }
        switch (codigo_operacion)
        {
            /*case HANDSHAKE_CORE:
                log_info(logger, "## Handshake recibido: Módulo CORE");
                enviar_mensaje("HANDSHAKE_OK", socket_cliente);
                break;

            case HANDSHAKE_PLANIFICADOR:
                log_info(logger, "## Handshake recibido: Módulo PLANIFICADOR");
                enviar_mensaje("HANDSHAKE_OK", socket_cliente);
                break;*/

        case MENSAJE:
        {
            // El texto está después del entero de tamaño (payload + 4 bytes)
            /*char* texto = (char*)(paquete->payload + sizeof(uint32_t));
            log_info(logger, "## Mensaje recibido: %s", texto);*/

            int size;
            char *buffer = recibir_buffer(&size, socket_cliente);
            log_info(logger, "Me llego el mensaje: %s", buffer);
            free(buffer);

            // Confirmación simple de vuelta al cliente
            enviar_mensaje("CONFIRMACION_RECIBIDA", socket_cliente);
            break;
        }

            /*case PAQUETE_PRUEBA:
            {
                log_info(logger, "## Paquete con datos recibido");

                t_paquete* respuesta = crear_paquete(PAQUETE_PRUEBA);
                agregar_string_a_paquete(respuesta, "ack desde la placa");
                enviar_paquete(respuesta, socket_cliente);
                eliminar_paquete(respuesta);
                break;
            }*/

        default:
            log_warning(logger, "## Código de operación desconocido: %d", codigo_operacion);
            break;
        }
    }

    close(socket_cliente);
    return NULL;
}
