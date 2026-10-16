#include "placa.h"

#include <utils/conexion.h>
#include <utils/comunicacion.h>
#include <utils/protocolo.h>
#include <utils/util.h>

// Definición de las variables globales declaradas como extern en el .h
t_log *logger;
t_config *config;
t_config_placa config_valores;

t_dictionary *diccionario_jobs;
pthread_mutex_t mutex_jobs = PTHREAD_MUTEX_INITIALIZER;

int socket_core;
int socket_planificador;

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Uso correcto: ./bin/placa [Archivo Config] \n");
        exit(EXIT_FAILURE);
    }

    char *path_config = (argc > 1) ? argv[1] : "placa.config";

    inicializar_placa(path_config);

    // int socket_servidor = inicia_servidor_placa(config_valores.puerto_escucha); --> puse iniciar servviddor para no repetir codigo al pedo
    int socket_servidor = iniciar_servidor(config_valores.puerto_escucha);

    if (socket_servidor < 0)
    {
        log_error(logger, "Error al iniciar servidor");
        config_destroy(config);
        log_destroy(logger);
        exit(EXIT_FAILURE);
    }

    log_info(logger, "Placa lista para recibir conexiones - Puerto: %s", config_valores.puerto_escucha);

    esperar_conexiones(socket_servidor);

    terminar_modulo();

    return 0;
}

void inicializar_placa(char *path_config)
{
    diccionario_jobs = dictionary_create();

    config = config_create(path_config);

    if (config == NULL)
    {
        printf("No se pudo abrir el archivo de configuración: %s\n", path_config);
        exit(EXIT_FAILURE);
    }

    // Cargamos los valores del config a nuestra estructura
    config_valores.puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");
    config_valores.log_level = config_get_string_value(config, "LOG_LEVEL");
    config_valores.tam_memoria = config_get_int_value(config, "TAM_MEMORIA");
    config_valores.tam_pagina = config_get_int_value(config, "TAM_PAGINA");
    config_valores.retardo_memoria = config_get_int_value(config, "RETARDO_MEMORIA");
    config_valores.algoritmo_reemplazo = config_get_string_value(config, "ALGORITMO_REEMPLAZO");
    config_valores.path_instrucciones = config_get_string_value(config, "PATH_INSTRUCCIONES");
    config_valores.path_offload = config_get_string_value(config, "PATH_OFFLOAD");
    config_valores.tam_offload = config_get_int_value(config, "TAM_OFFLOAD");
    config_valores.retardo_offload = config_get_int_value(config, "RETARDO_OFFLOAD");

    logger = log_create("placa.log", "PLACA", true, log_level_from_string(config_valores.log_level));
    if (logger == NULL)
    {
        printf("No se pudo crear el logger\n");
        exit(EXIT_FAILURE);
    }
}

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
        int codigo_operacion = recibir_operacion(socket_cliente); // Reccibimos el codigo del paquete aca, dentro del switch hay que recibir el paquete

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
            agregar_a_paquete(paquete, mensaje,  strlen(mensaje) + 1);    
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

        default:
            log_warning(logger, "## Código de operación desconocido: %d", codigo_operacion);
            break;
        }
    }

    close(socket_cliente);
    return NULL;
}

t_entrada_pagina* crear_entrada_pagina(uint32_t nro_pagina, uint32_t pos_offload) {
    t_entrada_pagina* entrada = malloc(sizeof(t_entrada_pagina));
    entrada->nro_pagina = nro_pagina;
    entrada->marco = -1;              // Arranca sin marco asignado (bajo demanda)
    entrada->presencia = false;       // No está en memoria física aún
    entrada->modificado = false;
    entrada->uso = false;
    entrada->timestamp_lru = 0;
    entrada->lockeada = false;
    entrada->posicion_offload = pos_offload;
    return entrada;
}

t_entrada_pagina* buscar_pagina(t_job* job, uint32_t nro_pagina) {
    if (job == NULL || job->tabla_paginas == NULL) {
        return NULL;
    }

    for (int i = 0; i < list_size(job->tabla_paginas); i++) {
        t_entrada_pagina* entrada = list_get(job->tabla_paginas, i);
        if (entrada->nro_pagina == nro_pagina) {
            return entrada;
        }
    }

    return NULL;
}

void liberar_entrada_pagina(void* elem) {
    t_entrada_pagina* entrada = (t_entrada_pagina*) elem;
    free(entrada);
}

