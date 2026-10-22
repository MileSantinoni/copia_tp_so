
#include "memoria_virtual.h"
#include "memoria.h"

#include "commons/bitarray.h"

// Offload
FILE *archivo_offload = NULL;
t_bitarray *bitmap_offload = NULL;
void *bits_offload = NULL;
int cantidad_bloques_offload = 0;

pthread_mutex_t mutex_offload = PTHREAD_MUTEX_INITIALIZER;

// Reemplazo de páginas
int puntero_clock = 0; // variable global para el puntero del clock

void inicializar_offload(void)
{
    archivo_offload = fopen(config_valores.path_offload, "w+b");
    if (archivo_offload == NULL)
    {
        log_error(logger, "No se pudo abrir/crear el archivo Offload en: %s", config_valores.path_offload);
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fileno(archivo_offload), config_valores.tam_offload) != 0)
    {
        log_error(logger, "Error al truncar el archivo de Offload");
        fclose(archivo_offload);
        exit(EXIT_FAILURE);
    }

    cantidad_bloques_offload = config_valores.tam_offload / config_valores.tam_pagina;
    size_t tam_bitmap_bytes = (cantidad_bloques_offload + 7) / 8;
    bits_offload = calloc(tam_bitmap_bytes, 1);
    bitmap_offload = bitarray_create_with_mode(bits_offload, tam_bitmap_bytes, LSB_FIRST);

    for (int i = 0; i < cantidad_bloques_offload; i++)
    {
        bitarray_clean_bit(bitmap_offload, i);
    }

    log_info(logger, "Offload inicializado: %d bytes (%d bloques de %d bytes) en %s",
             config_valores.tam_offload, cantidad_bloques_offload, config_valores.tam_pagina, config_valores.path_offload);
}

bool hay_espacio_sufiente_en_offload(int bloques_a_agregar)
{
    int contador = 0;

    for (int i = 0; i < cantidad_bloques_offload; i++)
    {
        if (!bitarray_test_bit(bitmap_offload, i))
        {
            contador++;
            if (contador == bloques_a_agregar)
            {

                return true;
            }
        }
    }

    return false;
}

int obtener_bloque_offload_libre(void)
{

    for (int i = 0; i < cantidad_bloques_offload; i++)
    {
        if (!bitarray_test_bit(bitmap_offload, i))
        {
            bitarray_set_bit(bitmap_offload, i);

            return i;
        }
    }

    return -1;
}

bool reservar_bloques_offload(int bloques_a_agregar, int bloques_reservados[])
{
    pthread_mutex_lock(&mutex_offload);
    if (hay_espacio_sufiente_en_offload(bloques_a_agregar))
    {
        for (int i = 0; i < bloques_a_agregar; i++)
        {
            bloques_reservados[i] = obtener_bloque_offload_libre();
        }
    }
    else
    {
        pthread_mutex_unlock(&mutex_offload);
        return false;
    }

    pthread_mutex_unlock(&mutex_offload);
    return true;
}
void liberar_bloque_offload(int nro_bloque)
{
    if (nro_bloque < 0 || nro_bloque >= cantidad_bloques_offload)
        return;

    pthread_mutex_lock(&mutex_offload);
    bitarray_clean_bit(bitmap_offload, nro_bloque);
    pthread_mutex_unlock(&mutex_offload);
}

void escribir_pagina_en_offload(uint32_t bloque_offload, void *buffer_origen)
{
    pthread_mutex_lock(&mutex_offload);
    usleep(config_valores.retardo_offload * 1000);

    fseek(archivo_offload, bloque_offload * config_valores.tam_pagina, SEEK_SET);
    fwrite(buffer_origen, config_valores.tam_pagina, 1, archivo_offload);
    fflush(archivo_offload);

    pthread_mutex_unlock(&mutex_offload);
}

