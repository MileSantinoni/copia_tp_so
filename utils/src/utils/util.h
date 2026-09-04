#ifndef UTILS_UTIL_H_
#define UTILS_UTIL_H_

#include <commons/config.h>
#include <commons/log.h>

#include <utils/protocolo.h>

extern t_log *logger;
extern t_config *config;


char* modulo_a_string(int modulo);

t_modulo enviar_handshake(int socket_cliente);
t_modulo recibir_handshake(int socket_cliente);

void terminar_modulo(void);

#endif
