#ifndef UTILS_PROTOCOLO_H_
#define UTILS_PROTOCOLO_H_
#include <stdint.h>

typedef enum
{
    // ACA IRIA LAS OPERACIONES QUE SE ENVIAN ENTRE MODULOS
    MENSAJE,
	PAQUETE,
    CONEXION, //  Agregue este para que el op_code del paquete se este y la info que agregamos es el t_modulo correspondinete
    ENVIO_ID, 

    // Operaciones check N2
    CREAR_JOB, //planificador crea job y se lo manda a la placa
    FETCH_INSTRUCCION, // Core le pide la instruccion a la placa
    RESPUESTA_INSTRUCCION, // placa responde al core
    DESPACHAR_JOB, // planificador despacha el job al core
    DESALOJAR_JOB,  // Fin de quantum / interrupcion 
    INTERRUPCION, // Planificador le envia al core
    FINALIZAR_JOB, // core le envia al planificador y el planificador a placa
} op_code;


typedef enum
{
    PLANIFICADOR,
    CORE,
    PLACA,
	STORAGE,
} t_modulo;

//Estructura de registros del core exigidos  
typedef struct 
{
    uint32_t PC;
    uint32_t AX;
    uint32_t BX;
    uint32_t CX;
    uint32_t DX;
    uint32_t P0;
    uint32_t P1;
    uint32_t P2;
    uint32_t P3;
    uint32_t P4;
    uint32_t P5;
    uint32_t E1;
    uint32_t E2;
    uint32_t E3;
}; t_registros_cpu;



#endif