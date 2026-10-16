#include "utils.h"

#include <utils/conexion.h>
#include <utils/comunicacion.h>
#include <utils/protocolo.h>
#include <utils/util.h>

// Definición de las variables globales declaradas como extern en el .h
t_log *logger;
t_config *config;
t_config_placa config_valores;

//-------CHECK 3---------
void* memoria_fisica = NULL;
t_marco* tabla_marcos = NULL;
int cantidad_marcos = 0;
pthread_mutex_t mutex_memoria = PTHREAD_MUTEX_INITIALIZER;
int puntero_clock = 0; //variable global para el puntero del clock

FILE* archivo_offload = NULL;
t_bitarray* bitmap_offload = NULL;
void* bits_offload = NULL;
int cantidad_bloques_offload = 0;
pthread_mutex_t mutex_offload = PTHREAD_MUTEX_INITIALIZER;

//--------------------------

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

// CONEXIONES

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

    //Inicializacion de la memoria fisica y tabla de marcos
    inicializar_memoria_fisica();
    inicializar_offload();
}

void inicializar_memoria_fisica(void) {
    memoria_fisica = malloc(config_valores.tam_memoria);
    memset(memoria_fisica, 0, config_valores.tam_memoria);

    cantidad_marcos = config_valores.tam_memoria / config_valores.tam_pagina;
    tabla_marcos = malloc(sizeof(t_marco) * cantidad_marcos);

    for (int i = 0; i < cantidad_marcos; i++) {
        tabla_marcos[i].nro_marco = i;
        tabla_marcos[i].libre = true;
        tabla_marcos[i].jid = 0;
        tabla_marcos[i].nro_pagina = 0;
    }

    log_info(logger, "Memoria física lista: %d bytes (%d marcos de %d bytes)",
             config_valores.tam_memoria, cantidad_marcos, config_valores.tam_pagina);
}

int obtener_marco_libre(void) {
    pthread_mutex_lock(&mutex_memoria);
    for (int i = 0; i < cantidad_marcos; i++) {
        if (tabla_marcos[i].libre) {
            tabla_marcos[i].libre = false;
            pthread_mutex_unlock(&mutex_memoria);
            return i;
        }
    }
    pthread_mutex_unlock(&mutex_memoria);
    return -1; // -1 indica que no hay lugar libre (requiere reemplazo)
}

void liberar_marco(int nro_marco) {
    if (nro_marco < 0 || nro_marco >= cantidad_marcos) return;

    pthread_mutex_lock(&mutex_memoria);
    tabla_marcos[nro_marco].libre = true;
    tabla_marcos[nro_marco].jid = 0;
    tabla_marcos[nro_marco].nro_pagina = 0;
    pthread_mutex_unlock(&mutex_memoria);
}

void inicializar_offload(void) {
    archivo_offload = fopen(config_valores.path_offload, "w+b");
    if (archivo_offload == NULL) {
        log_error(logger, "No se pudo abrir/crear el archivo Offload en: %s", config_valores.path_offload);
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fileno(archivo_offload), config_valores.tam_offload) != 0) {
        log_error(logger, "Error al truncar el archivo de Offload");
        fclose(archivo_offload);
        exit(EXIT_FAILURE);
    }

    cantidad_bloques_offload = config_valores.tam_offload / config_valores.tam_pagina;
    size_t tam_bitmap_bytes = (cantidad_bloques_offload + 7) / 8;
    bits_offload = calloc(tam_bitmap_bytes, 1);
    bitmap_offload = bitarray_create_with_mode(bits_offload, tam_bitmap_bytes, LSB_FIRST);

    for (int i = 0; i < cantidad_bloques_offload; i++) {
        bitarray_clean_bit(bitmap_offload, i);
    }

    log_info(logger, "Offload inicializado: %d bytes (%d bloques de %d bytes) en %s",
             config_valores.tam_offload, cantidad_bloques_offload, config_valores.tam_pagina, config_valores.path_offload);
}

