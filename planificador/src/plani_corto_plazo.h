#ifndef PLANI_CORTO_PLAZO_H_
#define PLANI_CORTO_PLAZO_H_

#include <stdlib.h>

#include "planificador.h"

void *planificador_corto_plazo(void *arg);
void despachar_proceso();
void *controlar_quantum(void* arg);
void iniciar_quantum(t_job *job_elegido, t_core *core_elegido);


typedef struct {
    t_job* job;
    t_core* core;
    u_int32_t quantum;
} t_args_quantum;

#endif