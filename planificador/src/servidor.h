#ifndef UTILS_SERVIDOR_H_
#define UTILS_SERVIDOR_H_

#include <commons/config.h>
#include <commons/log.h>
#include <utils/comunicacion.h>
#include <stdlib.h>
#include <pthread.h>
#include <utils/protocolo.h>
#include <utils/conexion.h>


void iniciar_conexion_como_servidor();
void *atender_core(void *arg);

#endif