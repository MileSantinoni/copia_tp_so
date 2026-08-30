#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>

#include <utils/conexion.h>
#include <utils/comunicacion.h>
#include <utils/protocolo.h>


t_log *logger = NULL;

int enviar_handshake_core(int socket_destino) {
    t_paquete* paquete = crear_paquete(CONEXION);
    t_modulo modulo = CORE;
    agregar_a_paquete(paquete, &modulo, sizeof(t_modulo));
    enviar_paquete(paquete, socket_destino);
    eliminar_paquete(paquete);
    return 0;
}

int main(int argc, char* argv[]) {
    
    if (argc < 3) {
        printf("Uso: %s [Archivo Config] [Identificador]\n", argv[0]);
        return EXIT_FAILURE;
    }

    char* path_config = argv[1];
    char* id_core = argv[2];

    
    t_config* config = config_create(path_config);
    if (config == NULL) {
        perror("Error al abrir el archivo de configuracion");
        return EXIT_FAILURE;
    }

    char* ip_planificador = config_get_string_value(config, "IP_PLANIFICADOR");
    char* puerto_planificador = config_get_string_value(config, "PUERTO_PLANIFICADOR");
    char* ip_placa = config_get_string_value(config, "IP_PLACA");
    char* puerto_placa = config_get_string_value(config, "PUERTO_PLACA");
    char* log_level_str = config_get_string_value(config, "LOG_LEVEL");

    if (!ip_planificador || !puerto_planificador || !ip_placa || !puerto_placa || !log_level_str) {
        fprintf(stderr, "Error: Faltan claves en el archivo de configuracion.\n");
        config_destroy(config);
        return EXIT_FAILURE;
    }

    // crear Logger (core_<ID>.log)
    char* log_filename = string_from_format("core_%s.log", id_core);
    t_log_level nivel_log = log_level_from_string(log_level_str);
    logger = log_create(log_filename, "CORE", true, nivel_log);
    free(log_filename);

    if (logger == NULL) {
        fprintf(stderr, "Error al inicializar el logger.\n");
        config_destroy(config);
        return EXIT_FAILURE;
    }

    log_info(logger, "Iniciando modulo CORE [%s]", id_core);

    // conecta al planificador
    log_info(logger, "Intentando conectar con Planificador (%s:%s)...", ip_planificador, puerto_planificador);
    int socket_planificador = crear_conexion(ip_planificador, puerto_planificador);
    if (socket_planificador == -1) {
        log_error(logger, "No se pudo conectar con el Planificador en %s:%s", ip_planificador, puerto_planificador);
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    enviar_handshake_core(socket_planificador);
    
    log_info(logger, "## Conectado a Planificador exitosamente");

    // conectar a la placa
    log_info(logger, "Intentando conectar con Placa (%s:%s)...", ip_placa, puerto_placa);
    int socket_placa = crear_conexion(ip_placa, puerto_placa);
    if (socket_placa == -1) {
        log_error(logger, "No se pudo conectar con la Placa en %s:%s", ip_placa, puerto_placa);
        liberar_conexion(socket_planificador);
        config_destroy(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    enviar_handshake_core(socket_placa);
    
    log_info(logger, "## Conectado a Placa exitosamente");

    //paquete serializado de prueba
    t_paquete* paquete_prueba = crear_paquete(PAQUETE);
    char* msg = "Paquete de prueba Checkpoint 1";
    agregar_a_paquete(paquete_prueba, msg, strlen(msg) + 1);
    agregar_a_paquete(paquete_prueba, id_core, strlen(id_core) + 1);
    enviar_paquete(paquete_prueba, socket_planificador);
    eliminar_paquete(paquete_prueba);

    log_info(logger, "Paquete de prueba enviado al Planificador.");

    liberar_conexion(socket_planificador);
    liberar_conexion(socket_placa);
    config_destroy(config);
    log_destroy(logger);

    return EXIT_SUCCESS;
}