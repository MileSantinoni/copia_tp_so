#include <utils/util.h>
#include <utils/protocolo.h>
#include <utils/comunicacion.h>

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

void enviar_handshake(int socket, t_modulo modulo){

  t_paquete* paquete = crear_paquete(CONEXION);

  
    char* modulo_s = modulo_a_string(modulo);

    agregar_a_paquete(paquete, &modulo, sizeof(t_modulo));
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);

    log_info(logger, "Handshake enviado a %s", modulo_s);

}

t_modulo recibir_handshake(int socket){

   // Falta implementar 
}


void terminar_modulo(void)
{
    log_destroy(logger);
    config_destroy(config);
}