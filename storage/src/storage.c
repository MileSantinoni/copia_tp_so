#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include "storage_server.h"
#include <utils/conexion.h>

t_log *logger;
t_config *config;

int main(int argc, char *argv[])
{
    
    if (argc < 2)
    {
        printf("Error: debe ingresar la ruta del archivo de configuracion \n");
        printf("uso: %s [PATH_CONFIG]\n", argv[0]); // argv inicializado en la posicion 0
        return EXIT_FAILURE;
    }

    // cargo configuracion
    if (cargar_configuracion(argv[1]) != 0) // argv [1]arreglo de strings en este caso ubicado en la posicion 1
    {
        printf("No se pudo cargar el archivo: %s", argv[1]);
        return EXIT_FAILURE;
    }
    
    printf("Configuración cargada con exito en puerto: %s \n", storage_config->PUERTO_ESCUCHA);
    
    // inicializo el logger
    logger = log_create("storage.log", "STORAGE", true, storage_config->log_level); // true para guardar archivos y mostrarlos por terminal, log_level info, mostrara mensajes de info/warnings/debug
                                                                                   
    if (logger == NULL)
    {
        perror("Error al iniciar el logger de storage");
        return EXIT_FAILURE;
    }

    // arranque del servidor
    // int server_fd = iniciar_servidor_storage();

    int server_fd = iniciar_servidor(storage_config->PUERTO_ESCUCHA);

    if (server_fd == -1)
    {
        log_error(logger, "Error no se pudo levantar el servidor");
        destruir_configuracion();
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    log_info(logger, "STORAGE listo para reecibir clientes");
    // bucle para aceptar conexiones y lanzar hilos
    esperar_cliente_storage(server_fd);

    // Limpieza final
    // close(server_fd); --> se haace con liberar conexion (esta en   utisl)
    liberar_conexion(server_fd);
    destruir_configuracion();
    log_destroy(logger);

    return EXIT_SUCCESS;
}
