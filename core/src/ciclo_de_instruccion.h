#ifndef CICLO_DE_INSTRUCCION_H
#define CICLO_DE_INSTRUCCION_H

#include <stdint.h>
#include <utils/protocolo.h>

void ciclo_de_instruccion(int socket_dispatch, int socket_placa, uint32_t jid);


t_instruccion *decodificar_instruccion(char *linea);


void destruir_instruccion(t_instruccion *instruccion);


uint32_t obtener_valor_operando(char *operando);


t_resultado_ejecucion ejecutar_instruccion(t_instruccion *instruccion, uint32_t jid);

#endif 