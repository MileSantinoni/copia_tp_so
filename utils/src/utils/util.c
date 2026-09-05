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

void enviar_handshake(int socket, t_modulo modulo) // MODULO --> El que envia la funcion 
{

    t_paquete* paquete = crear_paquete(CONEXION);

    agregar_a_paquete(paquete, &modulo, sizeof(t_modulo));
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);

    log_info(logger, "Handshake enviado a socket: %i", socket);

}

t_modulo recibir_handshake(int socket){
   
    int cod_op = recibir_operacion(socket);
    if (cod_op != CONEXION) {
        log_error(logger, "Error: Se esperaba un Handshake (op_code: CONEXION) pero se recibio op_code %d", cod_op);
        return -1;
    }

    t_list* valores = recibir_paquete(socket);
    if (valores == NULL || list_size(valores) == 0) {
        log_error(logger, "Error: No se pudieron recibir datos del handshake");
        if (valores != NULL) list_destroy(valores);
        return -1;
    }

    t_modulo* modulo_ptr = list_get(valores, 0);
    t_modulo modulo_remitente = *modulo_ptr;

    char* modulo_s = modulo_a_string(modulo_remitente);
    log_info(logger, "## Módulo: %s", modulo_s);

    list_destroy_and_destroy_elements(valores, free);

    return modulo_remitente;
}


void terminar_modulo(void)
{
    log_destroy(logger);
    config_destroy(config);
}

int recibir_core_id(int socket){
   
    int cod_op = recibir_operacion(socket);
    if (cod_op != ENVIO_ID) {

        log_error(logger, "Error: Se esperaba un Handshake (op_code: MENSAJE) pero se recibio op_code %d", cod_op);
        exit(EXIT_FAILURE);
    }

    t_list* valores = recibir_paquete(socket);
    if (valores == NULL || list_size(valores) == 0) {
        log_error(logger, "Error: No se pudieron recibir datos del paquete");
        if (valores != NULL) list_destroy(valores);
         exit(EXIT_FAILURE);
    }

    int id_recibido = *(int*) list_get(valores, 0);
    list_destroy_and_destroy_elements(valores, free);

    return id_recibido;
}