uint32_t asignar_paginas_job(t_job* job, uint32_t tamanio) {
    uint32_t cant_paginas_actuales = list_size(job->tabla_paginas);
    uint32_t direccion_logica_base = cant_paginas_actuales * config_valores.tam_pagina;

    // Cantidad de páginas necesarias redondeadas hacia arriba
    uint32_t paginas_a_agregar = (tamanio + config_valores.tam_pagina - 1) / config_valores.tam_pagina;

    for (uint32_t i = 0; i < paginas_a_agregar; i++) {
        uint32_t nro_pagina = cant_paginas_actuales + i;
        uint32_t bloque_offload = nro_pagina; // placeholder hasta conectar bitmap de swap
        
        t_entrada_pagina* nueva_pagina = crear_entrada_pagina(nro_pagina, bloque_offload);
        list_add(job->tabla_paginas, nueva_pagina);
    }

    job->tamanio_alloc += tamanio;

    log_info(logger, "## JID: %d - Asignación - Dirección Lógica: %u - Tamaño: %u",
             job->jid, direccion_logica_base, tamanio);

    return direccion_logica_base;
}

void liberar_espacio_job(t_job* job, uint32_t tamanio) {
    uint32_t paginas_a_liberar = (tamanio + config_valores.tam_pagina - 1) / config_valores.tam_pagina;

    for (uint32_t i = 0; i < paginas_a_liberar; i++) {
        if (!list_is_empty(job->tabla_paginas)) {
            t_entrada_pagina* entrada = list_remove(job->tabla_paginas, list_size(job->tabla_paginas) - 1);
            liberar_entrada_pagina(entrada);
        }
    }

    if (job->tamanio_alloc >= tamanio) {
        job->tamanio_alloc -= tamanio;
    } else {
        job->tamanio_alloc = 0;
    }

    uint32_t direccion_logica_base = list_size(job->tabla_paginas) * config_valores.tam_pagina;
    log_info(logger, "## JID: %d - Liberación - Dirección Lógica: %u - Tamaño: %u",
             job->jid, direccion_logica_base, tamanio);
}

char *obtener_instruccion(uint32_t jid, uint32_t pc)
{
    pthread_mutex_lock(&mutex_jobs);
    char *clave = string_itoa(jid);
    t_job *job = dictionary_get(diccionario_jobs, clave);
    free(clave);

    char *instruccion_copia = NULL;
    if (job != NULL && pc < job->cantidad_instrucciones)
        instruccion_copia = strdup(job->instrucciones[pc]);

    pthread_mutex_unlock(&mutex_jobs);

    if (instruccion_copia == NULL)
        log_error(logger, "## JID: %d - PC inválido o Job inexistente: %u", jid, pc);
    else
        log_info(logger, "## JID: %d - Obtener instrucción: %u - Instrucción: %s", jid, pc, instruccion_copia);

    return instruccion_copia; // es responsabilidad de quien la llama hacer free
}

void finalizar_job_en_placa(uint32_t jid)
{
    pthread_mutex_lock(&mutex_jobs);

    char *clave = string_itoa(jid);
    t_job *job = dictionary_remove(diccionario_jobs, clave);
    free(clave);

    pthread_mutex_unlock(&mutex_jobs);

    if (job == NULL)
    {
        log_warning(logger, "## JID: %d - Finalización solicitada pero el Job no existe en la Placa", jid);
        return;
    }

    for (int i = 0; i < job->cantidad_instrucciones; i++)
        free(job->instrucciones[i]);

    free(job->instrucciones);
    free(job);

    log_info(logger, "## JID: %d - Job finalizado, estructuras liberadas en la Placa", jid);
}

char *crear_job_en_placa(uint32_t jid, char *nombre_archivo)
{
    char *path_completo = string_from_format("%s%s", config_valores.path_instrucciones, nombre_archivo);

    log_info(logger, "Intentando abrir archivo: %s", path_completo);

    FILE *archivo = fopen(path_completo, "r");
    free(path_completo);

    if (archivo == NULL)
    {
        log_error(logger, "## JID: %d - Error al abrir archivo de instrucciones: %s", jid, nombre_archivo);
        return "ERROR EN CREAR JOB";
    }

    t_job *job = malloc(sizeof(t_job));
    job->jid = jid;
    job->instrucciones = NULL;
    job->cantidad_instrucciones = 0;

    char *linea = NULL;
    size_t len = 0;

    while (getline(&linea, &len, archivo) != -1)
    {
        /* Saco el salto de linea si hay */
        linea[strcspn(linea, "\n")] = '\0';

        job->instrucciones = realloc(job->instrucciones, sizeof(char *) * (job->cantidad_instrucciones + 1));
        job->instrucciones[job->cantidad_instrucciones] = strdup(linea);
        job->cantidad_instrucciones++;
    }

    free(linea);
    fclose(archivo);

    pthread_mutex_lock(&mutex_jobs);
    char *clave = string_itoa(jid);
    dictionary_put(diccionario_jobs, clave, job);
    free(clave);
    pthread_mutex_unlock(&mutex_jobs);

    log_info(logger, "## JID: %d - Job Creado", jid);
    return "CREAR JOB OK";
}