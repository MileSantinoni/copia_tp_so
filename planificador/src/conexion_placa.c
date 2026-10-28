#include "conexion_placa.h"
#include "planificador.h"

#include <stdlib.h>
#include <string.h>

#include <utils/conexion.h>
#include <utils/comunicacion.h>
#include <utils/protocolo.h>
#include <utils/util.h>

int conectar_placa_dedicada(char *uso)
{
    char *ip_placa = config_get_string_value(config, "IP_PLACA");
    char *puerto_placa = config_get_string_value(config, "PUERTO_PLACA");

    int socket = crear_conexion(ip_placa, puerto_placa);
    if (socket == -1)
    {
        log_error(logger, "No se pudo abrir la conexion dedicada a Placa para %s", uso);
        exit(EXIT_FAILURE);
    }

    enviar_handshake(socket, PLANIFICADOR);
    log_debug(logger, "Conexion dedicada a Placa para %s - socket: %d", uso, socket);
    return socket;
}

// Recibe la respuesta y chequea el op_code. Siempre consume el paquete para no dejar basura en el socket.
static t_list *recibir_respuesta(int socket, op_code esperado, uint32_t jid, char *operacion)
{
    int cod_op = recibir_operacion(socket);
    if (cod_op == -1)
    {
        log_error(logger, "## (%u) - Placa se desconecto durante %s", jid, operacion);
        exit(EXIT_FAILURE);
    }

    t_list *valores = recibir_paquete(socket);

    if (cod_op != (int)esperado)
    {
        log_error(logger, "## (%u) - %s: se esperaba op_code %d y llego %d", jid, operacion, esperado, cod_op);
        list_destroy_and_destroy_elements(valores, free);
        return NULL;
    }
    return valores;
}

bool placa_cargar_pagina(int socket, uint32_t jid, uint32_t nro_pagina)
{
    t_paquete *paquete = crear_paquete(CARGAR_PAGINA);
    agregar_a_paquete(paquete, &jid, sizeof(uint32_t));
    agregar_a_paquete(paquete, &nro_pagina, sizeof(uint32_t));
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);

    t_list *valores = recibir_respuesta(socket, RESPUESTA_SOLICITUD, jid, "CARGAR_PAGINA");
    if (valores == NULL)
        return false;

    list_destroy_and_destroy_elements(valores, free);
    return true;
}

bool placa_deslockear_paginas(int socket, uint32_t jid)
{
    t_paquete *paquete = crear_paquete(DESLOCKEAR_PAGINAS);
    agregar_a_paquete(paquete, &jid, sizeof(uint32_t));
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);

    t_list *valores = recibir_respuesta(socket, RESPUESTA_SOLICITUD, jid, "DESLOCKEAR_PAGINAS");
    if (valores == NULL)
        return false;

    bool ok = list_size(valores) > 0 && strcmp((char *)list_get(valores, 0), "OK") == 0;
    list_destroy_and_destroy_elements(valores, free);

    if (ok)
        log_info(logger, "## (%u) - Deslockeo de páginas", jid);
    else
        log_error(logger, "## (%u) - Placa no confirmo el deslockeo de paginas", jid);
    return ok;
}

bool placa_leer_memoria_job(int socket, uint32_t jid, t_list *direcciones, void *buffer_destino)
{
    uint32_t desplazamiento = 0;

    for (int i = 0; i < list_size(direcciones); i++)
    {
        t_direccion_fisica *df = list_get(direcciones, i);
        log_info(logger, "## (%u) - Lectura - Dirección Física: %u - Tamaño: %u", jid, df->direccion_fisica, df->tamanio);

        t_paquete *paquete = crear_paquete(LEER_MEMORIA);
        agregar_a_paquete(paquete, &jid, sizeof(uint32_t));
        agregar_a_paquete(paquete, &df->direccion_fisica, sizeof(uint32_t));
        agregar_a_paquete(paquete, &df->tamanio, sizeof(uint32_t));
        enviar_paquete(paquete, socket);
        eliminar_paquete(paquete);

        t_list *valores = recibir_respuesta(socket, RESPUESTA_LECTURA, jid, "LEER_MEMORIA");
        if (valores == NULL || list_size(valores) < 1)
        {
            if (valores != NULL)
                list_destroy_and_destroy_elements(valores, free);
            return false;
        }

        // Voy pegando cada pedazo a continuacion del anterior
        memcpy((char *)buffer_destino + desplazamiento, list_get(valores, 0), df->tamanio);
        desplazamiento += df->tamanio;
        list_destroy_and_destroy_elements(valores, free);
    }
    return true;
}

bool placa_escribir_memoria_job(int socket, uint32_t jid, t_list *direcciones, void *buffer_origen)
{
    uint32_t desplazamiento = 0;

    for (int i = 0; i < list_size(direcciones); i++)
    {
        t_direccion_fisica *df = list_get(direcciones, i);
        log_info(logger, "## (%u) - Escritura - Dirección Física: %u - Tamaño: %u", jid, df->direccion_fisica, df->tamanio);

        t_paquete *paquete = crear_paquete(ESCRIBIR_MEMORIA);
        agregar_a_paquete(paquete, &jid, sizeof(uint32_t));
        agregar_a_paquete(paquete, &df->direccion_fisica, sizeof(uint32_t));
        agregar_a_paquete(paquete, &df->tamanio, sizeof(uint32_t));
        agregar_a_paquete(paquete, (char *)buffer_origen + desplazamiento, df->tamanio);
        enviar_paquete(paquete, socket);
        eliminar_paquete(paquete);

        t_list *valores = recibir_respuesta(socket, RESPUESTA_ESCRITURA, jid, "ESCRIBIR_MEMORIA");
        if (valores == NULL)
            return false;

        desplazamiento += df->tamanio;
        list_destroy_and_destroy_elements(valores, free);
    }
    return true;
}