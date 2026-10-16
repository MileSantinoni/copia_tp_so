#include "utils.h"

char *obtener_instruccion(uint32_t jid, uint32_t pc)
{
    pthread_mutex_lock(&mutex_jobs);
    char *clave = string_itoa(jid);
    t_job_memoria*job = dictionary_get(diccionario_jobs, clave);
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
    t_job_memoria *job = dictionary_remove(diccionario_jobs, clave);
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
    job->tabla_de_paginas=list_create();
    job->pagina_siguiente=0;

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