int obtener_bloque_offload_libre(void) {
    pthread_mutex_lock(&mutex_offload);
    for (int i = 0; i < cantidad_bloques_offload; i++) {
        if (!bitarray_test_bit(bitmap_offload, i)) {
            bitarray_set_bit(bitmap_offload, i);
            pthread_mutex_unlock(&mutex_offload);
            return i;
        }
    }
    pthread_mutex_unlock(&mutex_offload);
    return -1;
}

void liberar_bloque_offload(int nro_bloque) {
    if (nro_bloque < 0 || nro_bloque >= cantidad_bloques_offload) return;

    pthread_mutex_lock(&mutex_offload);
    bitarray_clean_bit(bitmap_offload, nro_bloque);
    pthread_mutex_unlock(&mutex_offload);
}

void escribir_pagina_en_offload(uint32_t bloque_offload, void* buffer_origen) {
    pthread_mutex_lock(&mutex_offload);
    usleep(config_valores.retardo_offload * 1000);

    fseek(archivo_offload, bloque_offload * config_valores.tam_pagina, SEEK_SET);
    fwrite(buffer_origen, config_valores.tam_pagina, 1, archivo_offload);
    fflush(archivo_offload);

    pthread_mutex_unlock(&mutex_offload);
}

void leer_pagina_de_offload(uint32_t bloque_offload, void* buffer_destino) {
    pthread_mutex_lock(&mutex_offload);
    usleep(config_valores.retardo_offload * 1000);

    fseek(archivo_offload, bloque_offload * config_valores.tam_pagina, SEEK_SET);
    fread(buffer_destino, config_valores.tam_pagina, 1, archivo_offload);

    pthread_mutex_unlock(&mutex_offload);
}

void deslockear_paginas_job(uint32_t jid) {
    pthread_mutex_lock(&mutex_jobs);
    char *clave = string_itoa(jid);
    t_job *job = dictionary_get(diccionario_jobs, clave);
    free(clave);

    if (job != NULL) {
        for (int i = 0; i < list_size(job->tabla_paginas); i++) {
            t_entrada_pagina *entrada = list_get(job->tabla_paginas, i);
            entrada->lockeada = false;
        }
    }
    pthread_mutex_unlock(&mutex_jobs);
}

void leer_de_memoria_fisica(uint32_t dir_fisica, uint32_t tamanio, void *buffer_destino) {
    usleep(config_valores.retardo_memoria * 1000);
    pthread_mutex_lock(&mutex_memoria);
    memcpy(buffer_destino, memoria_fisica + dir_fisica, tamanio);
    pthread_mutex_unlock(&mutex_memoria);
}

void escribir_en_memoria_fisica(uint32_t jid, uint32_t dir_fisica, uint32_t tamanio, void *datos) {
    usleep(config_valores.retardo_memoria * 1000);
    pthread_mutex_lock(&mutex_memoria);
    memcpy(memoria_fisica + dir_fisica, datos, tamanio);

    int nro_marco = dir_fisica / config_valores.tam_pagina;
    if (nro_marco < cantidad_marcos && !tabla_marcos[nro_marco].libre) {
        char *clave = string_itoa(jid);
        t_job *job = dictionary_get(diccionario_jobs, clave);
        free(clave);
        if (job != NULL) {
            t_entrada_pagina *entrada = buscar_pagina(job, tabla_marcos[nro_marco].nro_pagina);
            if (entrada != NULL) {
                entrada->modificado = true;
                entrada->uso = true;
            }
        }
    }
    pthread_mutex_unlock(&mutex_memoria);
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

            int cantidad_paginas = tamaño / config_valores.tam_pagina;
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
            agregar_a_paquete(paquete,&direccion_logica, sizeof(u_int32_t));
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
    if (job == NULL || job->tabla_de_paginas == NULL) {
        return NULL;
    }

    for (int i = 0; i < list_size(job->tabla_de_paginas); i++) {
        t_entrada_pagina* entrada = list_get(job->tabla_de_paginas, i);
        if (entrada->nro_pagina == nro_pagina) {
            return entrada;
        }
    }

    return NULL;
}

//==============================
//Seguir aca con el algoritmo de clock y con page fault
//==============================

