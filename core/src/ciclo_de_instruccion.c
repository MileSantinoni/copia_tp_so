#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "core.h"
#include "registro.h"
#include "ciclo_de_instruccion.h"
#include "registro.h"
#include <utils/comunicacion.h>
#include <utils/protocolo.h>



void ciclo_de_instruccion(int socket_dispatch, int socket_placa, uint32_t jid)
{                       // denominado fetch, se encarga de captar la proxima instruccion
    int ejecutando = 1; // seteamos en true
    while (ejecutando)
    { // simil bucle infinito, siempre y cuando ejecutando valga 1
        // fase fetch
        uint32_t pc_anterior = registros_cpu.PC;

        log_info(logger, "## FETCH - PID: %d - PC: %d", jid, registros_cpu.PC);
        // pido la instruccion a placa indicando el jid y el PC actual
        t_paquete *paquete_fetch = crear_paquete(FETCH_INSTRUCCION); // crea la estructura del paquete en memoria dinamica
        agregar_a_paquete(paquete_fetch, &jid, sizeof(uint32_t));    // copia el dato real dentro del paquete y actualiza el tamaño total
        agregar_a_paquete(paquete_fetch, &(registros_cpu.PC), sizeof(uint32_t));
        enviar_paquete(paquete_fetch, socket_placa);
        eliminar_paquete(paquete_fetch);

        // esperamos la respuesta de la placa
        int cod_op = recibir_operacion(socket_placa);

        if (cod_op == -1) {
            log_error(logger, "La Placa se desconectó");
            break;
        }

        if (cod_op != RESPUESTA_INSTRUCCION)
        {
            log_error(logger, "Error al recibir instruccion de Placa. Codigo recibido: %d", cod_op);
            break;
        }
        // recibimos el string de la instruccion
        int size = 0;
        char *instruccion = recibir_buffer(&size, socket_placa);

        if (instruccion == NULL) {
            log_error(logger, "La Placa devolvió una instrucción nula");
            break;
        }

        
        log_info(logger, "## Instruccion recibida: %s", instruccion);

        //  fase decode y execute

        t_instruccion *instruccion_decodificada =
            decodificar_instruccion(instruccion);

        free(instruccion);

        if (instruccion_decodificada == NULL) {
            log_error(logger, "No se pudo decodificar la instrucción");
            break;
        }

        t_resultado_ejecucion resultado =
            ejecutar_instruccion(instruccion_decodificada, jid);

        destruir_instruccion(instruccion_decodificada);


        if (resultado == EJECUCION_FINALIZADA) {
            log_info(
                logger,
                "## JID: %u - Finalizó por instrucción EXIT",
                jid
            );

            enviar_contexto(
                socket_dispatch,
                FINALIZAR_JOB,
                jid,
                &registros_cpu
            );

            ejecutando = 0;
            continue;
        }

        //si ninguna instruccion modifico el PC, avanzamos a la siguiete
        //si JNZ esta tomado el PC cambia al destino y no incrementa
        //sino PC sigue igual e incrementa en 1

        if (registros_cpu.PC == pc_anterior) {
            registros_cpu.PC++;
        }

        verificar_interrupcion(socket_dispatch);


        // registros_cpu.PC++; // incremento el pc para que apunte a la siguiente instruccion

        // free(instruccion);

        // chequeamos interrupt
        if (hay_interrupcion)
        {
            log_info(logger, "## Interrupción detectada. Desalojando proceso JID: %d...", jid);
            hay_interrupcion = 0;
            ejecutando = 0;

            // devuelvo el contexto actualizado al planificador por desalojo
            enviar_contexto(socket_dispatch, DEVOLVER_CONTEXTO, jid, &registros_cpu);
            
        }
    }
}



t_instruccion *decodificar_instruccion(char *linea)
{
    if (linea == NULL || strlen(linea) == 0) {
        return NULL;
    }

    t_instruccion *instruccion = malloc(sizeof(t_instruccion));

    instruccion->nombre = NULL;
    instruccion->parametros = NULL;
    instruccion->cantidad_parametros = 0;

    char *linea_copia = strdup(linea);
    char *token = strtok(linea_copia, " \t\n");

    if (token == NULL) {
        free(linea_copia);
        free(instruccion);
        return NULL;
    }

    instruccion->nombre = strdup(token);

    while ((token = strtok(NULL, " \t\n")) != NULL) {
        char **nuevos_parametros = realloc(
            instruccion->parametros,
            sizeof(char *) * (instruccion->cantidad_parametros + 1)
        );

        instruccion->parametros = nuevos_parametros;
        instruccion->parametros[instruccion->cantidad_parametros] = strdup(token);
        instruccion->cantidad_parametros++;
    }

    free(linea_copia);

    return instruccion;
}




void destruir_instruccion(t_instruccion *instruccion)
{
    if (instruccion == NULL) {
        return;
    }

    free(instruccion->nombre);

    for (int i = 0; i < instruccion->cantidad_parametros; i++) {
        free(instruccion->parametros[i]);
    }

    free(instruccion->parametros);
    free(instruccion);
}


uint32_t obtener_valor_operando(char *operando)
{
    if (strcmp(operando, "PC") == 0 ||
        strcmp(operando, "AX") == 0 ||
        strcmp(operando, "BX") == 0 ||
        strcmp(operando, "CX") == 0 ||
        strcmp(operando, "DX") == 0 ||
        strcmp(operando, "P0") == 0 ||
        strcmp(operando, "P1") == 0 ||
        strcmp(operando, "P2") == 0 ||
        strcmp(operando, "P3") == 0 ||
        strcmp(operando, "P4") == 0 ||
        strcmp(operando, "P5") == 0 ||
        strcmp(operando, "E1") == 0 ||
        strcmp(operando, "E2") == 0 ||
        strcmp(operando, "E3") == 0) {
        
        return obtener_valor_registro(operando);
    }

    return (uint32_t) strtoul(operando, NULL, 10);
}


