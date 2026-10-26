#ifndef CORE_H_
#define CORE_H_

#include <commons/log.h>
#include <commons/config.h>
#include <utils/protocolo.h>
#include <stdint.h>

extern t_log* logger;
extern t_config* config;
// extern int hay_interrupcion;

typedef enum {
    EJECUCION_CONTINUA,
    EJECUCION_FINALIZADA,
    EJECUCION_DESALOJADA,
    EJECUCION_PAGE_FAULT,
    EJECUCION_BLOQUEADA,
    EJECUCION_ERROR
} t_resultado_ejecucion;

typedef struct {
    char *nombre;
    char **parametros;
    int cantidad_parametros;
} t_instruccion;

t_instruccion *decodificar_instruccion(char *linea);
void destruir_instruccion(t_instruccion *instruccion);

uint32_t obtener_valor_operando(char *operando);

t_resultado_ejecucion ejecutar_instruccion(
    t_instruccion *instruccion,
    uint32_t jid,
    int socket_placa,
    int socket_dispatch
);


#endif