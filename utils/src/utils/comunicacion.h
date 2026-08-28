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

#endif