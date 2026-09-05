#ifndef UTILS_PROTOCOLO_H_
#define UTILS_PROTOCOLO_H_

typedef enum
{
    // ACA IRIA LAS OPERACIONES QUE SE ENVIAN ENTRE MODULOS
    MENSAJE,
	PAQUETE,
    CONEXION, //  Agregue este para que el op_code del paquete se este y la info que agregamos es el t_modulo correspondinete
    ENVIO_ID, 
} op_code;

typedef enum
{
    PLANIFICADOR,
    CORE,
    PLACA,
	STORAGE,
} t_modulo;


#endif