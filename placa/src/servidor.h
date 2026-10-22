#ifndef SERVIDOR_H_
#define SERVIDOR_H_

#include "placa.h"

void esperar_conexiones(int socket_servidor);
void *atender_cliente(void *arg);

#endif