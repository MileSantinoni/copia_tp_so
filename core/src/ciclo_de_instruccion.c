#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "core.h"
#include "conexiones.h"
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

        
        int cod_op = recibir_operacion(socket_placa);

        if (cod_op != MENSAJE)
        {
            log_error(logger, "Se esperaba MENSAJE de Placa pero llego op_code: %d", cod_op);
            break;
        }

        int size = 0;
        char *instruccion = recibir_buffer(&size, socket_placa);

        if (strcmp(instruccion, "ERROR") == 0)
        {
            log_error(logger, "No se recibio la instrucción correctamente");
            break;
        }

    
        log_info(logger, "## Instruccion recibida: %s", instruccion);

        //  fase decode y execute

        t_instruccion *instruccion_decodificada =
            decodificar_instruccion(instruccion);

        free(instruccion);

        if (instruccion_decodificada == NULL)
        {
            log_error(logger, "No se pudo decodificar la instrucción");
            break;
        }

        t_resultado_ejecucion resultado =
            ejecutar_instruccion(instruccion_decodificada, jid);

        destruir_instruccion(instruccion_decodificada);

        if (resultado == EJECUCION_FINALIZADA)
        {
            log_info(
                logger,
                "## JID: %u - Finalizó por instrucción EXIT",
                jid);

            enviar_contexto(
                socket_dispatch,
                FINALIZAR_JOB,
                jid,
                &registros_cpu);

            // Lo agregue para ver si terminaban bien los registros
            /*log_info(logger, "REGISTROS \n AX=%u \n BX=%u \n CX=%u \n DX=%u \n E1=%u  \n E2=%u \n E3=%u \n P0=%u \n P1=%u \n P2=%u \n P3=%u \n P4=%u  \n" ,
                registros_cpu.AX, registros_cpu.BX, registros_cpu.CX, registros_cpu.DX,
                registros_cpu.E1, registros_cpu.E2, registros_cpu.E3,
                registros_cpu.P0, registros_cpu.P1, registros_cpu.P2, registros_cpu.P3, registros_cpu.P4,registros_cpu.P5);*/

            ejecutando = 0;
            continue;
        }

        // si ninguna instruccion modifico el PC, avanzamos a la siguiete
        // si JNZ esta tomado el PC cambia al destino y no incrementa
        // sino PC sigue igual e incrementa en 1

        t_motivo_desalojo motivo;

        if (registros_cpu.PC == pc_anterior)
        {
            registros_cpu.PC++;
        }

        // chequeamos interrupt
        
        if (tomar_interrupcion_para_jid(jid, &motivo)) {
            log_info(logger, "## Interrupción válida para JID: %u - Motivo: %d", jid, motivo);


            // devuelvo el contexto actualizado al planificador por desalojo
            // TODO revisar si acá iría un envio con motivo 
            enviar_contexto(socket_dispatch, DESALOJAR_JOB, jid, &registros_cpu);
            ejecutando = 0;
        }
        
    }
}

t_instruccion *decodificar_instruccion(char *linea)
{
    if (linea == NULL || strlen(linea) == 0)
    {
        return NULL;
    }

    t_instruccion *instruccion = malloc(sizeof(t_instruccion));

    instruccion->nombre = NULL;
    instruccion->parametros = NULL;
    instruccion->cantidad_parametros = 0;

    char *linea_copia = strdup(linea);
    char *token = strtok(linea_copia, " \t\n");

    if (token == NULL)
    {
        free(linea_copia);
        free(instruccion);
        return NULL;
    }

    instruccion->nombre = strdup(token);

    while ((token = strtok(NULL, " \t\n")) != NULL)
    {
        char **nuevos_parametros = realloc(
            instruccion->parametros,
            sizeof(char *) * (instruccion->cantidad_parametros + 1));

        instruccion->parametros = nuevos_parametros;
        instruccion->parametros[instruccion->cantidad_parametros] = strdup(token);
        instruccion->cantidad_parametros++;
    }

    free(linea_copia);

    return instruccion;
}

