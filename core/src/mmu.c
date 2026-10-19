#include "mmu.h"

#include <stdlib.h>
#include <string.h>

#include <commons/log.h>

#include <utils/comunicacion.h>
#include <utils/protocolo.h>

#include "core.h"

extern t_log *logger;
static uint32_t tamanio_pagina = 0;

// esta funcion quizá podria ser reemplazada por eliminar_paquete
// TODO revisar
static void destruir_lista_paquete(t_list *valores)
{
    if (valores != NULL)
    {
        list_destroy_and_destroy_elements(valores, free);
    }
}


static bool lista_tiene_elementos(t_list *valores, int cantidad_minima)
{
    return valores != NULL && list_size(valores) >= cantidad_minima;
}


uint32_t mmu_get_tamanio_pagina(void)
{
    return tamanio_pagina;
}

bool mmu_inicializar(int socket_placa)
{
    if (socket_placa < 0)
    {
        if (logger != NULL)
        {
            log_error(
                logger,
                "MMU: Socket de Placa inválido"
            );
        }

        return false;
    }

    t_paquete *paquete = crear_paquete(PEDIR_TAM_PAGINA);

    if (paquete == NULL)
    {
        if (logger != NULL)
        {
            log_error(
                logger,
                "MMU: No se pudo crear el paquete para solicitar el tamaño de página"
            );
        }

        return false;
    }

    enviar_paquete(paquete, socket_placa);
    eliminar_paquete(paquete);

    int codigo_operacion = recibir_operacion(socket_placa);

    if (codigo_operacion == -1)
    {
        if (logger != NULL)
        {
            log_error(
                logger,
                "MMU: Placa se desconectó al solicitar el tamaño de página"
            );
        }

        return false;
    }

    if (codigo_operacion != RESPUESTA_TAM_PAGINA)
    {
        if (logger != NULL)
        {
            log_error(
                logger,
                "MMU: Se esperaba RESPUESTA_TAM_PAGINA y se recibió op_code %d",
                codigo_operacion
            );
        }

        return false;
    }

    t_list *valores = recibir_paquete(socket_placa);

    if (!lista_tiene_elementos(valores, 1))
    {
        if (logger != NULL)
        {
            log_error(
                logger,
                "MMU: Respuesta inválida para el tamaño de página"
            );
        }

        destruir_lista_paquete(valores);
        return false;
    }

    uint32_t *tamanio_recibido = list_get(valores, 0);

    if (tamanio_recibido == NULL ||
        !mmu_set_tamanio_pagina(*tamanio_recibido))
    {
        destruir_lista_paquete(valores);
        return false;
    }

    destruir_lista_paquete(valores);

    if (logger != NULL)
    {
        log_info(
            logger,
            "MMU: Tamaño de página recibido de Placa: %u",
            tamanio_pagina
        );
    }

    return true;
}

bool mmu_separar_direccion(
    uint32_t direccion_logica,
    uint32_t *nro_pagina,
    uint32_t *desplazamiento
)
{
    if (tamanio_pagina == 0)
    {
        if (logger != NULL)
        {
            log_error(
                logger,
                "MMU: No se puede traducir porque el tamaño de página no fue inicializado"
            );
        }

        return false;
    }

    if (nro_pagina == NULL || desplazamiento == NULL)
    {
        if (logger != NULL)
        {
            log_error(
                logger,
                "MMU: Parámetros nulos al separar dirección lógica"
            );
        }

        return false;
    }

    *nro_pagina = direccion_logica / tamanio_pagina;
    *desplazamiento = direccion_logica % tamanio_pagina;

    return true;
}

