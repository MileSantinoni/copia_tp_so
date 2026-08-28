#include <cliente.h>
#include <servidor.h>

// Varibales de configuracion

t_log *logger;
t_log_level log_level;
t_config *config;

// Variables de conexión

// int socket_core;

int main(int argc, char *argv[])
{

    saludar("planificador");

    if (argc < 3)
    {
        printf("Uso correcto: ./bin/planificador [Archivo Config] [Path Job Inicial]\n");
        exit(EXIT_FAILURE);
    }

    config = config_create(argv[1]);

    if (config == NULL)
    {
        printf("No se pudo cargar el config en la ruta: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    log_level = log_level_from_string(config_get_string_value(config, "LOG_LEVEL"));
    logger = log_create("planificador.log", "Planificador", 1, log_level);

    /*if (logger == NULL)
    {
        printf("Error en la creación el logger");
        exit(EXIT_FAILURE);
    }*/

    // Parte cliente
    iniciar_conexiones(config);
    iniciar_hilo_escucha();

    // Parte servidor

    iniciar_conexion_como_servidor();

    return 0;
}