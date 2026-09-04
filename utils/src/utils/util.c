#include <utils/util.h>
#include <utils/protocolo.h>

char* modulo_a_string(int modulo){

    char* respuesta;

    if(modulo==PLANIFICADOR){

        respuesta = "PLANIFICADOR";

    }else if(modulo==CORE){

        respuesta = "CORE";
       
    }else if(modulo==PLACA){

        respuesta = "PLACA";
       
    }else if(modulo==STORAGE){

        respuesta = "STORAGE";

    }else{

        log_error(logger, "Modulo desconocido");
    }

    return respuesta; 
}


void terminar_modulo(void)
{
    log_destroy(logger);
    config_destroy(config);
}