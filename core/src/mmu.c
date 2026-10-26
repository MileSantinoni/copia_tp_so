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


// traduccion de rangos 

static t_segmento_fisico *crear_segmento_fisico(
    uint32_t direccion_fisica,
    uint32_t tamanio
)
{
    t_segmento_fisico *segmento = malloc(sizeof(t_segmento_fisico));

    if (segmento == NULL)
    {
        return NULL;
    }

    segmento->direccion_fisica = direccion_fisica;
    segmento->tamanio = tamanio;

    return segmento;
}

void mmu_destruir_segmentos(t_list *segmentos_fisicos)
{
    if (segmentos_fisicos != NULL)
    {
        list_destroy_and_destroy_elements(
            segmentos_fisicos,
            free
        );
    }
}

t_resultado_mmu mmu_traducir_rango(
    int socket_placa,
    uint32_t jid,
    uint32_t direccion_logica,
    uint32_t tamanio,
    t_list **segmentos_fisicos
)
{
    if (segmentos_fisicos == NULL ||
        tamanio == 0 ||
        mmu_get_tamanio_pagina() == 0)
    {
        return MMU_ERROR;
    }

    *segmentos_fisicos = list_create();

    if (*segmentos_fisicos == NULL)
    {
        return MMU_ERROR;
    }

    uint32_t pagina = mmu_get_tamanio_pagina();
    uint32_t restante = tamanio;
    uint32_t direccion_actual = direccion_logica;

    while (restante > 0)
    {
        t_traduccion_mmu traduccion;

        t_resultado_mmu resultado =
            mmu_traducir_direccion(
                socket_placa,
                jid,
                direccion_actual,
                &traduccion
            );

        if (resultado != MMU_OK)
        {
            mmu_destruir_segmentos(*segmentos_fisicos);
            *segmentos_fisicos = NULL;
            return resultado;
        }

        uint32_t espacio_en_pagina =
            pagina - traduccion.desplazamiento;

        uint32_t tamanio_segmento =
            restante < espacio_en_pagina
                ? restante
                : espacio_en_pagina;

        t_segmento_fisico *segmento =
            crear_segmento_fisico(
                traduccion.direccion_fisica,
                tamanio_segmento
            );

        if (segmento == NULL)
        {
            mmu_destruir_segmentos(*segmentos_fisicos);
            *segmentos_fisicos = NULL;
            return MMU_ERROR;
        }

        list_add(*segmentos_fisicos, segmento);

        direccion_actual += tamanio_segmento;
        restante -= tamanio_segmento;
    }

    return MMU_OK;
}

// lectura y escritura de rangos 
static t_resultado_mmu leer_segmento_fisico(
    int socket_placa,
    uint32_t jid,
    t_segmento_fisico *segmento,
    void *destino
)
{
    if (segmento == NULL || destino == NULL)
    {
        return MMU_ERROR;
    }

    t_paquete *paquete =
        crear_paquete(LEER_MEMORIA);

    if (paquete == NULL)
    {
        return MMU_ERROR;
    }

    agregar_a_paquete(
        paquete,
        &jid,
        sizeof(uint32_t)
    );

    agregar_a_paquete(
        paquete,
        &segmento->direccion_fisica,
        sizeof(uint32_t)
    );

    agregar_a_paquete(
        paquete,
        &segmento->tamanio,
        sizeof(uint32_t)
    );

    enviar_paquete(paquete, socket_placa);
    eliminar_paquete(paquete);

    int codigo = recibir_operacion(socket_placa);

    if (codigo != RESPUESTA_LECTURA)
    {
        if (logger != NULL)
        {
            log_error(
                logger,
                "## JID: %u - Respuesta inválida de lectura: %d",
                jid,
                codigo
            );
        }

        return MMU_ERROR;
    }

    t_list *respuesta =
        recibir_paquete(socket_placa);

    if (respuesta == NULL ||
        list_size(respuesta) < 1)
    {
        if (respuesta != NULL)
        {
            list_destroy_and_destroy_elements(
                respuesta,
                free
            );
        }

        return MMU_ERROR;
    }

    void *contenido = list_get(respuesta, 0);

    if (contenido == NULL)
    {
        list_destroy_and_destroy_elements(
            respuesta,
            free
        );

        return MMU_ERROR;
    }

    memcpy(
        destino,
        contenido,
        segmento->tamanio
    );

    list_destroy_and_destroy_elements(
        respuesta,
        free
    );

    return MMU_OK;
}

