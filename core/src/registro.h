#ifndef REGISTRO_H_
#define REGISTRO_H_

#include <stdint.h>
#include <stdbool.h>
#include <utils/protocolo.h>

// declaro variable global del core que mantiene los registros del proceso en ejecuccion
extern t_registros registros_cpu;

//inicializo registros en 0
void incializar_registros(void);

// cargo en el core, el contexto recibido del planificador 
void cargar_contexto (t_registros * registros_nuevos);

//obtenemos el valor de los registros pasando su nombre como string
uint32_t obtener_valor_registro ( char* nombre_registro);

// escribir un valor en un registro pasando su nombre como string
bool asignar_valor_registro (char * nombre_registro, uint32_t valor);
bool setear_valor_registro(char *nombre_registro, uint32_t valor);

#endif