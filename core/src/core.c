#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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



t_log *logger = NULL;
t_config* config = NULL; 

int hay_interrupcion = 0; // 0 false || 1 true


void ciclo_de_instruccion (int socket_dispatch, int socket_placa, uint32_t jid){// denominado fetch, se encarga de captar la proxima instruccion
   int ejecutando = 1; // seteamos en true
    while(ejecutando){ // simil bucle infinito, siempre y cuando ejecutando valga 1
    // fase fetch
    log_info(logger, "## FETCH - PID: %d - PC: %d", jid, registros_cpu.PC);
    // pido la instruccion a placa indicando el jid y el PC actual
    t_paquete * paquete_fetch = crear_paquete(FETCH_INSTRUCCION);  //crea la estructura del paquete en memoria dinamica
    agregar_a_paquete(paquete_fetch, &jid, sizeof(uint32_t)); // copia el dato real dentro del paquete y actualiza el tamaño total
    agregar_a_paquete(paquete_fetch, &(registros_cpu.PC), sizeof(uint32_t));
    enviar_paquete(paquete_fetch, socket_placa);
    eliminar_paquete(paquete_fetch);

    // esperamos la respuesta de la placa
    int cod_op = recibir_operacion(socket_placa);
    if(cod_op != RESPUESTA_INSTRUCCION){
        log_error(logger, "Error al recibir instruccion de Placa. Codigo recibido: %d", cod_op);
        break;
    }
    // recibimos el string de la instruccion
    int size = 0;
    char * instruccion = recibir_buffer(&size, socket_placa);
    log_info(logger, "## Instruccion recibida: %s", instruccion);

    //  fase decode y execute

    registros_cpu.PC ++; // incremento el pc para que apunte a la siguiente instruccion

    free(instruccion);

    // chequeamos interrupt 
    if(hay_interrupcion){
        log_info(logger, "## Interrupción detectada. Desalojando proceso JID: %d...", jid);
        hay_interrupcion = 0;
        ejecutando = 0;

        // devuelvo el contexto actualizado al planificador por desalojo
        enviar_contexto(socket_dispatch, DESALOJAR_JOB, jid, &registros_cpu);
    }


    }
}  

void setear_valor_registro (char * nombre_registro, uint32_t valor){
    if(strcmp(nombre_registro, "PC")== 0) { registros_cpu.PC = valor; return;} // extraemos el valor y lo guardamos en el registro que segun matcheen
    if(strcmp(nombre_registro, "AX")== 0) { registros_cpu.AX = valor; return;}
    if(strcmp(nombre_registro, "BX")== 0) { registros_cpu.BX = valor; return;}
    if(strcmp(nombre_registro, "CX")== 0) { registros_cpu.CX = valor; return;}
    if(strcmp(nombre_registro, "DX")== 0) { registros_cpu.DX = valor; return;}
    if(strcmp(nombre_registro, "P0")== 0) { registros_cpu.P0 = valor; return;}
    if(strcmp(nombre_registro, "P1")== 0) { registros_cpu.P1 = valor; return;}
    if(strcmp(nombre_registro, "P2")== 0) { registros_cpu.P2 = valor; return;}
    if(strcmp(nombre_registro, "P3")== 0) { registros_cpu.P3 = valor; return;}
    if(strcmp(nombre_registro, "P4")== 0) { registros_cpu.P4 = valor; return;}
    if(strcmp(nombre_registro, "P5")== 0) { registros_cpu.P5 = valor; return;}
    if(strcmp(nombre_registro, "E1")== 0) { registros_cpu.E1 = valor; return;}
    if(strcmp(nombre_registro, "E2")== 0) { registros_cpu.E2 = valor; return;}
    if(strcmp(nombre_registro, "E3")== 0) { registros_cpu.E3 = valor; return;}
}


int main(int argc, char* argv[]) {
    
    if (argc < 3) {
        printf("Uso: %s [Archivo Config] [Identificador]\n", argv[0]);
        return EXIT_FAILURE;
    }

    char* path_config = argv[1]; //osea que el config es el primer argumento
    char* id_core = argv[2]; //y el id del core el 2do

    
    config = config_create(path_config);
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
    int socket_planificador = conectar_con_planificador(ip_planificador, puerto_planificador, atoi(id_core));
    if (socket_planificador == -1) {
        terminar_modulo();
        return EXIT_FAILURE;
    }
    

    // conectar a la placa
    int socket_placa = conectar_con_placa(ip_placa, puerto_placa, atoi(id_core));
    if (socket_placa == -1) {
        liberar_conexion(socket_planificador);
        terminar_modulo();
        return EXIT_FAILURE;
    }

    //paquete serializado de prueba
    /*t_paquete* paquete_prueba = crear_paquete(CO);
    char* msg = "Paquete de prueba Checkpoint 1";
    agregar_a_paquete(paquete_prueba, msg, strlen(msg) + 1);
    agregar_a_paquete(paquete_prueba, id_core, strlen(id_core) + 1);
    enviar_paquete(paquete_prueba, socket_planificador);
    eliminar_paquete(paquete_prueba);*/

    crear_hilo_escucha();
    while (1)
    {
       int cod_op = recibir_operacion(socket_planificador); // asigno lo que recibo del planificador, recibo la operacion en este caso
       if( cod_op == -1){ // si el codigo op es -1 entonces esto quiere decir que el planificador no se conecto correctamente
        log_error(logger, "El Planificador se desconecto.");
        break;  
      
      }
      switch(cod_op){
        case DESPACHAR_JOB:{
            uint32_t jid;
            t_registros_cpu contexto_recibido;

            recibir_contexto (socket_planificador, &jid, &contexto_recibido); // core extrae paquete de jid y su estado anterior 
            log_info (logger, "## Proceso despachado - JID: %d", jid);

            cargar_contexto (&contexto_recibido); // volcamos los datos obtenidos a la variable global registros_cpu
            ciclo_de_instruccion(socket_planificador,socket_placa, jid); // el core entra a ejecutar las instrucciones del proceso en un bucle local 
            break;
        }
        default: // el core recibio un codigo de operacion que el no esperaba del planificador por ese socket
            log_warning(logger, "Operación no reconocida del Planificador: %d", cod_op);
            break; 
      }
    
    }
    


    escuchar_servidores(NULL);

    
    liberar_conexion(socket_planificador);
    liberar_conexion(socket_placa);
    terminar_modulo();

    return EXIT_SUCCESS;
}