static t_resultado_mmu escribir_segmento_fisico(
    int socket_placa,
    uint32_t jid,
    t_segmento_fisico *segmento,
    const void *origen
)
{
    if (segmento == NULL || origen == NULL)
    {
        return MMU_ERROR;
    }

    t_paquete *paquete =
        crear_paquete(ESCRIBIR_MEMORIA);

    if (paquete == NULL)
    {
        return MMU_ERROR;
    }

    agregar_a_paquete(
        paquete,
        &jid,
        sizeof(uint32_t)
    );

    agregar_a_paquete(
        paquete,
        &segmento->direccion_fisica,
        sizeof(uint32_t)
    );

    agregar_a_paquete(
        paquete,
        &segmento->tamanio,
        sizeof(uint32_t)
    );

    agregar_a_paquete(
        paquete,
        (void *)origen,
        segmento->tamanio
    );

    enviar_paquete(paquete, socket_placa);
    eliminar_paquete(paquete);

    int codigo = recibir_operacion(socket_placa);

    if (codigo != RESPUESTA_ESCRITURA)
    {
        if (logger != NULL)
        {
            log_error(
                logger,
                "## JID: %u - Respuesta inválida de escritura: %d",
                jid,
                codigo
            );
        }

        return MMU_ERROR;
    }

    return MMU_OK;
}

t_resultado_mmu mmu_leer_rango(
    int socket_placa,
    uint32_t jid,
    uint32_t direccion_logica,
    uint32_t tamanio,
    void *buffer_destino
)
{
    if (buffer_destino == NULL || tamanio == 0)
    {
        return MMU_ERROR;
    }

    t_list *segmentos = NULL;

    t_resultado_mmu resultado =
        mmu_traducir_rango(
            socket_placa,
            jid,
            direccion_logica,
            tamanio,
            &segmentos
        );

    if (resultado != MMU_OK)
    {
        return resultado;
    }

    uint32_t desplazamiento_buffer = 0;

    for (int i = 0; i < list_size(segmentos); i++)
    {
        t_segmento_fisico *segmento =
            list_get(segmentos, i);

        resultado =
            leer_segmento_fisico(
                socket_placa,
                jid,
                segmento,
                (char *)buffer_destino +
                    desplazamiento_buffer
            );

        if (resultado != MMU_OK)
        {
            mmu_destruir_segmentos(segmentos);
            return resultado;
        }

        desplazamiento_buffer += segmento->tamanio;
    }

    mmu_destruir_segmentos(segmentos);

    return MMU_OK;
}

t_resultado_mmu mmu_escribir_rango(
    int socket_placa,
    uint32_t jid,
    uint32_t direccion_logica,
    uint32_t tamanio,
    const void *buffer_origen
)
{
    if (buffer_origen == NULL || tamanio == 0)
    {
        return MMU_ERROR;
    }

    t_list *segmentos = NULL;

    t_resultado_mmu resultado =
        mmu_traducir_rango(
            socket_placa,
            jid,
            direccion_logica,
            tamanio,
            &segmentos
        );

    if (resultado != MMU_OK)
    {
        return resultado;
    }

    uint32_t desplazamiento_buffer = 0;

    for (int i = 0; i < list_size(segmentos); i++)
    {
        t_segmento_fisico *segmento =
            list_get(segmentos, i);

        resultado =
            escribir_segmento_fisico(
                socket_placa,
                jid,
                segmento,
                (const char *)buffer_origen +
                    desplazamiento_buffer
            );

        if (resultado != MMU_OK)
        {
            mmu_destruir_segmentos(segmentos);
            return resultado;
        }

        desplazamiento_buffer += segmento->tamanio;
    }

    mmu_destruir_segmentos(segmentos);

    return MMU_OK;
}

t_resultado_mmu mmu_leer_uint32(
    int socket_placa,
    uint32_t jid,
    uint32_t direccion_logica,
    uint32_t *valor
)
{
    if (valor == NULL)
    {
        return MMU_ERROR;
    }

    return mmu_leer_rango(
        socket_placa,
        jid,
        direccion_logica,
        sizeof(uint32_t),
        valor
    );
}

t_resultado_mmu mmu_escribir_uint32(
    int socket_placa,
    uint32_t jid,
    uint32_t direccion_logica,
    uint32_t valor
)
{
    return mmu_escribir_rango(
        socket_placa,
        jid,
        direccion_logica,
        sizeof(uint32_t),
        &valor
    );
}