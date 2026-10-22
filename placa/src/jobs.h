#ifndef JOBS_H_
#define JOBS_H_

#include "placa.h"

char *crear_job_en_placa(uint32_t jid, char *nombre_archivo);

char *obtener_instruccion(uint32_t jid, uint32_t pc);

void finalizar_job_en_placa(uint32_t jid);

#endif