#ifndef CORE_MMU_H_
#define CORE_MMU_H_

#include <stdbool.h>
#include <stdint.h>

#include <commons/collections/list.h>

#include <utils/protocolo.h>

typedef enum
{
    MMU_OK,
    MMU_PAGE_FAULT,
    MMU_ERROR
} t_resultado_mmu;

typedef struct
{
    uint32_t direccion_logica;
    uint32_t nro_pagina;
    uint32_t desplazamiento;
    uint32_t marco;
    uint32_t direccion_fisica;
} t_traduccion_mmu;

// una porcion fisicamente contigua de un acceso lógico
typedef struct
{
    uint32_t direccion_fisica;
    uint32_t tamanio;
} t_segmento_fisico;

// solicita el tamaño de la página a placa
bool mmu_inicializar(int socket_placa);

// esto es para setear a mano el tamaño, puede servir para test
bool mmu_set_tamanio_pagina(uint32_t tamanio_pagina);

uint32_t mmu_get_tamanio_pagina(void);

bool mmu_separar_direccion(
    uint32_t direccion_logica,
    uint32_t *nro_pagina,
    uint32_t *desplazamiento);

t_resultado_mmu mmu_traducir_direccion(
    int socket_placa,
    uint32_t jid,
    uint32_t direccion_logica,
    t_traduccion_mmu *traduccion
    // uint32_t *pagina_page_fault
    );

t_resultado_mmu mmu_traducir_rango(
    int socket_placa,
    uint32_t jid,
    uint32_t direccion_logica,
    uint32_t tamanio,
    t_list **segmentos_fisicos);

void mmu_destruir_segmentos(t_list *segmentos_fisicos);

t_resultado_mmu mmu_leer_rango(
    int socket_placa,
    uint32_t jid,
    uint32_t direccion_logica,
    uint32_t tamanio,
    void *buffer_destino);

t_resultado_mmu mmu_escribir_rango(
    int socket_placa,
    uint32_t jid,
    uint32_t direccion_logica,
    uint32_t tamanio,
    const void *buffer_origen);

t_resultado_mmu mmu_leer_uint32(
    int socket_placa,
    uint32_t jid,
    uint32_t direccion_logica,
    uint32_t *valor);

t_resultado_mmu mmu_escribir_uint32(
    int socket_placa,
    uint32_t jid,
    uint32_t direccion_logica,
    uint32_t valor);

bool mmu_deslockear_paginas(
    int socket_placa,
    uint32_t jid);

#endif