#ifndef PLACA_H_
#define PLACA_H_

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
    char* puerto_escucha;
    char* log_level;
    int tam_memoria;
    int tam_pagina;
    int retardo_memoria;
    char* algoritmo_reemplazo;
    char* path_instrucciones;
    char* path_offload;
    int tam_offload;
    int retardo_offload;
} t_config_placa;

//Estructura para entrada de pagina
typedef struct {
    uint32_t nro_pagina;
    int marco;                  // -1 si no esta en RAM (Page Fault)
    bool presencia;             // true si esta en RAM, false si está en swap/offload
    bool modificado;            // bit M
    bool uso;                   // bit U
    uint64_t timestamp_lru;     // instante de ultimo acceso
    bool lockeada;              // true si esta lock (no elegible como victima)
    uint32_t posicion_offload;  // bloque en el archivo offload
} t_entrada_pagina;

// Estructura para guardar las instrucciones de cada Job
typedef struct {
    uint32_t jid;
    char** instrucciones;
    int cantidad_instrucciones;
    t_list* tabla_paginas;      // Lista de t_entrada_pagina*
    uint32_t tamanio_alloc;     // Bytes dinámicos asignados
} t_job;

//Estructura para representar cada marco fisico de la RAM
typedef struct {
    int nro_marco;
    bool libre;
    uint32_t jid;
    uint32_t nro_pagina;
} t_marco;


// Diccionario/lista de Jobs cargafos.
extern t_dictionary* diccionario_jobs;
extern pthread_mutex_t mutex_jobs;


// Variables Globales
extern t_log* logger;
extern t_config* config;
extern t_config_placa config_valores;

// Prototipos de funciones
void inicializar_placa(char* path_config);
void esperar_conexiones(int socket_servidor);
void* atender_cliente(void* arg);
void finalizar_job_en_placa(uint32_t jid);
// void terminar_placa(void); --> la pase de forma general al util  porque nos sirve a todos los modulos

//Prototipos CHECK 2
char* crear_job_en_placa(uint32_t jid, char* nombre_archivo); // Devueelve mensaje de ERROR o de OK
char* obtener_instruccion(uint32_t jid, uint32_t pc);

//Prototipos para CHECK 3

// Tablas de Páginas
t_entrada_pagina* crear_entrada_pagina(uint32_t nro_pagina, uint32_t pos_offload);
t_entrada_pagina* buscar_pagina(t_job* job, uint32_t nro_pagina);
uint32_t asignar_paginas_job(t_job* job, uint32_t tamanio);
void liberar_espacio_job(t_job* job, uint32_t tamanio);
void liberar_entrada_pagina(void* elem);

// Memoria Física y Marcos
extern void* memoria_fisica;
extern t_marco* tabla_marcos;
extern int cantidad_marcos;
extern pthread_mutex_t mutex_memoria;

void inicializar_memoria_fisica(void);
int obtener_marco_libre(void);
void liberar_marco(int nro_marco);

// Offload / Swap
extern FILE* archivo_offload;
extern t_bitarray* bitmap_offload;
extern void* bits_offload;
extern int cantidad_bloques_offload;
extern pthread_mutex_t mutex_offload;

void inicializar_offload(void);
int obtener_bloque_offload_libre(void);
void liberar_bloque_offload(int nro_bloque);
void escribir_pagina_en_offload(uint32_t bloque_offload, void* buffer_origen);
void leer_pagina_de_offload(uint32_t bloque_offload, void* buffer_destino);

// Algoritmo de Reemplazo y Page Fault
extern int puntero_clock;

t_entrada_pagina* seleccionar_victima_clock_m(void);
int cargar_pagina_a_ram(uint32_t jid, t_entrada_pagina* pagina_a_cargar);
int traducir_y_obtener_marco(uint32_t jid, uint32_t nro_pagina);

#endif 