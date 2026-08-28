#include "config.h"
#include <stdlib.h>
#include <string.h>

t_storage_config * storage_config = NULL; // reserva espacio en memoria,se inicializa en nulo
t_log * storage_logger = NULL; //reeservar espacio para el puntero para las logs commons
int cargar_configuracion(char*path_config){
    t_config * config = config_create (path_config);
    if( config == NULL){ // verificar si fallo la lectura o apertura del archivo
        return -1;
    }
    storage_config = malloc(sizeof(storage_config)); // reservamos memoria exacta del storage cofig
    if(storage_config == NULL){
        config_destroy (config);
        return -1;
    } 
    // extraemos los flujos del archivo y los guardamos en el struct 
    storage_config -> PUERTO_ESCUCHA = strdup(config_get_string_value(config, "Puerto_Escucha")); //strdup (copiar cadena de texto), get_string_value (devolver el puntero a dicho archivo)
    storage_config -> PATH_STORAGE = strdup (config_get_string_value(config, "Path_storage"));
    storage_config -> log_level = log_level_from_string(config_get_string_value(config , "log_level"));
    // a los textos los transformo a ints
    storage_config -> CANT_BLOQUES = config_get_int_value(config, "Cant_bloques");
    


}