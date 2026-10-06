#ifndef UTILS_COMUNICACION_H_
#define UTILS_COMUNICACION_H_

#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netdb.h>
#include<commons/log.h>
#include<commons/collections/list.h>
#include<string.h>
#include<assert.h>
#include <utils/protocolo.h>
#include <pthread.h>
#include <semaphore.h>

extern t_log *logger;

typedef struct
{
	int size;
	void* stream;
} t_buffer;

typedef struct
{
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;

// PETICIONES
// Hice esta estructura para poder manejar las respuestas de confirmacion, porque si se envia solo un mensaje no podemos manejar el error y/o podemos tener dos recv en un mismo socket
typedef struct {
	u_int32_t id_solicitd; // si un proceso hace mas de una MISMA operacion
	op_code operacion;
	u_int32_t jid;
	bool exito;
	sem_t sem_respuesta;
}t_solicitud_pendiente;

extern t_list *solicitudes_pendientes;
extern pthread_mutex_t mutex_lista_sol_pendientes;

u_int32_t siguiente_id_solicitud();
t_solicitud_pendiente* crear_solicitud_pendiente(u_int32_t jid, int operacion);
void eliminar_solicitud_pendiente(t_solicitud_pendiente* peticion);
t_solicitud_pendiente*  buscar_solicitud_por_id_solicitud(t_list* solicitudes_pendientes, u_int32_t id);

void crear_buffer(t_paquete *paquete);
t_paquete *crear_paquete(op_code codigo_operacion);
void agregar_a_paquete(t_paquete *paquete, void *valor, int tamanio);
void *serializar_paquete(t_paquete *paquete, int bytes);
void enviar_paquete(t_paquete *paquete, int socket_cliente);
void eliminar_paquete(t_paquete *paquete);
void enviar_mensaje(char *mensaje, int socket_cliente);
void *recibir_buffer(int *size, int socket_cliente);
t_list *recibir_paquete(int socket_cliente);
void recibir_mensaje(int socket_cliente);
int recibir_operacion(int socket_cliente);
char *recibir_nombre(int socket_cliente);

void enviar_contexto(int socket_destino, op_code codigo, uint32_t jid, t_registros* registros);
void recibir_contexto(int socket_origen, uint32_t* jid, t_registros* registros);

#endif