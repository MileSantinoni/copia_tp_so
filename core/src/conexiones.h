#ifndef CONEXIONES_H_
#define CONEXIONES_H_

#include <commons/log.h>
#include <pthread.h>
#include <stdint.h>
#include <pthread.h>


extern int hay_interrupcion; // flag compartido entre el hilo de escucha y el hilo del ciclo de instruccion
extern pthread_mutex_t mutex_interrupcion; //mutex el cual sirve para evitar condiciones de carrera en el acceso y modificaciones en hay_interrupcion
extern int socket_dispatch;
extern int socket_interrupt;
extern int socket_placa;

int conectar_con_planificador(char* ip, char* puerto, int id_core);
int conectar_con_placa(char* ip, char* puerto, int id_core);

void crear_hilo_escucha(void);
void* escuchar_servidores(void *arg);

void enviar_id(int socket_planificador, int id);

#endif