
#include "servidor.h"
#include "jobs.h"
#include "memoria.h"
#include "memoria_virtual.h"

#include <utils/conexion.h>
#include <utils/comunicacion.h>
#include <utils/protocolo.h>
#include <utils/util.h>
// Conexiones y atencion a clientes

int socket_core;
int socket_planificador;

void esperar_conexiones(int socket_servidor)
{
    while (1)
    {
        int socket_cliente = accept(socket_servidor, NULL, NULL);

        if (socket_cliente == -1)
            continue;

        int *socket_ptr = malloc(sizeof(int));
        *socket_ptr = socket_cliente;

        pthread_t hilo_cliente;
        pthread_create(&hilo_cliente, NULL, atender_cliente, socket_ptr);
        pthread_detach(hilo_cliente);
    }
}

void *atender_cliente(void *arg)
{
    int socket_cliente = *(int *)arg;
    free(arg);

    t_modulo modulo = recibir_handshake(socket_cliente);
    char *modulo_s = modulo_a_string(modulo);

    switch (modulo)
    {
    case CORE:
    {
        socket_core = socket_cliente;
        int id_core = recibir_core_id(socket_cliente);
        log_info(logger, "ID CORE: %i", id_core);
        break;
    }

    case PLANIFICADOR:
    {
        socket_planificador = socket_cliente;
        break;
    }

    default:
        log_warning(logger, "## Módulo desconocido: %s", modulo_s);
        close(socket_cliente);
        return NULL;
    }

    log_info(logger, "Esperando peticiones de %s", modulo_s);

    while (1)
    {
        int codigo_operacion = recibir_operacion(socket_cliente); // Recibimos el codigo del paquete aca, dentro del switch hay que recibir el paquete

        if (codigo_operacion == -1)
        {
            log_warning(logger, "Se desconecto %s", modulo_s);
            return NULL;
        }
        switch (codigo_operacion)
        {
        case CREAR_JOB:
        {
            t_list *lista = recibir_paquete(socket_cliente);

            uint32_t jid = *(uint32_t *)list_get(lista, 0);
            char *nombre_archivo = (char *)list_get(lista, 1);
            uint32_t id_solicitud = *(uint32_t *)list_get(lista, 2);
            char *mensaje = crear_job_en_placa(jid, nombre_archivo);

            // LE ENVIO LA RESPUESTA DE CREAR JOB
            // enviar_mensaje(mensaje, socket_cliente);
            t_paquete *paquete = crear_paquete(RESPUESTA_SOLICITUD);
            agregar_a_paquete(paquete, mensaje, strlen(mensaje) + 1);
            agregar_a_paquete(paquete, &id_solicitud, sizeof(uint32_t));
            enviar_paquete(paquete, socket_cliente);
            eliminar_paquete(paquete);

            list_destroy_and_destroy_elements(lista, free);
            break;
        }

        case FETCH_INSTRUCCION:
        {
            t_list *lista = recibir_paquete(socket_cliente);

            uint32_t jid = *(uint32_t *)list_get(lista, 0);
            uint32_t pc = *(uint32_t *)list_get(lista, 1);

            char *instruccion = obtener_instruccion(jid, pc);
            enviar_mensaje(instruccion != NULL ? instruccion : "ERROR", socket_cliente);
            if (instruccion != NULL)
                free(instruccion);

            list_destroy_and_destroy_elements(lista, free);
            break;
        }

        case FINALIZAR_JOB:
        {
            t_list *lista = recibir_paquete(socket_cliente);

            uint32_t jid = *(uint32_t *)list_get(lista, 0);

            finalizar_job_en_placa(jid);

            enviar_mensaje("JOB_FINALIZADO", socket_cliente);

            list_destroy_and_destroy_elements(lista, free);
            break;
        }

        case MENSAJE:
        {
            // El texto está después del entero de tamaño (payload + 4 bytes)
            /*char* texto = (char*)(paquete->payload + sizeof(uint32_t));
            log_info(logger, "## Mensaje recibido: %s", texto);*/

            int size;
            char *buffer = recibir_buffer(&size, socket_cliente);
            log_info(logger, "Me llego el mensaje: %s", buffer);
            free(buffer);

            // Confirmación simple de vuelta al cliente
            enviar_mensaje("CONFIRMACION_RECIBIDA", socket_cliente);
            break;
        }
        case ALLOC: // Se carga la memoria pedida en disco
        {
            t_list *lista = recibir_paquete(socket_cliente);
            uint32_t jid = *(uint32_t *)list_get(lista, 0);
            uint32_t tamaño = *(uint32_t *)list_get(lista, 0);

           /* int cantidad_paginas = tamaño / config_valores.tam_pagina;
            if (tamaño % config_valores.tam_pagina != 0)
            {
                cantidad_paginas++;
            }

            //  FALTA --> RESERVAR ESPACIO EN OFFLOAF, si hay espacio hace lo que sigue sino no 

            // Agrego a la tabla de paginas del job el nuevo registro
            pthread_mutex_lock(&mutex_jobs);
            t_job*job = dictionary_get(diccionario_jobs, string_itoa(jid));

            t_tabla_de_paginas *registro_nuevo;
            
            u_int32_t pagina_inicial = job->pagina_siguiente;

            for (int i = 0; i < cantidad_paginas; i++)
            {
                registro_nuevo = malloc(sizeof(t_tabla_de_paginas));
                registro_nuevo->marco = -1;
                registro_nuevo->nro_pagina = job->pagina_siguiente;
                job->pagina_siguiente++;
                registro_nuevo->presente = 0;
                list_add(job->tabla_de_paginas, registro_nuevo);
            }

            pthread_mutex_unlock(&mutex_jobs);
            t_paquete *paquete = crear_paquete(RESPUESTA_ALLOC);
            agregar_a_paquete(paquete, &jid,sizeof(uint32_t));
            u_int32_t direccion_logica = pagina_inicial *  config_valores.tam_pagina;
            agregar_a_paquete(paquete,&direccion_logica, sizeof(u_int32_t));*/
        }

        case PEDIR_TAM_PAGINA:
        {
            uint32_t tam_pag = (uint32_t)config_valores.tam_pagina;
            t_paquete *paquete = crear_paquete(RESPUESTA_TAM_PAGINA);
            agregar_a_paquete(paquete, &tam_pag, sizeof(uint32_t));
            enviar_paquete(paquete, socket_cliente);
            eliminar_paquete(paquete);
            break;
        }

        case OBTENER_MARCO:
        {
            t_list *lista = recibir_paquete(socket_cliente);
            uint32_t jid = *(uint32_t *)list_get(lista, 0);
            uint32_t nro_pagina = *(uint32_t *)list_get(lista, 1);

            pthread_mutex_lock(&mutex_jobs);
            char *clave = string_itoa(jid);
            t_job *job = dictionary_get(diccionario_jobs, clave);
            free(clave);

            t_entrada_pagina *entrada = (job != NULL) ? buscar_pagina(job, nro_pagina) : NULL;
            pthread_mutex_unlock(&mutex_jobs);

            usleep(config_valores.retardo_memoria * 1000);

            if (entrada == NULL || !entrada->presencia) {
                log_info(logger, "## (%u) - Page Fault en página: %u", jid, nro_pagina);
                t_paquete *paquete = crear_paquete(RESPUESTA_PAGE_FAULT);
                agregar_a_paquete(paquete, &nro_pagina, sizeof(uint32_t));
                enviar_paquete(paquete, socket_cliente);
                eliminar_paquete(paquete);
            } else {
                entrada->uso = true;
                entrada->lockeada = true;
                uint32_t marco = (uint32_t)entrada->marco;

                t_paquete *paquete = crear_paquete(RESPUESTA_MARCO);
                agregar_a_paquete(paquete, &marco, sizeof(uint32_t));
                enviar_paquete(paquete, socket_cliente);
                eliminar_paquete(paquete);
            }

            list_destroy_and_destroy_elements(lista, free);
            break;
        }

        case CARGAR_PAGINA:
        {
            t_list *lista = recibir_paquete(socket_cliente);
            uint32_t jid = *(uint32_t *)list_get(lista, 0);
            uint32_t nro_pagina = *(uint32_t *)list_get(lista, 1);

            pthread_mutex_lock(&mutex_jobs);
            char *clave = string_itoa(jid);
            t_job *job = dictionary_get(diccionario_jobs, clave);
            free(clave);

            t_entrada_pagina *entrada = (job != NULL) ? buscar_pagina(job, nro_pagina) : NULL;
            pthread_mutex_unlock(&mutex_jobs);

            int marco = -1;
            if (entrada != NULL) {
                marco = cargar_pagina_a_ram(jid, entrada);
            }

            uint32_t respuesta_marco = (marco >= 0) ? (uint32_t)marco : 0;
            t_paquete *paquete = crear_paquete(RESPUESTA_SOLICITUD);
            agregar_a_paquete(paquete, &respuesta_marco, sizeof(uint32_t));
            enviar_paquete(paquete, socket_cliente);
            eliminar_paquete(paquete);

            list_destroy_and_destroy_elements(lista, free);
            break;
        }

        case LEER_MEMORIA:
        {
            t_list *lista = recibir_paquete(socket_cliente);
            uint32_t jid = *(uint32_t *)list_get(lista, 0);
            uint32_t dir_fisica = *(uint32_t *)list_get(lista, 1);
            uint32_t tamanio = *(uint32_t *)list_get(lista, 2);

            void *buffer_lectura = malloc(tamanio);
            leer_de_memoria_fisica(dir_fisica, tamanio, buffer_lectura);

            log_info(logger, "## JID: %u - Lectura Física - Dir: %u - Tamaño: %u", jid, dir_fisica, tamanio);

            t_paquete *paquete = crear_paquete(RESPUESTA_LECTURA);
            agregar_a_paquete(paquete, buffer_lectura, tamanio);
            enviar_paquete(paquete, socket_cliente);
            eliminar_paquete(paquete);

            free(buffer_lectura);
            list_destroy_and_destroy_elements(lista, free);
            break;
        }

        case ESCRIBIR_MEMORIA:
        {
            t_list *lista = recibir_paquete(socket_cliente);
            uint32_t jid = *(uint32_t *)list_get(lista, 0);
            uint32_t dir_fisica = *(uint32_t *)list_get(lista, 1);
            uint32_t tamanio = *(uint32_t *)list_get(lista, 2);
            void *contenido = list_get(lista, 3);

            escribir_en_memoria_fisica(jid, dir_fisica, tamanio, contenido);

            log_info(logger, "## JID: %u - Escritura Física - Dir: %u - Tamaño: %u", jid, dir_fisica, tamanio);

            t_paquete *paquete = crear_paquete(RESPUESTA_ESCRITURA);
            enviar_paquete(paquete, socket_cliente);
            eliminar_paquete(paquete);

            list_destroy_and_destroy_elements(lista, free);
            break;
        }

        case DESLOCKEAR_PAGINAS:
        {
            t_list *lista = recibir_paquete(socket_cliente);
            uint32_t jid = *(uint32_t *)list_get(lista, 0);

            deslockear_paginas_job(jid);

            t_paquete *paquete = crear_paquete(RESPUESTA_SOLICITUD);
            char *msg = "OK";
            agregar_a_paquete(paquete, msg, strlen(msg) + 1);
            enviar_paquete(paquete, socket_cliente);
            eliminar_paquete(paquete);

            list_destroy_and_destroy_elements(lista, free);
            break;
        }

        case ASIGNAR_ESPACIO:
        {
            t_list *lista = recibir_paquete(socket_cliente);
            uint32_t jid = *(uint32_t *)list_get(lista, 0);
            uint32_t tamanio = *(uint32_t *)list_get(lista, 1);

            pthread_mutex_lock(&mutex_jobs);
            char *clave = string_itoa(jid);
            t_job *job = dictionary_get(diccionario_jobs, clave);
            free(clave);

            uint32_t dir_logica = 0;
            if (job != NULL) {
                dir_logica = asignar_paginas_job(job, tamanio);
            }
            pthread_mutex_unlock(&mutex_jobs);

            t_paquete *paquete = crear_paquete(RESPUESTA_SOLICITUD);
            agregar_a_paquete(paquete, &dir_logica, sizeof(uint32_t));
            enviar_paquete(paquete, socket_cliente);
            eliminar_paquete(paquete);

            list_destroy_and_destroy_elements(lista, free);
            break;
        }

        case LIBERAR_ESPACIO:
        {
            t_list *lista = recibir_paquete(socket_cliente);
            uint32_t jid = *(uint32_t *)list_get(lista, 0);
            uint32_t dir_logica = *(uint32_t *)list_get(lista, 1);

            pthread_mutex_lock(&mutex_jobs);
            char *clave = string_itoa(jid);
            t_job *job = dictionary_get(diccionario_jobs, clave);
            free(clave);

            if (job != NULL) {
                uint32_t tamanio_a_liberar = job->tamanio_alloc - dir_logica;
                liberar_espacio_job(job, tamanio_a_liberar);
            }
            pthread_mutex_unlock(&mutex_jobs);

            t_paquete *paquete = crear_paquete(RESPUESTA_SOLICITUD);
            char *ok_msg = "LIBERADO_OK";
            agregar_a_paquete(paquete, ok_msg, strlen(ok_msg) + 1);
            enviar_paquete(paquete, socket_cliente);
            eliminar_paquete(paquete);

            list_destroy_and_destroy_elements(lista, free);
            break;
        }

        default:
            log_warning(logger, "## Código de operación desconocido: %d", codigo_operacion);
            break;
        }
    }

    close(socket_cliente);
    return NULL;

}