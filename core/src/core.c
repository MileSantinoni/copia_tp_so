#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>

#include "core.h"
#include "conexiones.h"
#include "registro.h"
#include <utils/conexion.h>
#include <utils/comunicacion.h>
#include <utils/protocolo.h>
#include <utils/util.h>
#include "ciclo_de_instruccion.h"

t_log *logger = NULL;
t_config *config = NULL;

// int hay_interrupcion = 0; // 0 false || 1 true

int main(int argc, char *argv[])
{

    if (argc < 3)
    {
        printf("Uso: %s [Archivo Config] [Identificador]\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *path_config = argv[1]; // osea que el config es el primer argumento
    char *id_core = argv[2];     // y el id del core el 2do

    config = config_create(path_config);
    if (config == NULL)
    {
        perror("Error al abrir el archivo de configuracion");
        return EXIT_FAILURE;
    }

    char *ip_planificador = config_get_string_value(config, "IP_PLANIFICADOR");
    char *puerto_planificador = config_get_string_value(config, "PUERTO_PLANIFICADOR");
    char *ip_placa = config_get_string_value(config, "IP_PLACA");
    char *puerto_placa = config_get_string_value(config, "PUERTO_PLACA");
    char *log_level_str = config_get_string_value(config, "LOG_LEVEL");

    if (!ip_planificador || !puerto_planificador || !ip_placa || !puerto_placa || !log_level_str)
    {
        fprintf(stderr, "Error: Faltan claves en el archivo de configuracion.\n");
        config_destroy(config);
        return EXIT_FAILURE;
    }

    // crear Logger (core_<ID>.log)
    char *log_filename = string_from_format("core_%s.log", id_core);
    t_log_level nivel_log = log_level_from_string(log_level_str);
    logger = log_create(log_filename, "CORE", true, nivel_log);
    free(log_filename);

    if (logger == NULL)
    {
        fprintf(stderr, "Error al inicializar el logger.\n");
        config_destroy(config);
        return EXIT_FAILURE;
    }

    log_info(logger, "Iniciando modulo CORE [%s]", id_core);


    
    // 1. Conexion dispatch
    socket_dispatch = conectar_con_planificador(ip_planificador, puerto_planificador, atoi(id_core),CONEXION_DISPATCH);
    if (socket_dispatch == -1)
    {
        terminar_modulo();
        return EXIT_FAILURE;
    }

    // 2. Conexion interrupt
    socket_interrupt = conectar_con_planificador(ip_planificador, puerto_planificador, atoi(id_core),CONEXION_INTERRUPT);
    if (socket_interrupt == -1)
    {
        liberar_conexion(socket_dispatch);
        terminar_modulo();
        return EXIT_FAILURE;
    }

    // 3. Conexion con placa
    socket_placa = conectar_con_placa(ip_placa, puerto_placa, atoi(id_core));
    if (socket_placa == -1)
    {
        liberar_conexion(socket_dispatch);
        liberar_conexion(socket_interrupt);
        terminar_modulo();
        return EXIT_FAILURE;
    }

    interrupciones_pendientes = list_create();


    crear_hilo_escucha();
    while (1)
    {
        int cod_op = recibir_operacion(socket_dispatch); // asigno lo que recibo del planificador, recibo la operacion en este caso
        if (cod_op == -1) // si el codigo op es -1 entonces esto quiere decir que el planificador no se conecto correctamente
        { 
            log_error(logger, "El Planificador se desconecto.");
            break;
        }
        switch (cod_op)
        {
        case DESPACHAR_JOB:
        {
            uint32_t jid;
            t_registros contexto_recibido;

            recibir_contexto(socket_dispatch, &jid, &contexto_recibido); // core extrae paquete de jid y su estado anterior

            cargar_contexto(&contexto_recibido); 
            
            descartar_interrupciones_de_otros_jids(jid);

            log_info(logger, "## Proceso despachado - JID: %u", jid);
            
            // volcamos los datos obtenidos a la variable global registros_cpu
            ciclo_de_instruccion(socket_dispatch, socket_placa, jid); // el core entra a ejecutar las instrucciones del proceso en un bucle local
            break;
        }
        default: // el core recibio un codigo de operacion que el no esperaba del planificador por ese socket
            log_warning(logger, "Operación no reconocida del Planificador: %d", cod_op);
            break;
        }
    }


    liberar_conexion(socket_dispatch);
    liberar_conexion(socket_interrupt);
    liberar_conexion(socket_placa);
    list_destroy_and_destroy_elements(interrupciones_pendientes, free);
    terminar_modulo();

    return EXIT_SUCCESS;
}