// Busca la página asociada a un marco ocupado
static t_entrada_pagina* obtener_pagina_por_marco(int marco, t_job** out_job) {
    if (tabla_marcos[marco].libre) return NULL;

    char* clave = string_itoa(tabla_marcos[marco].jid);
    t_job* job = dictionary_get(diccionario_jobs, clave);
    free(clave);

    if (job == NULL) return NULL;
    if (out_job != NULL) *out_job = job;

    return buscar_pagina(job, tabla_marcos[marco].nro_pagina);
}

// Algoritmo Clock Modificado (CLOCK-M)
t_entrada_pagina* seleccionar_victima_clock_m(void) {
    int marcos_revisados;

    while (1) {
        // Paso 1: Buscar (U=0, M=0) sin modificar bits
        marcos_revisados = 0;
        while (marcos_revisados < cantidad_marcos) {
            t_job* job = NULL;
            t_entrada_pagina* entrada = obtener_pagina_por_marco(puntero_clock, &job);

            if (entrada != NULL && entrada->presencia && !entrada->lockeada) {
                if (!entrada->uso && !entrada->modificado) {
                    puntero_clock = (puntero_clock + 1) % cantidad_marcos;
                    return entrada;
                }
            }
            puntero_clock = (puntero_clock + 1) % cantidad_marcos;
            marcos_revisados++;
        }

        // Paso 2: Buscar (U=0, M=1) y poner U=0 en los que tengan U=1
        marcos_revisados = 0;
        while (marcos_revisados < cantidad_marcos) {
            t_job* job = NULL;
            t_entrada_pagina* entrada = obtener_pagina_por_marco(puntero_clock, &job);

            if (entrada != NULL && entrada->presencia && !entrada->lockeada) {
                if (!entrada->uso && entrada->modificado) {
                    puntero_clock = (puntero_clock + 1) % cantidad_marcos;
                    return entrada;
                } else if (entrada->uso) {
                    entrada->uso = false;
                }
            }
            puntero_clock = (puntero_clock + 1) % cantidad_marcos;
            marcos_revisados++;
        }
        // Si no encontró, el bucle repite (Paso 3 y Paso 4)
    }
}

// Carga una página que sufrió Page Fault a un marco físico
int cargar_pagina_a_ram(uint32_t jid, t_entrada_pagina* pagina_a_cargar) {
    int marco_asignado = obtener_marco_libre();

    // Si no hay marcos libres, se ejecuta el reemplazo con CLOCK-M
    if (marco_asignado == -1) {
        pthread_mutex_lock(&mutex_memoria);
        t_entrada_pagina* victima = seleccionar_victima_clock_m();
        pthread_mutex_unlock(&mutex_memoria);

        if (victima == NULL) {
            log_error(logger, "## JID: %d - Error: Todas las páginas están lockeadas, no hay víctima posible", jid);
            return -1;
        }

        marco_asignado = victima->marco;
        log_info(logger, "## Reemplazo: Sale víctima Job %d (Pág: %d, Marco: %d)",
                 tabla_marcos[marco_asignado].jid, victima->nro_pagina, marco_asignado);

        // Si la víctima estaba modificada, se sincroniza con el archivo de Offload
        if (victima->modificado) {
            void* origen = memoria_fisica + (marco_asignado * config_valores.tam_pagina);
            escribir_pagina_en_offload(victima->posicion_offload, origen);
            victima->modificado = false;
        }

        // La víctima deja de estar en RAM
        victima->presencia = false;
        victima->marco = -1;
    }

    // Traer la página desde el archivo de Offload a la RAM
    void* destino = memoria_fisica + (marco_asignado * config_valores.tam_pagina);
    leer_pagina_de_offload(pagina_a_cargar->posicion_offload, destino);

    // Actualizar tabla de marcos
    pthread_mutex_lock(&mutex_memoria);
    tabla_marcos[marco_asignado].libre = false;
    tabla_marcos[marco_asignado].jid = jid;
    tabla_marcos[marco_asignado].nro_pagina = pagina_a_cargar->nro_pagina;
    pthread_mutex_unlock(&mutex_memoria);

    // Actualizar la entrada de página cargada
    pagina_a_cargar->marco = marco_asignado;
    pagina_a_cargar->presencia = true;
    pagina_a_cargar->uso = true;
    pagina_a_cargar->modificado = false;

    return marco_asignado;
}

