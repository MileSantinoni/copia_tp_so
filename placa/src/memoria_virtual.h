#ifndef MEMORIA_VIRTUAL_H_
#define MEMORIA_VIRTUAL_H_

#include "placa.h"
#include <commons/bitarray.h>

// Offload
extern FILE *archivo_offload;
extern t_bitarray *bitmap_offload;
extern void *bits_offload;
extern int cantidad_bloques_offload;
extern pthread_mutex_t mutex_offload;

void inicializar_offload(void);

int obtener_bloque_offload_libre(void);

void liberar_bloque_offload(int nro_bloque);

void escribir_pagina_en_offload(uint32_t bloque_offload, void *buffer_origen);

void leer_pagina_de_offload( uint32_t bloque_offload,void *buffer_destino);

// Reemplazo de páginas
extern int puntero_clock;

t_entrada_pagina *seleccionar_victima_clock_m(void);

// Paginación bajo demanda
int cargar_pagina_a_ram(uint32_t jid, t_entrada_pagina *pagina_a_cargar);

#endif