#ifndef MEMORIA_H_
#define MEMORIA_H_

#include "placa.h"

// Memoria física
extern void *memoria_fisica;
extern t_marco *tabla_marcos;
extern int cantidad_marcos;
extern pthread_mutex_t mutex_memoria;

void inicializar_memoria_fisica(void);

// Administración de marcos
int obtener_marco_libre(void);
void liberar_marco(int nro_marco);

// Tablas de páginas
t_entrada_pagina *crear_entrada_pagina(uint32_t nro_pagina,uint32_t pos_offload);

t_entrada_pagina *buscar_pagina(t_job *job, uint32_t nro_pagina);

void liberar_entrada_pagina(void *elem);

// ALLOC y FREE
uint32_t asignar_espacio_job(t_job *job,uint32_t tamanio);

void liberar_espacio_job(t_job *job, uint32_t tamanio);

// Consulta de páginas
int traducir_y_obtener_marco(uint32_t jid, uint32_t nro_pagina);

void deslockear_paginas_job(uint32_t jid);

// Acceso a memoria física
void leer_de_memoria_fisica(uint32_t dir_fisica, uint32_t tamanio, void *buffer_destino);

void escribir_en_memoria_fisica( uint32_t jid,uint32_t dir_fisica, uint32_t tamanio, void *datos);

#endif