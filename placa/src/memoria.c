
#include "placa.h"
#include "memoria_virtual.h"

void *memoria_fisica = NULL;
t_marco *tabla_marcos = NULL;
int cantidad_marcos = 0;
pthread_mutex_t mutex_memoria = PTHREAD_MUTEX_INITIALIZER;

// Administración de marcos

int obtener_marco_libre(void)
{
    pthread_mutex_lock(&mutex_memoria);
    for (int i = 0; i < cantidad_marcos; i++)
    {
        if (tabla_marcos[i].libre)
        {
            tabla_marcos[i].libre = false;
            pthread_mutex_unlock(&mutex_memoria);
            return i;
        }
    }
    pthread_mutex_unlock(&mutex_memoria);
    return -1; // -1 indica que no hay lugar libre (requiere reemplazo)
}

void liberar_marco(int nro_marco)
{
    if (nro_marco < 0 || nro_marco >= cantidad_marcos)
        return;

    pthread_mutex_lock(&mutex_memoria);
    tabla_marcos[nro_marco].libre = true;
    tabla_marcos[nro_marco].jid = 0;
    tabla_marcos[nro_marco].nro_pagina = 0;
    pthread_mutex_unlock(&mutex_memoria);
}

void inicializar_memoria_fisica(void)
{
    memoria_fisica = malloc(config_valores.tam_memoria);
    memset(memoria_fisica, 0, config_valores.tam_memoria);

    cantidad_marcos = config_valores.tam_memoria / config_valores.tam_pagina;
    tabla_marcos = malloc(sizeof(t_marco) * cantidad_marcos);

    for (int i = 0; i < cantidad_marcos; i++)
    {
        tabla_marcos[i].nro_marco = i;
        tabla_marcos[i].libre = true;
        tabla_marcos[i].jid = 0;
        tabla_marcos[i].nro_pagina = 0;
    }

    log_info(logger, "Memoria física lista: %d bytes (%d marcos de %d bytes)",
             config_valores.tam_memoria, cantidad_marcos, config_valores.tam_pagina);
}

// Tablas de páginas

t_entrada_pagina *crear_entrada_pagina(uint32_t nro_pagina, uint32_t pos_offload)
{
    t_entrada_pagina *entrada = malloc(sizeof(t_entrada_pagina));
    entrada->nro_pagina = nro_pagina;
    entrada->marco = -1;        // Arranca sin marco asignado (bajo demanda)
    entrada->presencia = false; // No está en memoria física aún
    entrada->modificado = false;
    entrada->uso = false;
    entrada->timestamp_lru = 0;
    entrada->lockeada = false;
    entrada->posicion_offload = pos_offload;
    return entrada;
}

t_entrada_pagina *buscar_pagina(t_job *job, uint32_t nro_pagina)
{
    if (job == NULL || job->tabla_de_paginas == NULL)
    {
        return NULL;
    }

    for (int i = 0; i < list_size(job->tabla_de_paginas); i++)
    {
        t_entrada_pagina *entrada = list_get(job->tabla_de_paginas, i);
        if (entrada->nro_pagina == nro_pagina)
        {
            return entrada;
        }
    }

    return NULL;
}

void liberar_entrada_pagina(void *elem)
{
    t_entrada_pagina *entrada = (t_entrada_pagina *)elem;

    if (entrada->presencia && entrada->marco != -1)
    {
        liberar_marco(entrada->marco);
    }

    liberar_bloque_offload(entrada->posicion_offload);

    free(entrada);
}

// ALLOC y FREE

uint32_t asignar_espacio_job(t_job *job, uint32_t tamanio)
{
    // uint32_t cant_paginas_actuales = list_size(job->tabla_de_paginas);
    uint32_t direccion_logica_base = job->pagina_siguiente * config_valores.tam_pagina;

    // Cantidad de páginas necesarias redondeadas hacia arriba: ceil(tamanio / tam_pagina)
    u_int32_t paginas_a_agregar = (tamanio + config_valores.tam_pagina - 1) / config_valores.tam_pagina;

    int bloques_reservado[paginas_a_agregar];
    if (reservar_bloques_offload(paginas_a_agregar, bloques_reservado)) // se reservan todos los bloques juntos
    {
        for (u_int32_t i = 0; i < paginas_a_agregar; i++)
        {
            uint32_t nro_pagina = job->pagina_siguiente;
            job->pagina_siguiente++;
            t_entrada_pagina *nueva_pagina = crear_entrada_pagina(nro_pagina, bloques_reservado[i]);
            list_add(job->tabla_de_paginas, nueva_pagina);
        }
    }
    else
    {

        log_error(logger, "No hay espacio suficiente en OFFLOAD para el tamaño solicitado por el JID: %u", job->jid);
        return UINT32_MAX; // Como nno me puede devolver -1 uso eso
    }

    // job->tamanio_alloc += tamanio;

    t_asignacion_espacio *asignacion_nueva = malloc(sizeof(t_asignacion_espacio));
    asignacion_nueva->cantidad_paginas = paginas_a_agregar;
    asignacion_nueva->direccion_logica = direccion_logica_base;
    asignacion_nueva->tamanio = tamanio;
    list_add(job->asignaciones_de_espacio, asignacion_nueva);

    log_info(logger, "## JID: %d - Asignación - Dirección Lógica: %u - Tamaño: %u", job->jid, direccion_logica_base, tamanio);

    return direccion_logica_base;
}

