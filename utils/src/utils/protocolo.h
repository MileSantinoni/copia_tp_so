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
    ENVIO_TIPO_CONEXION,
    RESPUESTA_SOLICITUD,

    // Operaciones check N2

    // PLANI <--> PLACA
    CREAR_JOB, //planificador crea job y se lo manda a la placa
    INIT_JOB,
    // PLANI <--> CORE
    //DEVOLVER_CONTEXTO,
    DESPACHAR_JOB, // planificador despacha el job al core
    DESALOJAR_JOB,  // Fin de quantum / interrupcion 
    INTERRUPCION, // Planificador le envia al core
    FINALIZAR_JOB, // core le envia al planificador y el planificador a placa+

    // CORE <--> PLACA
    FETCH_INSTRUCCION, // Core le pide la instruccion a la placa
    RESPUESTA_INSTRUCCION, // placa responde al core
    RESPUESTA_SYSCALL,

    // core <-> placa (MMU)
    PEDIR_TAM_PAGINA, //Core->placa: El core no tiene TAM_PAGINA en su config
    RESPUESTA_TAM_PAGINA, // Placa le responde al core, el tamaño de pagina
    OBTENER_MARCO, //el core le solicita a la placa [Jid: nro_pagina]
    RESPUESTA_MARCO, // placa le responde al core [marco] 
    RESPUESTA_PAGE_FAULT, //placa le responde al core: La pagina no esta en memoria


    // Core <-> planificador
    LEER_MEMORIA, // [Jid, dir__fisica, tamanio]
    RESPUESTA_LECTURA, // Placa --> contenido
    ESCRIBIR_MEMORIA, //[Jid, dir_fisica, Tamanio_contenido]
    RESPUESTA_ESCRITURA, //Placa responde que esta todo ok
    DESLOCKEAR_PAGINAS, // [Jid]

    // Planificador <-> placa
    ASIGNAR_ESPACIO, //alloc [jid,tamanio] -> responde una direccion logica
    LIBERAR_ESPACIO, //Liberamos [jid, dir_logica]
    CARGAR_PAGINA, //[jid, nro_pagina]

    // Core <-> planificador
    SYSCALL_ALLOC, //[jid, tamanio]  respuesta a la syscall con la dir logica
    SYSCALL_FREE, // [jid, direccion logica] respuesta a la syscall

    
  
} op_code;

typedef enum
{
    FIN_QUANTUM,
    SYSCALL_BLOQUEANTE,
    PAGE_FAULT, // Block para pedir a la placa la pagina, luego de eso ready
    SYSCALL_LOAD_BATCH, 
    SYSCALL_LABEL,
    SYSCALL_REPORT,
    // Usamos un motivo por servicio, asi el switch de desalojar job, sabe a que cola mandar el job
} t_motivo_desalojo;



typedef enum
{
    CONEXION_DISPATCH, // Conexión para recibir las instruccciones a ejecutar 
    CONEXION_INTERRUPT // Conexión para recibir las interrupciones
} t_tipo_conexion_core;


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
} t_registros;

typedef struct
{
    uint32_t direccion_fisica;
    uint32_t tamanio;
} t_direccion_fisica;



#endif