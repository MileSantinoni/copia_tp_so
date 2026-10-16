#ifndef PLACA_UTILS_H_
#define PLACA_UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/collections/list.h>
#include <commons/bitarray.h>

#include <commons/collections/dictionary.h>

#include <utils/protocolo.h>

// Estructura para guardar la configuracion
typedef struct
{
    char *puerto_escucha;
    char *log_level;
    int tam_memoria;
    int tam_pagina;
    int retardo_memoria;
    char *algoritmo_reemplazo;
    char *path_instrucciones;
    char *path_offload;
    int tam_offload;
    int retardo_offload;
} t_config_placa;

// Estructura para entrada de pagina
typedef struct
{
    uint32_t nro_pagina;
    int marco;                 // -1 si no esta en RAM (Page Fault)
    bool presencia;            // true si esta en RAM, false si está en swap/offload
    bool modificado;           // bit M
    bool uso;                  // bit U
    uint64_t timestamp_lru;    // instante de ultimo acceso
    bool lockeada;             // true si esta lock (no elegible como victima)
    uint32_t posicion_offload; // bloque en el archivo offload
} t_entrada_pagina;

// Estructura para guardar las instrucciones de cada Job
typedef struct
{
    uint32_t jid;
    char **instrucciones; // arrray de strings
    int cantidad_instrucciones;
    t_list *tabla_de_paginas;
    u_int32_t pagina_siguiente;
    uint32_t tamanio_alloc;
} t_job;

// Estructura para representar cada marco fisico de la RAM

typedef struct
{
    int nro_marco;
    bool libre;
    uint32_t jid;
    uint32_t nro_pagina;
} t_marco;
typedef struct
{
    u_int32_t nro_pagina;
    u_int32_t marco;
    bool presente;

} t_tabla_de_paginas;

typedef struct
{
    void *memoria_total;

} t_memoria;

// Diccionario/lista de Jobs cargados.
extern t_dictionary *diccionario_jobs;
extern pthread_mutex_t mutex_jobs;

// Variables Globales
extern t_log *logger;
extern t_config *config;
extern t_config_placa config_valores;

// Prototipos de funciones
void inicializar_placa(char *path_config);
void esperar_conexiones(int socket_servidor);
void *atender_cliente(void *arg);
void finalizar_job_en_placa(uint32_t jid);
// void terminar_placa(void); --> la pase de forma general al util  porque nos sirve a todos los modulos

// Prototipos CHECK 2
char *crear_job_en_placa(uint32_t jid, char *nombre_archivo); // Devueelve mensaje de ERROR o de OK
char *obtener_instruccion(uint32_t jid, uint32_t pc);

// Prototipos para CHECK 3
t_entrada_pagina *crear_entrada_pagina(uint32_t nro_pagina, uint32_t pos_offload);
t_entrada_pagina *buscar_pagina(t_job *job, uint32_t nro_pagina);
uint32_t asignar_paginas_job(t_job *job, uint32_t tamanio);
void liberar_espacio_job(t_job *job, uint32_t tamanio);
void liberar_entrada_pagina(void *elem);

extern void *memoria_fisica;
extern t_marco *tabla_marcos;
extern int cantidad_marcos;
extern pthread_mutex_t mutex_memoria;

void inicializar_memoria_fisica(void);
int obtener_marco_libre(void);
void liberar_marco(int nro_marco);

#endif