t_resultado_mmu mmu_traducir_direccion(
    int socket_placa,
    uint32_t jid,
    uint32_t direccion_logica,
    t_traduccion_mmu *traduccion
)
{
    if (socket_placa < 0)
    {
        if (logger != NULL)
        {
            log_error(
                logger,
                "## JID: %u - MMU - Socket de Placa inválido",
                jid
            );
        }

        return MMU_ERROR;
    }

    if (traduccion == NULL)
    {
        if (logger != NULL)
        {
            log_error(
                logger,
                "## JID: %u - MMU - Resultado de traducción inválido",
                jid
            );
        }

        return MMU_ERROR;
    }

    if (tamanio_pagina == 0)
    {
        if (logger != NULL)
        {
            log_error(
                logger,
                "## JID: %u - MMU - Tamaño de página no inicializado",
                jid
            );
        }

        return MMU_ERROR;
    }

    uint32_t nro_pagina;
    uint32_t desplazamiento;

    if (!mmu_separar_direccion(
            direccion_logica,
            &nro_pagina,
            &desplazamiento))
    {
        return MMU_ERROR;
    }

    
    t_paquete *paquete = crear_paquete(OBTENER_MARCO);

    if (paquete == NULL)
    {
        if (logger != NULL)
        {
            log_error(
                logger,
                "## JID: %u - MMU - No se pudo crear paquete OBTENER_MARCO",
                jid
            );
        }

        return MMU_ERROR;
    }

    agregar_a_paquete(
        paquete,
        &jid,
        sizeof(uint32_t)
    );

    agregar_a_paquete(
        paquete,
        &nro_pagina,
        sizeof(uint32_t)
    );

    enviar_paquete(paquete, socket_placa);
    eliminar_paquete(paquete);

    int codigo_operacion = recibir_operacion(socket_placa);

    if (codigo_operacion == -1)
    {
        if (logger != NULL)
        {
            log_error(
                logger,
                "## JID: %u - MMU - Placa se desconectó al solicitar marco",
                jid
            );
        }

        return MMU_ERROR;
    }

    if (codigo_operacion == RESPUESTA_PAGE_FAULT)
    {
       //  la placa envía el número de página que produjo el Page Fault
         
        t_list *valores = recibir_paquete(socket_placa);

        if (!lista_tiene_elementos(valores, 1))
        {
            if (logger != NULL)
            {
                log_error(
                    logger,
                    "## JID: %u - MMU - Respuesta de Page Fault inválida",
                    jid
                );
            }

            destruir_lista_paquete(valores);
            return MMU_ERROR;
        }

        uint32_t *pagina_page_fault = list_get(valores, 0);

        if (pagina_page_fault == NULL)
        {
            destruir_lista_paquete(valores);
            return MMU_ERROR;
        }

        if (logger != NULL)
        {
            log_info(
                logger,
                "## JID: %u - Page Fault - Página: %u",
                jid,
                *pagina_page_fault
            );
        }

        destruir_lista_paquete(valores);

        return MMU_PAGE_FAULT;
    }

    if (codigo_operacion != RESPUESTA_MARCO)
    {
        if (logger != NULL)
        {
            log_error(
                logger,
                "## JID: %u - MMU - Se esperaba RESPUESTA_MARCO o "
                "RESPUESTA_PAGE_FAULT y se recibió op_code %d",
                jid,
                codigo_operacion
            );
        }

        return MMU_ERROR;
    }

    t_list *valores = recibir_paquete(socket_placa);

    if (!lista_tiene_elementos(valores, 1))
    {
        if (logger != NULL)
        {
            log_error(
                logger,
                "## JID: %u - MMU - Respuesta RESPUESTA_MARCO inválida",
                jid
            );
        }

        destruir_lista_paquete(valores);
        return MMU_ERROR;
    }

    uint32_t *marco_recibido = list_get(valores, 0);

    if (marco_recibido == NULL)
    {
        destruir_lista_paquete(valores);
        return MMU_ERROR;
    }

    uint32_t marco = *marco_recibido;
    uint32_t direccion_fisica =
        marco * tamanio_pagina + desplazamiento;

    traduccion->direccion_logica = direccion_logica;
    traduccion->nro_pagina = nro_pagina;
    traduccion->desplazamiento = desplazamiento;
    traduccion->marco = marco;
    traduccion->direccion_fisica = direccion_fisica;

    destruir_lista_paquete(valores);

    if (logger != NULL)
    {
        log_debug(
            logger,
            "## JID: %u - MMU - Lógica: %u - Página: %u - "
            "Desplazamiento: %u - Marco: %u - Física: %u",
            jid,
            direccion_logica,
            nro_pagina,
            desplazamiento,
            marco,
            direccion_fisica
        );
    }

    return MMU_OK;
}

bool mmu_deslockear_paginas(
    int socket_placa,
    uint32_t jid
)
{
    if (socket_placa < 0)
    {
        if (logger != NULL)
        {
            log_error(
                logger,
                "## JID: %u - MMU - Socket de Placa inválido al deslockear",
                jid
            );
        }

        return false;
    }

    t_paquete *paquete = crear_paquete(DESLOCKEAR_PAGINAS);

    if (paquete == NULL)
    {
        return false;
    }

    agregar_a_paquete(
        paquete,
        &jid,
        sizeof(uint32_t)
    );

    enviar_paquete(paquete, socket_placa);
    eliminar_paquete(paquete);

    int codigo_operacion = recibir_operacion(socket_placa);

    if (codigo_operacion == -1)
    {
        if (logger != NULL)
        {
            log_error(
                logger,
                "## JID: %u - MMU - Placa se desconectó al deslockear",
                jid
            );
        }

        return false;
    }

    if (codigo_operacion != RESPUESTA_SOLICITUD)
    {
        if (logger != NULL)
        {
            log_error(
                logger,
                "## JID: %u - MMU - Respuesta inválida al deslockear: %d",
                jid,
                codigo_operacion
            );
        }

        return false;
    }

    t_list *valores = recibir_paquete(socket_placa);

    if (!lista_tiene_elementos(valores, 1))
    {
        destruir_lista_paquete(valores);
        return false;
    }

    char *respuesta = list_get(valores, 0);
    bool correcto = respuesta != NULL &&
                    strcmp(respuesta, "OK") == 0;

    destruir_lista_paquete(valores);

    if (correcto && logger != NULL)
    {
        log_debug(
            logger,
            "## JID: %u - MMU - Páginas deslockeadas",
            jid
        );
    }

    return correcto;
}



bool mmu_set_tamanio_pagina(uint32_t nuevo_tamanio_pagina)
{
    if (nuevo_tamanio_pagina == 0)
    {
        if (logger != NULL)
        {
            log_error(
                logger,
                "MMU: El tamaño de página no puede ser cero"
            );
        }

        return false;
    }

    tamanio_pagina = nuevo_tamanio_pagina;

    if (logger != NULL)
    {
        log_debug(
            logger,
            "MMU: Tamaño de página configurado: %u",
            tamanio_pagina
        );
    }

    return true;
}