void destruir_instruccion(t_instruccion *instruccion)
{
    if (instruccion == NULL)
    {
        return;
    }

    free(instruccion->nombre);

    for (int i = 0; i < instruccion->cantidad_parametros; i++)
    {
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
        strcmp(operando, "E3") == 0)
    {

        return obtener_valor_registro(operando);
    }

    return (uint32_t)strtoul(operando, NULL, 10);
}

t_resultado_ejecucion ejecutar_instruccion(
    t_instruccion *instruccion,
    uint32_t jid)
{
    if (instruccion == NULL)
    {
        return EJECUCION_CONTINUA;
    }

    log_info(
        logger,
        "## JID: %u - Ejecutando: %s",
        jid,
        instruccion->nombre);

    if (strcmp(instruccion->nombre, "NOOP") == 0)
    {
        return EJECUCION_CONTINUA;
    }

    if (strcmp(instruccion->nombre, "EXIT") == 0)
    {
        return EJECUCION_FINALIZADA;
    }

    if (strcmp(instruccion->nombre, "SET") == 0)
    {
        if (instruccion->cantidad_parametros != 2)
        {
            log_error(logger, "SET requiere dos parámetros");
            return EJECUCION_CONTINUA;
        }

        char *registro_destino = instruccion->parametros[0];
        char *operando = instruccion->parametros[1];

        uint32_t valor = obtener_valor_operando(operando);

        if (!asignar_valor_registro(registro_destino, valor))
        {
            log_error(
                logger,
                "## JID: %u - Registro desconocido: %s",
                jid,
                registro_destino);
        }

        return EJECUCION_CONTINUA;
    }

    if (strcmp(instruccion->nombre, "SUM") == 0)
    {
        if (instruccion->cantidad_parametros != 2)
        {
            log_error(logger, "SUM requiere dos parámetros");
            return EJECUCION_CONTINUA;
        }

        char *registro_destino = instruccion->parametros[0];
        char *operando = instruccion->parametros[1];

        uint32_t actual = obtener_valor_registro(registro_destino);
        uint32_t valor = obtener_valor_operando(operando);

        if (!asignar_valor_registro(registro_destino, actual + valor))
        {
            log_error(
                logger,
                "## JID: %u - Registro desconocido: %s",
                jid,
                registro_destino);
        }

        return EJECUCION_CONTINUA;
    }

    if (strcmp(instruccion->nombre, "SUB") == 0)
    {
        if (instruccion->cantidad_parametros != 2)
        {
            log_error(logger, "SUB requiere dos parámetros");
            return EJECUCION_CONTINUA;
        }

        char *registro_destino = instruccion->parametros[0];
        char *operando = instruccion->parametros[1];

        uint32_t actual = obtener_valor_registro(registro_destino);
        uint32_t valor = obtener_valor_operando(operando);

        if (!asignar_valor_registro(registro_destino, actual - valor))
        {
            log_error(
                logger,
                "## JID: %u - Registro desconocido: %s",
                jid,
                registro_destino);
        }

        return EJECUCION_CONTINUA;
    }

    if (strcmp(instruccion->nombre, "JNZ") == 0)
    {
        if (instruccion->cantidad_parametros != 2)
        {
            log_error(logger, "JNZ requiere dos parámetros");
            return EJECUCION_CONTINUA;
        }

        char *registro = instruccion->parametros[0];
        uint32_t destino = (uint32_t)strtoul(
            instruccion->parametros[1],
            NULL,
            10);

        uint32_t valor = obtener_valor_registro(registro);

        if (valor != 0)
        {
            registros_cpu.PC = destino;

            log_info(
                logger,
                "## JID: %u - JNZ salta a PC: %u",
                jid,
                destino);
        }

        return EJECUCION_CONTINUA;
    }

    log_warning(
        logger,
        "## JID: %u - Instrucción no implementada: %s",
        jid,
        instruccion->nombre);

    return EJECUCION_CONTINUA;
}
