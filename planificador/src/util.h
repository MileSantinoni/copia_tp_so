#ifndef UTIL_H_
#define UTIL_H_

#include "planificador.h"

// BUSQUEDA DE LISTAS

t_core *obtener_core_libre();
t_job *buscar_job_por_jid(t_list *lista_job, u_int32_t jid);
t_core *buscar_core_por_socket_dispatch(t_list *lista_core, int socket);
t_core *buscar_core_por_id(t_list* lista_cores, u_int32_t  id_core);

// ESTADOS
char *estado_a_string(t_estado estado);
void log_cambio_estado(uint32_t jid, char *anterior, char *actual);

// CAMBIOS DE ESTADO
void encolar_new(t_job *job);
void pasar_a_ready(t_job *job);
void pasar_a_exec(t_job *job);
void finalizar_job(t_job *job, char *motivo, u_int32_t id_core);

// LIBERAR SOCKET
void liberar_socket_core(int socket_core);


void actualizar_contexto(u_int32_t jid, t_registros *registros);

#endif