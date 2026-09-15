#include "servidor.h"
#include "cliente.h"
#include "planificador.h"
#include "plani_corto_plazo.h"

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

// LISTAS
//// COLAS DE ESTADOS

t_list *cola_new;
t_list *cola_ready;
t_list *cola_block; // DEBERIAN SER DISTINTAS COLAS DE BLOQUEO SEGUN EL MOTIVO

//// CORES

t_list *lista_cores;

// MUTEX
//// DE COLAS DE ESTADO

pthread_mutex_t mutex_cola_ready;
pthread_mutex_t mutex_cola_block; // DEBERIAN SER DISTINTAS COLAS DE BLOQUEO SEGUN EL MOTIVO

// SEMAFOROS
//// DE COLAS DE ESTADO

sem_t sem_cola_ready;
sem_t sem_cola_block; // DEBERIAN SER DISTINTAS COLAS DE BLOQUEO SEGUN EL MOTIVO

//// PARA VER SI HAY CORES DISPONIBLES

sem_t sem_cores;


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

    iniciar_conexion_como_servidor();

    // PARTE CENTRAL

    // Obtener el nombre del Job inicial. argv[2] = nombre del job
    char *path_job_inicial = argv[2];

    // INICIALIZAR MUTEX

    pthread_mutex_init(&mutex_cola_ready, NULL);

    // INICIALIZAR SEMAFOROSS

    sem_init(&sem_cola_ready, 0, 0);
    sem_init(&sem_cores, 0, 0);

    // HILOS DE PLANIFICADORES
    pthread_t hilo_corto_plazo;   // Creo el hilo para que siempre este esperando algun job para ejecutar pero sin frenar todo el resto
    pthread_create(&hilo_corto_plazo, NULL, planificador_corto_plazo, NULL);
    pthread_detach(hilo_corto_plazo);

    // ESTO LO PUSE DE GUIA
    // Crearlo e inicializarlo --> Pasa a new
    // Le pasamos las instrucciones a placa. Esperrramos su respuesta de confirmacion
    // Plani largo plazo --> se fija el grado de multiprogramacion y se fija si loadmite
    // Si lo puede admitir --> Pasa a ready (actualizo lista y semaforo)
    // Sino deberia esperar a que baje el grado de multiprogramacion. Podriamos usar un semaforo que cuando finaliza un proograma se ponde en 1 (no es tan neceesario ahora pero es bueno tenerlo en cuenta)


  

    // Liberar todo
    log_destroy(logger);
    config_destroy(config);

    return 0;
}



t_core *obtener_core_libre()
{

    for (int i = 0; i < list_size(lista_cores); i++)
    {

        t_core *core_actual = (t_core *)list_get(lista_cores, i);
        if (core_actual->libre)
        {

            return core_actual;
        }
    }

    return NULL;
}

