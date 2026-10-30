#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <utils/util.h>
#include <commons/string.h>

#include "core.h"
#include "conexiones.h"
#include "registro.h"
#include "ciclo_de_instruccion.h"
#include "registro.h"
#include "mmu.h"
#include <utils/comunicacion.h>
#include <utils/protocolo.h>

void ciclo_de_instruccion(int socket_dispatch, int socket_placa, uint32_t jid)
{                       // denominado fetch, se encarga de captar la proxima instruccion
    int ejecutando = 1; // seteamos en true
    while (ejecutando)
    { // simil bucle infinito, siempre y cuando ejecutando valga 1
        // fase fetch
        uint32_t pc_anterior = registros_cpu.PC;

        log_info(logger, "## JID: %u - FETCH - Program Counter: %u", jid, registros_cpu.PC);
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

        // acá debería ir forward, backward y update
        // TODO

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
            ejecutar_instruccion(instruccion_decodificada, jid, socket_placa, socket_dispatch);

        destruir_instruccion(instruccion_decodificada);

        if (resultado == EJECUCION_PAGE_FAULT)
        {
            log_info(
                logger,
                "## JID: %u - Devolviendo contexto por Page Fault en PC: %u",
                jid,
                registros_cpu.PC);

            mmu_deslockear_paginas(socket_placa, jid);

            enviar_desalojo_page_fault(
                socket_dispatch,
                jid,
                &registros_cpu,
                mmu_get_pagina_page_fault());

            ejecutando = 0;
            continue;
        }

        if (resultado == EJECUCION_ERROR)
        {
            log_error(
                logger,
                "## JID: %u - Error ejecutando instrucción",
                jid);

            mmu_deslockear_paginas(
                socket_placa,
                jid);

            ejecutando = 0;
            continue;
        }

        if (resultado == EJECUCION_BLOQUEADA)
        {
            if (registros_cpu.PC == pc_anterior)
                registros_cpu.PC++;
            ejecutando = 0;
            continue;
        }

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

        if (tomar_interrupcion_para_jid(jid, &motivo))
        {
            log_info(logger, "## Interrupción válida para JID: %u - Motivo: %s", jid, motivo_de_desalojo_a_string(motivo));

            // devuelvo el contexto actualizado al planificador por desalojo
            // TODO revisar si acá iría un envio con motivo
            enviar_desalojo(socket_dispatch, jid, &registros_cpu, motivo);
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
static t_resultado_ejecucion preparar_accesos(
    int socket_placa,
    uint32_t jid,
    const uint32_t *direcciones,
    size_t cantidad)
{
    for (size_t i = 0; i < cantidad; i++)
    {
        t_list *segmentos = NULL;
        t_resultado_mmu resultado = mmu_traducir_rango(
            socket_placa,
            jid,
            direcciones[i],
            sizeof(uint32_t),
            &segmentos);
        mmu_destruir_segmentos(segmentos);
        if (resultado != MMU_OK)
        {
            return resultado == MMU_PAGE_FAULT
                       ? EJECUCION_PAGE_FAULT
                       : EJECUCION_ERROR;
        }
    }
    return EJECUCION_CONTINUA;
}

static t_resultado_ejecucion ejecutar_syscall_memoria(
    t_instruccion *instruccion,
    uint32_t jid,
    int socket_placa,
    int socket_dispatch,
    t_motivo_desalojo motivo)
{
    uint32_t direccion;
    uint32_t tamanio;
    char *nombre = NULL;
    if (motivo == SYSCALL_LOAD_BATCH)
    {
        if (instruccion->cantidad_parametros != 3)
        {
            log_error(logger, "LOAD_BATCH requiere archivo, dirección y tamaño");
            return EJECUCION_ERROR;
        }
        nombre = instruccion->parametros[0];
        direccion = obtener_valor_operando(instruccion->parametros[1]);
        tamanio = obtener_valor_operando(instruccion->parametros[2]);
    }
    else
    {
        if (instruccion->cantidad_parametros != 2)
        {
            log_error(
                logger,
                "%s requiere dirección y tamaño",
                motivo == SYSCALL_LABEL ? "LABEL" : "REPORT");
            return EJECUCION_ERROR;
        }
        direccion = obtener_valor_operando(instruccion->parametros[0]);
        tamanio = obtener_valor_operando(instruccion->parametros[1]);
    }
    if (tamanio == 0)
    {
        log_error(logger, "## JID: %u - Tamaño de syscall inválido", jid);
        return EJECUCION_ERROR;
    }
    t_list *segmentos = NULL;
    t_resultado_mmu resultado = mmu_traducir_rango(
        socket_placa,
        jid,
        direccion,
        tamanio,
        &segmentos);
    if (resultado != MMU_OK)
    {
        mmu_destruir_segmentos(segmentos);
        return resultado == MMU_PAGE_FAULT
                   ? EJECUCION_PAGE_FAULT
                   : EJECUCION_ERROR;
    }
    enviar_desalojo_syscall_memoria(
        socket_dispatch,
        jid,
        &registros_cpu,
        motivo,
        nombre,
        tamanio,
        segmentos);
    mmu_destruir_segmentos(segmentos);
    return EJECUCION_BLOQUEADA;
}

// INIT_JOB es una syscall no bloqueante: el Core se la pide al Planificador,
// espera la respuesta y sigue ejecutando el mismo Job (no devuelve el contexto).
static void syscall_init_job(uint32_t jid, char *archivo)
{
    // paso 1 le pido al planificador que cree el job [jid que la pide][nombre del archivo]
    t_paquete *paquete = crear_paquete(INIT_JOB);
    agregar_a_paquete(paquete, &jid, sizeof(uint32_t));
    agregar_a_paquete(paquete, archivo, strlen(archivo) + 1);
    enviar_paquete(paquete, socket_dispatch);
    eliminar_paquete(paquete);

    // 2. Espero la respuesta, recibir_operacion es bloqueante: el Core se queda
    //    acá hasta que el Planificador termine de crear el Job.
    int cod_op = recibir_operacion(socket_dispatch);
    if (cod_op != RESPUESTA_SYSCALL)
    {
        log_error(logger, "## JID: %u - Se esperaba RESPUESTA_SYSCALL y llegó op_code %d", jid, cod_op);
        return;
    }
    // 3 Leo el paquete de la respuesta para dejar el socket limpio
    t_list *respuesta = recibir_paquete(socket_dispatch);
    list_destroy_and_destroy_elements(respuesta, free);
}

t_resultado_ejecucion ejecutar_instruccion(
    t_instruccion *instruccion,
    uint32_t jid,
    int socket_placa,
    int socket_dispatch)
{
    if (instruccion == NULL)
    {
        return EJECUCION_CONTINUA;
    }

    // armo el string de parámetros separados por espacio
    char *parametros = string_new();
    for (int i = 0; i < instruccion->cantidad_parametros; i++)
    {
        string_append(&parametros, instruccion->parametros[i]);
        if (i < instruccion->cantidad_parametros - 1)
            string_append(&parametros, " ");
    }

    if (instruccion->cantidad_parametros > 0)
        log_info(logger, "## JID: %u - Ejecutando: %s - %s", jid, instruccion->nombre, parametros);
    else
        log_info(logger, "## JID: %u - Ejecutando: %s", jid, instruccion->nombre);

    free(parametros);

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
    if (strcmp(instruccion->nombre, "INIT_JOB") == 0)
    {
        if (instruccion->cantidad_parametros != 1)
        {
            log_error(logger, "INIT_JOB requiere un parámetro");
            return EJECUCION_CONTINUA;
        }

        syscall_init_job(jid, instruccion->parametros[0]);
        return EJECUCION_CONTINUA;
    }

    // TODO: revisar estas implementaciones de forward, backward y update
    if (strcmp(instruccion->nombre, "FORWARD") == 0)
    {
        if (instruccion->cantidad_parametros != 4)
        {
            log_error(logger, "FORWARD requiere 4 parámetros");
            return EJECUCION_ERROR;
        }

        uint32_t direccion_pesos =
            obtener_valor_operando(instruccion->parametros[0]);

        uint32_t direccion_entrada =
            obtener_valor_operando(instruccion->parametros[1]);

        uint32_t direccion_salida =
            obtener_valor_operando(instruccion->parametros[2]);

        uint32_t direccion_acumulador =
            obtener_valor_operando(instruccion->parametros[3]);

        const uint32_t direcciones[] = {
            direccion_pesos,
            direccion_entrada,
            direccion_acumulador,
            direccion_salida};
        t_resultado_ejecucion preparacion = preparar_accesos(
            socket_placa,
            jid,
            direcciones,
            sizeof(direcciones) / sizeof(direcciones[0]));
        if (preparacion != EJECUCION_CONTINUA)
        {
            mmu_deslockear_paginas(socket_placa, jid);
            return preparacion;
        }

        uint32_t pesos;
        uint32_t entrada;
        uint32_t acumulador;

        t_resultado_mmu resultado =
            mmu_leer_uint32(
                socket_placa,
                jid,
                direccion_pesos,
                &pesos);

        if (resultado == MMU_PAGE_FAULT)
        {
            mmu_deslockear_paginas(socket_placa, jid);
            return EJECUCION_PAGE_FAULT;
        }

        if (resultado != MMU_OK)
        {
            mmu_deslockear_paginas(socket_placa, jid);
            return EJECUCION_ERROR;
        }

        resultado =
            mmu_leer_uint32(
                socket_placa,
                jid,
                direccion_entrada,
                &entrada);

        // if (resultado == MMU_PAGE_FAULT)
        // {
        //     mmu_deslockear_paginas(socket_placa, jid);
        //     return EJECUCION_PAGE_FAULT;
        // }

        if (resultado != MMU_OK)
        {
            mmu_deslockear_paginas(socket_placa, jid);
            return EJECUCION_ERROR;
        }

        resultado =
            mmu_leer_uint32(
                socket_placa,
                jid,
                direccion_acumulador,
                &acumulador);

        //  if (resultado == MMU_PAGE_FAULT)
        // {
        //     mmu_deslockear_paginas(socket_placa, jid);
        //     return EJECUCION_PAGE_FAULT;
        // }

        if (resultado != MMU_OK)
        {
            mmu_deslockear_paginas(socket_placa, jid);
            return EJECUCION_ERROR;
        }

        uint32_t nuevo_acumulador =
            acumulador + pesos * entrada;

        resultado =
            mmu_escribir_uint32(
                socket_placa,
                jid,
                direccion_salida,
                nuevo_acumulador);

        // // if (resultado == MMU_PAGE_FAULT)
        // {
        //     mmu_deslockear_paginas(socket_placa, jid);
        //     return EJECUCION_PAGE_FAULT;
        // }

        if (resultado != MMU_OK)
        {
            mmu_deslockear_paginas(socket_placa, jid);
            return EJECUCION_ERROR;
        }

        registros_cpu.E1 = pesos;
        registros_cpu.E2 = entrada;
        registros_cpu.E3 = nuevo_acumulador;

        mmu_deslockear_paginas(socket_placa, jid);

        return EJECUCION_CONTINUA;
    }

    if (strcmp(instruccion->nombre, "BACKWARD") == 0)
    {
        if (instruccion->cantidad_parametros != 4)
        {
            log_error(logger, "BACKWARD requiere 4 parámetros");
            return EJECUCION_ERROR;
        }

        uint32_t direccion_pesos =
            obtener_valor_operando(instruccion->parametros[0]);

        uint32_t direccion_activaciones =
            obtener_valor_operando(instruccion->parametros[1]);

        uint32_t direccion_gradientes =
            obtener_valor_operando(instruccion->parametros[2]);

        uint32_t direccion_acumulador =
            obtener_valor_operando(instruccion->parametros[3]);

        const uint32_t direcciones[] = {
            direccion_pesos,
            direccion_activaciones,
            direccion_acumulador,
            direccion_gradientes};
        t_resultado_ejecucion preparacion = preparar_accesos(
            socket_placa,
            jid,
            direcciones,
            sizeof(direcciones) / sizeof(direcciones[0]));
        if (preparacion != EJECUCION_CONTINUA)
        {
            mmu_deslockear_paginas(socket_placa, jid);
            return preparacion;
        }

        uint32_t pesos;
        uint32_t activaciones;
        uint32_t acumulador;

        t_resultado_mmu resultado =
            mmu_leer_uint32(
                socket_placa,
                jid,
                direccion_pesos,
                &pesos);

        if (resultado != MMU_OK)
        {
            mmu_deslockear_paginas(socket_placa, jid);
            return resultado == MMU_PAGE_FAULT
                       ? EJECUCION_PAGE_FAULT
                       : EJECUCION_ERROR;
        }

        resultado =
            mmu_leer_uint32(
                socket_placa,
                jid,
                direccion_activaciones,
                &activaciones);

        if (resultado != MMU_OK)
        {
            mmu_deslockear_paginas(socket_placa, jid);
            return resultado == MMU_PAGE_FAULT
                       ? EJECUCION_PAGE_FAULT
                       : EJECUCION_ERROR;
        }

        resultado =
            mmu_leer_uint32(
                socket_placa,
                jid,
                direccion_acumulador,
                &acumulador);

        if (resultado != MMU_OK)
        {
            mmu_deslockear_paginas(socket_placa, jid);
            return resultado == MMU_PAGE_FAULT
                       ? EJECUCION_PAGE_FAULT
                       : EJECUCION_ERROR;
        }

        uint32_t nuevo_acumulador =
            acumulador - pesos * activaciones;

        resultado =
            mmu_escribir_uint32(
                socket_placa,
                jid,
                direccion_gradientes,
                nuevo_acumulador);

        if (resultado != MMU_OK)
        {
            mmu_deslockear_paginas(socket_placa, jid);
            return resultado == MMU_PAGE_FAULT
                       ? EJECUCION_PAGE_FAULT
                       : EJECUCION_ERROR;
        }

        registros_cpu.E1 = pesos;
        registros_cpu.E2 = activaciones;
        registros_cpu.E3 = nuevo_acumulador;

        mmu_deslockear_paginas(socket_placa, jid);

        return EJECUCION_CONTINUA;
    }

    if (strcmp(instruccion->nombre, "UPDATE") == 0)
    {
        if (instruccion->cantidad_parametros != 3)
        {
            log_error(logger, "UPDATE requiere 3 parámetros");
            return EJECUCION_ERROR;
        }

        uint32_t dir_gradientes = obtener_valor_registro(instruccion->parametros[0]);
        uint32_t dir_estado = obtener_valor_registro(instruccion->parametros[1]);
        uint32_t dir_pesos = obtener_valor_registro(instruccion->parametros[2]);

        // if (mmu_leer_uint32(socket_placa, jid, dir_gradientes, &registros_cpu.E1) == MMU_PAGE_FAULT)
        // {
        //     return EJECUCION_PAGE_FAULT;
        // }
        // if (mmu_leer_uint32(socket_placa, jid, dir_estado, &registros_cpu.E2) == MMU_PAGE_FAULT)
        // {
        //     return EJECUCION_PAGE_FAULT;
        // }
        // if (mmu_leer_uint32(socket_placa, jid, dir_pesos, &registros_cpu.E3) == MMU_PAGE_FAULT)
        // {
        //     return EJECUCION_PAGE_FAULT;
        // }

        const uint32_t direcciones[] = {
            dir_gradientes,
            dir_estado,
            dir_pesos};
        t_resultado_ejecucion preparacion = preparar_accesos(
            socket_placa,
            jid,
            direcciones,
            sizeof(direcciones) / sizeof(direcciones[0]));
        if (preparacion != EJECUCION_CONTINUA)
        {
            mmu_deslockear_paginas(socket_placa, jid);
            return preparacion;
        }
        t_resultado_mmu resultado = mmu_leer_uint32(
            socket_placa,
            jid,
            dir_gradientes,
            &registros_cpu.E1);
        if (resultado != MMU_OK)
        {
            mmu_deslockear_paginas(socket_placa, jid);
            return resultado == MMU_PAGE_FAULT
                       ? EJECUCION_PAGE_FAULT
                       : EJECUCION_ERROR;
        }
        resultado = mmu_leer_uint32(
            socket_placa,
            jid,
            dir_estado,
            &registros_cpu.E2);
        if (resultado != MMU_OK)
        {
            mmu_deslockear_paginas(socket_placa, jid);
            return resultado == MMU_PAGE_FAULT
                       ? EJECUCION_PAGE_FAULT
                       : EJECUCION_ERROR;
        }
        resultado = mmu_leer_uint32(
            socket_placa,
            jid,
            dir_pesos,
            &registros_cpu.E3);
        if (resultado != MMU_OK)
        {
            mmu_deslockear_paginas(socket_placa, jid);
            return resultado == MMU_PAGE_FAULT
                       ? EJECUCION_PAGE_FAULT
                       : EJECUCION_ERROR;
        }

        registros_cpu.E2 = (registros_cpu.E2 + registros_cpu.E1) / 2;
        registros_cpu.E3 = registros_cpu.E3 - registros_cpu.E2;

        // if (mmu_escribir_uint32(socket_placa, jid, dir_estado, registros_cpu.E2) == MMU_PAGE_FAULT)
        // {
        //     mmu_deslockear_paginas(socket_placa, jid);
        //     return EJECUCION_PAGE_FAULT;
        // }
        // if (mmu_escribir_uint32(socket_placa, jid, dir_pesos, registros_cpu.E3) == MMU_PAGE_FAULT)
        // {
        //     mmu_deslockear_paginas(socket_placa, jid);
        //     return EJECUCION_PAGE_FAULT;
        // }

        resultado = mmu_escribir_uint32(
            socket_placa,
            jid,
            dir_estado,
            registros_cpu.E2);
        if (resultado != MMU_OK)
        {
            mmu_deslockear_paginas(socket_placa, jid);
            return resultado == MMU_PAGE_FAULT
                       ? EJECUCION_PAGE_FAULT
                       : EJECUCION_ERROR;
        }
        resultado = mmu_escribir_uint32(
            socket_placa,
            jid,
            dir_pesos,
            registros_cpu.E3);
        if (resultado != MMU_OK)
        {
            mmu_deslockear_paginas(socket_placa, jid);
            return resultado == MMU_PAGE_FAULT
                       ? EJECUCION_PAGE_FAULT
                       : EJECUCION_ERROR;
        }
        mmu_deslockear_paginas(socket_placa, jid);

        return EJECUCION_CONTINUA;
    }

    if (strcmp(instruccion->nombre, "LOAD_BATCH") == 0)
    {
        return ejecutar_syscall_memoria(
            instruccion,
            jid,
            socket_placa,
            socket_dispatch,
            SYSCALL_LOAD_BATCH);
        if (strcmp(instruccion->nombre, "LABEL") == 0)
        {
            return ejecutar_syscall_memoria(
                instruccion,
                jid,
                socket_placa,
                socket_dispatch,
                SYSCALL_LABEL);
        }
        if (strcmp(instruccion->nombre, "REPORT") == 0)
        {
            return ejecutar_syscall_memoria(
                instruccion,
                jid,
                socket_placa,
                socket_dispatch,
                SYSCALL_REPORT);
        }
    }

    log_error(logger, "Instruccion desconocida o no implementada");

    mmu_deslockear_paginas(socket_placa, jid);
    return EJECUCION_CONTINUA;
}

//  // FORWARD
//         static t_resultado_ejecucion ejecutar_forward(
//             t_instruccion * instruccion,
//             uint32_t jid,
//             int socket_placa)
//         {

//         }

//  // BACKWARD
//         static t_resultado_ejecucion ejecutar_backward(
//             t_instruccion * instruccion,
//             uint32_t jid,
//             int socket_placa)
//         {

//         }

//  // UPDATE
//         static t_resultado_ejecucion ejecutar_backward(
//             t_instruccion * instruccion,
//             uint32_t jid,
//             int socket_placa)
//         {

//         }