t_asignacion_espacio *buscar_asignacion_por_dl(t_list *asignaciones_de_espacio, u_int32_t dir_logica)
{

    for (int i = 0; i < list_size(asignaciones_de_espacio); i++)
    {

        t_asignacion_espacio *espacio = (t_asignacion_espacio *)list_get(asignaciones_de_espacio, i);
        if (espacio->direccion_logica == dir_logica)
        {
            return espacio;
        }
    }

    return NULL;
}

void liberar_espacio_job(t_job *job, uint32_t dir_logica)
{
    // uint32_t paginas_a_liberar = (tamanio + config_valores.tam_pagina - 1) / config_valores.tam_pagina;

    t_asignacion_espacio *espacio_a_eliminar = buscar_asignacion_por_dl(job->asignaciones_de_espacio, dir_logica);

    if (espacio_a_eliminar == NULL)
    {
        log_error(logger, "JID: %u - No existe una asignación en la dirección lógica %u", job->jid, dir_logica);
        return;
    }

    for (int i = 0; i < espacio_a_eliminar->cantidad_paginas; i++)
    {
        u_int32_t nro_pagina = (dir_logica / config_valores.tam_pagina) + i;

        if (!list_is_empty(job->tabla_de_paginas))
        {
            t_entrada_pagina *entrada = buscar_pagina(job, nro_pagina);

            if (entrada != NULL)
            {
                // Eliminamos la página de la tabla de paginas
                list_remove_element(job->tabla_de_paginas, entrada);
                // Liberamos el marco de memoria si estaba ocupado
                liberar_marco(entrada->marco);
                // Liberamos el espacio de offload
                liberar_bloque_offload(entrada->posicion_offload);
                // Lo elimanamos de la memoria real
                free(entrada);
            }
        }
    }

    // uint32_t direccion_logica_base = list_size(job->tabla_de_paginas) * config_valores.tam_pagina;
    log_info(logger, "## JID: %d - Liberación - Dirección Lógica: %u - Tamaño: %u ", job->jid, dir_logica, espacio_a_eliminar->tamanio);

    list_remove_element(job->asignaciones_de_espacio, espacio_a_eliminar);
    free(espacio_a_eliminar);
}

// Acceso a memoria física

void leer_de_memoria_fisica(uint32_t dir_fisica, uint32_t tamanio, void *buffer_destino)
{
    usleep(config_valores.retardo_memoria * 1000);
    pthread_mutex_lock(&mutex_memoria);
    memcpy(buffer_destino, memoria_fisica + dir_fisica, tamanio);
    pthread_mutex_unlock(&mutex_memoria);
}

void escribir_en_memoria_fisica(uint32_t jid, uint32_t dir_fisica, uint32_t tamanio, void *datos)
{
    usleep(config_valores.retardo_memoria * 1000);
    pthread_mutex_lock(&mutex_memoria);
    memcpy(memoria_fisica + dir_fisica, datos, tamanio);

    int nro_marco = dir_fisica / config_valores.tam_pagina;
    if (nro_marco < cantidad_marcos && !tabla_marcos[nro_marco].libre)
    {
        char *clave = string_itoa(jid);
        t_job *job = dictionary_get(diccionario_jobs, clave);
        free(clave);
        if (job != NULL)
        {
            t_entrada_pagina *entrada = buscar_pagina(job, tabla_marcos[nro_marco].nro_pagina);
            if (entrada != NULL)
            {
                entrada->modificado = true;
                entrada->uso = true;
            }
        }
    }
    pthread_mutex_unlock(&mutex_memoria);
}

// Consulta de páginas

int traducir_y_obtener_marco(uint32_t jid, uint32_t nro_pagina) // Traducción MMU
{
    pthread_mutex_lock(&mutex_jobs);
    char *clave = string_itoa(jid);
    t_job *job = dictionary_get(diccionario_jobs, clave);
    free(clave);

    if (job == NULL)
    {
        pthread_mutex_unlock(&mutex_jobs);
        log_error(logger, "## JID: %d - Job no encontrado al solicitar traducción", jid);
        return -1;
    }

    t_entrada_pagina *entrada = buscar_pagina(job, nro_pagina);
    pthread_mutex_unlock(&mutex_jobs);

    if (entrada == NULL)
    {
        log_error(logger, "## JID: %d - Página %u fuera de rango (Segmentation Fault)", jid, nro_pagina);
        return -1;
    }

    usleep(config_valores.retardo_memoria * 1000);

    // Si está en RAM
    if (entrada->presencia)
    {
        entrada->uso = true;
        entrada->lockeada = true;
        return entrada->marco;
    }

    // Si no está en RAM (Page Fault)
    log_info(logger, "## (%u) - Page Fault en página: %u", jid, nro_pagina);
    int marco = cargar_pagina_a_ram(jid, entrada);
    if (marco != -1)
    {
        entrada->lockeada = true;
    }

    return marco;
}

void deslockear_paginas_job(uint32_t jid)
{
    pthread_mutex_lock(&mutex_jobs);
    char *clave = string_itoa(jid);
    t_job *job = dictionary_get(diccionario_jobs, clave);
    free(clave);

    if (job != NULL)
    {
        for (int i = 0; i < list_size(job->tabla_de_paginas); i++)
        {
            t_entrada_pagina *entrada = list_get(job->tabla_de_paginas, i);
            entrada->lockeada = false;
        }
    }
    pthread_mutex_unlock(&mutex_jobs);
}