void leer_pagina_de_offload(uint32_t bloque_offload, void *buffer_destino)
{
    pthread_mutex_lock(&mutex_offload);
    usleep(config_valores.retardo_offload * 1000);

    fseek(archivo_offload, bloque_offload * config_valores.tam_pagina, SEEK_SET);
    fread(buffer_destino, config_valores.tam_pagina, 1, archivo_offload);

    pthread_mutex_unlock(&mutex_offload);
}

// Busca la página asociada a un marco ocupado
static t_entrada_pagina *obtener_pagina_por_marco(int marco, t_job **out_job)
{
    if (tabla_marcos[marco].libre)
        return NULL;

    char *clave = string_itoa(tabla_marcos[marco].jid);
    t_job *job = dictionary_get(diccionario_jobs, clave);
    free(clave);

    if (job == NULL)
        return NULL;
    if (out_job != NULL)
        *out_job = job;

    return buscar_pagina(job, tabla_marcos[marco].nro_pagina);
}

// Algoritmo Clock Modificado (CLOCK-M)
t_entrada_pagina *seleccionar_victima_clock_m(void)
{
    int marcos_revisados;

    while (1)
    {
        // Paso 1: Buscar (U=0, M=0) sin modificar bits
        marcos_revisados = 0;
        while (marcos_revisados < cantidad_marcos)
        {
            t_job *job = NULL;
            t_entrada_pagina *entrada = obtener_pagina_por_marco(puntero_clock, &job);

            if (entrada != NULL && entrada->presencia && !entrada->lockeada)
            {
                if (!entrada->uso && !entrada->modificado)
                {
                    puntero_clock = (puntero_clock + 1) % cantidad_marcos;
                    return entrada;
                }
            }
            puntero_clock = (puntero_clock + 1) % cantidad_marcos;
            marcos_revisados++;
        }

        // Paso 2: Buscar (U=0, M=1) y poner U=0 en los que tengan U=1
        marcos_revisados = 0;
        while (marcos_revisados < cantidad_marcos)
        {
            t_job *job = NULL;
            t_entrada_pagina *entrada = obtener_pagina_por_marco(puntero_clock, &job);

            if (entrada != NULL && entrada->presencia && !entrada->lockeada)
            {
                if (!entrada->uso && entrada->modificado)
                {
                    puntero_clock = (puntero_clock + 1) % cantidad_marcos;
                    return entrada;
                }
                else if (entrada->uso)
                {
                    entrada->uso = false;
                }
            }
            puntero_clock = (puntero_clock + 1) % cantidad_marcos;
            marcos_revisados++;
        }
        // Si no encontró, el bucle repite (Paso 3 y Paso 4)
    }
}

int cargar_pagina_a_ram(uint32_t jid, t_entrada_pagina *pagina_a_cargar) // Carga una página que sufrió Page Fault a un marco físico
{
    int marco_asignado = obtener_marco_libre();

    // Si no hay marcos libres, se ejecuta el reemplazo con CLOCK-M
    if (marco_asignado == -1)
    {
        pthread_mutex_lock(&mutex_memoria);
        t_entrada_pagina *victima = seleccionar_victima_clock_m();
        pthread_mutex_unlock(&mutex_memoria);

        if (victima == NULL)
        {
            log_error(logger, "## JID: %d - Error: Todas las páginas están lockeadas, no hay víctima posible", jid);
            return -1;
        }

        marco_asignado = victima->marco;
        log_info(logger, "## Reemplazo: Sale víctima Job %d (Pág: %d, Marco: %d)",
                 tabla_marcos[marco_asignado].jid, victima->nro_pagina, marco_asignado);

        // Si la víctima estaba modificada, se sincroniza con el archivo de Offload
        if (victima->modificado)
        {
            void *origen = memoria_fisica + (marco_asignado * config_valores.tam_pagina);
            escribir_pagina_en_offload(victima->posicion_offload, origen);
            victima->modificado = false;
        }

        // La víctima deja de estar en RAM
        victima->presencia = false;
        victima->marco = -1;
    }

    // Traer la página desde el archivo de Offload a la RAM
    void *destino = memoria_fisica + (marco_asignado * config_valores.tam_pagina);
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
