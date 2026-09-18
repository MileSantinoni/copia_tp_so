#ifndef UTILS_UTIL_H_
#define UTILS_UTIL_H_

#include <commons/config.h>
#include <commons/log.h>

#include <utils/protocolo.h>

extern t_log *logger;
extern t_config *config;


char* modulo_a_string(int modulo);

void enviar_handshake(int socket, t_modulo modulo);
t_modulo recibir_handshake(int socket);

int  recibir_core_id(int socket);

void terminar_modulo(void);

char* motivo_de_desalojo_a_string(t_motivo_desalojo motivo);

#endif
