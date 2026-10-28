#ifndef CONEXION_PLACA_H_
#define CONEXION_PLACA_H_

#include <stdbool.h>
#include <stdint.h>
#include <commons/collections/list.h>

// conexiones DEDICADAS a Placa (pedido -> respuesta sincronico).
// lee socket_placa el hilo de escucha y empareja por id_solicitud, estas operaciones de Placa no devuelven id.

int conectar_placa_dedicada(char *uso);

bool placa_cargar_pagina(int socket, uint32_t jid, uint32_t nro_pagina);
bool placa_deslockear_paginas(int socket, uint32_t jid);

// direcciones: lista de t_direccion_fisica*, una por pagina involucrada
bool placa_leer_memoria_job(int socket, uint32_t jid, t_list *direcciones, void *buffer_destino);
bool placa_escribir_memoria_job(int socket, uint32_t jid, t_list *direcciones, void *buffer_origen);

#endif