t_resultado_ejecucion ejecutar_instruccion(
    t_instruccion *instruccion,
    uint32_t jid
)
{
    if (instruccion == NULL) {
        return EJECUCION_CONTINUA;
    }

    log_info(
        logger,
        "## JID: %u - Ejecutando: %s",
        jid,
        instruccion->nombre
    );

    if (strcmp(instruccion->nombre, "NOOP") == 0) {
        return EJECUCION_CONTINUA;
    }

    if (strcmp(instruccion->nombre, "EXIT") == 0) {
        return EJECUCION_FINALIZADA;
    }

    if (strcmp(instruccion->nombre, "SET") == 0) {
        if (instruccion->cantidad_parametros != 2) {
            log_error(logger, "SET requiere dos parámetros");
            return EJECUCION_CONTINUA;
        }

        char *registro_destino = instruccion->parametros[0];
        char *operando = instruccion->parametros[1];

        uint32_t valor = obtener_valor_operando(operando);

        if (!asignar_valor_registro(registro_destino, valor)) {
            log_error(
                logger,
                "## JID: %u - Registro desconocido: %s",
                jid,
                registro_destino
            );
        }

        return EJECUCION_CONTINUA;
    }

    if (strcmp(instruccion->nombre, "SUM") == 0) {
        if (instruccion->cantidad_parametros != 2) {
            log_error(logger, "SUM requiere dos parámetros");
            return EJECUCION_CONTINUA;
        }

        char *registro_destino = instruccion->parametros[0];
        char *operando = instruccion->parametros[1];

        uint32_t actual = obtener_valor_registro(registro_destino);
        uint32_t valor = obtener_valor_operando(operando);

        if (!asignar_valor_registro(registro_destino, actual + valor)) {
            log_error(
                logger,
                "## JID: %u - Registro desconocido: %s",
                jid,
                registro_destino
            );
        }

        return EJECUCION_CONTINUA;
    }

    if (strcmp(instruccion->nombre, "SUB") == 0) {
        if (instruccion->cantidad_parametros != 2) {
            log_error(logger, "SUB requiere dos parámetros");
            return EJECUCION_CONTINUA;
        }

        char *registro_destino = instruccion->parametros[0];
        char *operando = instruccion->parametros[1];

        uint32_t actual = obtener_valor_registro(registro_destino);
        uint32_t valor = obtener_valor_operando(operando);

        if (!asignar_valor_registro(registro_destino, actual - valor)) {
            log_error(
                logger,
                "## JID: %u - Registro desconocido: %s",
                jid,
                registro_destino
            );
        }

        return EJECUCION_CONTINUA;
    }

    if (strcmp(instruccion->nombre, "JNZ") == 0) {
        if (instruccion->cantidad_parametros != 2) {
            log_error(logger, "JNZ requiere dos parámetros");
            return EJECUCION_CONTINUA;
        }

        char *registro = instruccion->parametros[0];
        uint32_t destino = (uint32_t) strtoul(
            instruccion->parametros[1],
            NULL,
            10
        );

        uint32_t valor = obtener_valor_registro(registro);

        if (valor != 0) {
            registros_cpu.PC = destino;

            log_info(
                logger,
                "## JID: %u - JNZ salta a PC: %u",
                jid,
                destino
            );
        }

        return EJECUCION_CONTINUA;
    }

    log_warning(
        logger,
        "## JID: %u - Instrucción no implementada: %s",
        jid,
        instruccion->nombre
    );

    return EJECUCION_CONTINUA;
}

void verificar_interrupcion(int socket_planificador)
{
    fd_set sockets;
    struct timeval timeout;

    FD_ZERO(&sockets);
    FD_SET(socket_planificador, &sockets);

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    int resultado = select(
        socket_planificador + 1,
        &sockets,
        NULL,
        NULL,
        &timeout
    );

    if (resultado == -1)
    {
        if (errno == EINTR)
            return;

        log_error(logger, "Error en select() al verificar interrupción");
        return;
    }

    if (resultado > 0 &&
        FD_ISSET(socket_planificador, &sockets))
    {
        int cod_op = recibir_operacion(socket_planificador);

        if (cod_op == -1)
        {
            log_error(logger, "El Planificador se desconectó");
            return;
        }

        if (cod_op == INTERRUPCION)
        {
            int size;
            char *buffer = recibir_buffer(
                &size,
                socket_planificador
            );

            if (buffer == NULL)
            {
                log_error(
                    logger,
                    "No se pudo recibir el contenido de la interrupción"
                );
                return;
            }

            uint32_t jid_interrumpido;
            t_motivo_desalojo motivo;

            memcpy(
                &jid_interrumpido,
                buffer,
                sizeof(uint32_t)
            );

            memcpy(
                &motivo,
                buffer + sizeof(uint32_t),
                sizeof(t_motivo_desalojo)
            );

            free(buffer);

            log_info(
                logger,
                "## Interrupción recibida - JID: %u - Motivo: %d",
                jid_interrumpido,
                motivo
            );

            hay_interrupcion = 1;
        }
        else
        {
            log_warning(
                logger,
                "Operación inesperada durante ejecución: %d",
                cod_op
            );
        }
    }
}