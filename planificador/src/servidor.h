#ifndef SERVIDOR_H_
#define SERVIDOR_H_

#include <stdlib.h>
#include <stdbool.h>

void iniciar_conexion_como_servidor();
void *atender_core(void *arg);

bool comparar_por_socket(void* ptr);

#endif