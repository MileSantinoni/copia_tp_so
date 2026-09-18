#include "servidor.h"
#include "cliente.h"
#include "planificador.h"
#include "plani_corto_plazo.h"
#include "plani_largo_plazo.h"

#include <stdlib.h>
#include <pthread.h>

#include <commons/config.h>
#include <commons/log.h>
#include <commons/collections/list.h>

#include <utils/conexion.h>
#include <utils/comunicacion.h>
#include <utils/protocolo.h>

// VARIABLES DE CONFIGURACION

t_log *logger;
t_config *config;

// LISTA DE TODOS LOS JOBS

t_list *lista_jobs;
pthread_mutex_t mutex_lista_jobs;

// LISTA CORES

t_list *lista_cores;
pthread_mutex_t mutex_lista_core;
sem_t sem_cores;

// COLAS DE ESTADO

t_list *cola_new;
pthread_mutex_t mutex_cola_new;
sem_t sem_cola_new;

t_list *cola_ready;
pthread_mutex_t mutex_cola_ready;
sem_t sem_cola_ready;

t_list *cola_block; // DEBERIAN SER DISTINTAS COLAS DE BLOQUEO SEGUN EL MOTIVO
pthread_mutex_t mutex_cola_block;
sem_t sem_cola_block;

sem_t sem_cupo_multiprogramacion;
int grado_max_multiprogramacion;



int main(int argc, char *argv[])
{

    saludar("planificador");

    if (argc < 3)
    {
        printf("Uso correcto: ./bin/planificador [Archivo Config] [Path Job Inicial]\n");
        exit(EXIT_FAILURE);
    }

    config = config_create(argv[1]);

    if (config == NULL)
    {
        printf("No se pudo cargar el config en la ruta: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    t_log_level log_level = log_level_from_string(config_get_string_value(config, "LOG_LEVEL"));
    logger = log_create("planificador.log", "Planificador", 1, log_level);

    if (logger == NULL)
    {
        printf("Error en la creación el logger");
        exit(EXIT_FAILURE);
    }

    // Parte cliente
    iniciar_conexiones(config);
    iniciar_hilo_escucha();

    // Parte servidor

    pthread_t hilo_servidor;
    pthread_create(&hilo_servidor, NULL, iniciar_conexion_como_servidor, NULL);
    pthread_detach(hilo_servidor);

    // PARTE CENTRAL

    // Obtener el nombre del Job inicial. argv[2] = nombre del job
    char *path_job_inicial = argv[2];

    incializar_mutex();
    incializar_semaforos();
    incializar_listas();

    // HILOS DE PLANIFICADORES
    pthread_t hilo_corto_plazo;
    pthread_create(&hilo_corto_plazo, NULL, planificador_corto_plazo, NULL);
    pthread_detach(hilo_corto_plazo);

    pthread_t hilo_largo_plazo;
    pthread_create(&hilo_largo_plazo, NULL, planificador_largo_plazo, NULL);
    pthread_detach(hilo_largo_plazo);

    // Job inicial: se crea, se le avisa a la Placa, y se encola en NEW
    t_job *job_inicial = crear_job(0);
    enviar_crear_job_a_placa(0, path_job_inicial);
    encolar_new(job_inicial);

    // Liberar todo
    log_destroy(logger);
    config_destroy(config);

    return 0;
}

void incializar_mutex()
{

    pthread_mutex_init(&mutex_cola_ready, NULL);
    pthread_mutex_init(&mutex_lista_core, NULL);
    pthread_mutex_init(&mutex_cola_new, NULL);
    pthread_mutex_init(&mutex_lista_jobs, NULL);
}  

void incializar_semaforos()
{
    sem_init(&sem_cola_ready, 0, 0);
    sem_init(&sem_cores, 0, 0);
    sem_init(&sem_cola_new, 0, 0);
    grado_max_multiprogramacion = config_get_int_value(config, "GRADO_MULTIPROGRAMACION");
    sem_init(&sem_cupo_multiprogramacion, 0, grado_max_multiprogramacion); // inicializo el semáforo de cupo con el valor del grado de multiprogramación, no en 0 (es un semáforo contador, no uno de señalización)
}

void incializar_listas()
{
    cola_new = list_create();
    cola_ready = list_create();
    cola_block = list_create();

    lista_cores = list_create();

    lista_jobs = list_create();
}