// Traducción MMU
int traducir_y_obtener_marco(uint32_t jid, uint32_t nro_pagina) {
    pthread_mutex_lock(&mutex_jobs);
    char* clave = string_itoa(jid);
    t_job* job = dictionary_get(diccionario_jobs, clave);
    free(clave);

    if (job == NULL) {
        pthread_mutex_unlock(&mutex_jobs);
        log_error(logger, "## JID: %d - Job no encontrado al solicitar traducción", jid);
        return -1;
    }

    t_entrada_pagina* entrada = buscar_pagina(job, nro_pagina);
    pthread_mutex_unlock(&mutex_jobs);

    if (entrada == NULL) {
        log_error(logger, "## JID: %d - Página %u fuera de rango (Segmentation Fault)", jid, nro_pagina);
        return -1;
    }

    usleep(config_valores.retardo_memoria * 1000);

    // Si está en RAM
    if (entrada->presencia) {
        entrada->uso = true;
        entrada->lockeada = true;
        return entrada->marco;
    }

    // Si no está en RAM (Page Fault)
    log_info(logger, "## (%u) - Page Fault en página: %u", jid, nro_pagina);
    int marco = cargar_pagina_a_ram(jid, entrada);
    if (marco != -1) {
        entrada->lockeada = true;
    }

    return marco;
}

void liberar_entrada_pagina(void* elem) {
    t_entrada_pagina* entrada = (t_entrada_pagina*) elem;
    if (entrada->presencia && entrada->marco != -1) {
        liberar_marco(entrada->marco);
    }
    liberar_bloque_offload(entrada->posicion_offload);
    free(entrada);
}

uint32_t asignar_paginas_job(t_job* job, uint32_t tamanio) {
    uint32_t cant_paginas_actuales = list_size(job->tabla_de_paginas);
    uint32_t direccion_logica_base = cant_paginas_actuales * config_valores.tam_pagina;

    // Cantidad de páginas necesarias redondeadas hacia arriba: ceil(tamanio / tam_pagina)
    uint32_t paginas_a_agregar = (tamanio + config_valores.tam_pagina - 1) / config_valores.tam_pagina;

    for (uint32_t i = 0; i < paginas_a_agregar; i++) {
        uint32_t nro_pagina = cant_paginas_actuales + i;
        uint32_t bloque_offload = nro_pagina; // placeholder temporal hasta conectar bitmap de swap
        
        t_entrada_pagina* nueva_pagina = crear_entrada_pagina(nro_pagina, bloque_offload);
        list_add(job->tabla_de_paginas, nueva_pagina);
    }

    job->tamanio_alloc += tamanio;

    log_info(logger, "## JID: %d - Asignación - Dirección Lógica: %u - Tamaño: %u",
             job->jid, direccion_logica_base, tamanio);

    return direccion_logica_base;
}

void liberar_espacio_job(t_job* job, uint32_t tamanio) {
    uint32_t paginas_a_liberar = (tamanio + config_valores.tam_pagina - 1) / config_valores.tam_pagina;

    for (uint32_t i = 0; i < paginas_a_liberar; i++) {
        if (!list_is_empty(job->tabla_de_paginas)) {
            t_entrada_pagina* entrada = list_remove(job->tabla_de_paginas, list_size(job->tabla_de_paginas) - 1);
            liberar_entrada_pagina(entrada);
        }
    }

    if (job->tamanio_alloc >= tamanio) {
        job->tamanio_alloc -= tamanio;
    } else {
        job->tamanio_alloc = 0;
    }

    uint32_t direccion_logica_base = list_size(job->tabla_de_paginas) * config_valores.tam_pagina;
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

    job->tabla_paginas = list_create();
    job->tamanio_alloc = 0;

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