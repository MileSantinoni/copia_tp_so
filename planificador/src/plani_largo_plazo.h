#ifndef PLANI_LARGO_PLAZO_H_
#define PLANI_LARGO_PLAZO_H_

#include "planificador.h"

t_job *crear_job(uint32_t jid);
void encolar_new(t_job *job);
void enviar_crear_job_a_placa(uint32_t jid, char *nombre_archivo);
void *planificador_largo_plazo(void *arg);
void pasar_a_ready(t_job *job);
void finalizar_job(t_job *job, char *motivo);
char *estado_a_string(t_estado estado);
void log_cambio_estado(uint32_t jid, char *anterior, char *actual);

#endif