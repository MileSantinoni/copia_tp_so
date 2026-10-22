#include "placa.h"
#include "memoria_virtual.h"
#include "servidor.h"
#include "memoria.h"

#include <utils/conexion.h>
#include <utils/comunicacion.h>
#include <utils/protocolo.h>
#include <utils/util.h>

// Definición de las variables globales declaradas como extern en el .h
t_log *logger;
t_config *config;
t_config_placa config_valores;


int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Uso correcto: ./bin/placa [Archivo Config] \n");
        exit(EXIT_FAILURE);
    }

    char *path_config = (argc > 1) ? argv[1] : "placa.config";

    inicializar_placa(path_config);

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
    diccionario_jobs = dictionary_create();

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

    //Inicializacion de la memoria fisica y tabla de marcos
    inicializar_memoria_fisica();
    inicializar_offload();
}






