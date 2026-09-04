#ifndef PROTOCOLO_H_
#define PROTOCOLO_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>

/*typedef enum
{
    MENSAJE,
    HANDSHAKE_PLANIFICADOR,
    HANDSHAKE_CORE,
    HANDSHAKE_PLACA,
    HANDSHAKE_STORAGE,
    PAQUETE_PRUEBA
} op_code; --> mesajes generales que usamos todos van en utils para no repertir codigo y q sea consistente

typedef struct
{
    op_code codigo_operacion;
    uint32_t tamanio_payload;
    void* payload;
} t_paquete;

// --- Funciones para Mensajes Simples / Confirmaciones ---
void enviar_mensaje(char* mensaje, int socket_cliente);
char* recibir_mensaje(int socket_cliente);

// --- Funciones para Paquetes con Datos ---
t_paquete* crear_paquete(op_code codigo);
void agregar_a_paquete(t_paquete* paquete, void* valor, uint32_t tamanio);
void agregar_string_a_paquete(t_paquete* paquete, char* string);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
t_paquete* recibir_paquete(int socket_cliente);
void eliminar_paquete(t_paquete* paquete);*/

#endif /* PROTOCOLO_H_ */