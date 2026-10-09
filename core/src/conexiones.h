#ifndef CONEXIONES_H_
#define CONEXIONES_H_

#include <commons/log.h>
#include <commons/collections/list.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>
#include <pthread.h>

#include <utils/protocolo.h>


// extern int hay_interrupcion; // flag compartido entre el hilo de escucha y el hilo del ciclo de instruccion
// declaramos una lista de interrupciones y evitamos perderlas
extern t_list *interrupciones_pendientes;
extern pthread_mutex_t mutex_interrupcion; //mutex el cual sirve para evitar condiciones de carrera en el acceso y modificaciones en hay_interrupcion
extern int socket_dispatch;
extern int socket_interrupt;
extern int socket_placa;

typedef struct {
    uint32_t jid;
    t_motivo_desalojo motivo;
} t_interrupcion_pendiente;

int conectar_con_planificador(char* ip, char* puerto, int id_core, int  tipo);
int conectar_con_placa(char* ip, char* puerto, int id_core);

void crear_hilo_escucha(void);
void* escuchar_servidores(void *arg);

void enviar_id(int socket_planificador, int id);

bool tomar_interrupcion_para_jid(
    uint32_t jid,
    t_motivo_desalojo *motivo
);

void descartar_interrupciones_de_otros_jids(
    uint32_t jid_actual
);

#endif