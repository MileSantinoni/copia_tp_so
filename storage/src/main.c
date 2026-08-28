#include "config.h"
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    if (argc < 2){
        printf("Error: debe ingresar la ruta del archivo de configuracion \n");
        printf("uso: %s [PATH_CONFIG]\n", argv[0]); // argv inicializado en la posicion 0
        return EXIT_FAILURE;
    }
// inicializo el logger
storage_logger = log_create ("storage.log", "STORAGE", true, LOG_LEVEL_INFO); // true para guardar archivos y mostrarlos por terminal, log_level info, mostrara mensajes de info/warnings/debug
 if(storage_logger ==NULL){
    perror("Error al iniciar el logger de storage");
    return EXIT_FAILURE;
 }
 // cargo configuracion
 if(cargar_configuracion(argv[1])!=0) {// argv [1]arreglo de strings en este caso ubicado en la posicion 1
 log_error(storage_logger, "No se pudo cargar el archivo: %s", argv[1]);
 log_destroy(storage_logger);
 return EXIT_FAILURE;
}
log_info (storage_logger, "configuracion cargada con exito en puerto: %s", storage_config -> PUERTO_ESCUCHA);

// limpieza final
destruir_configuracion();
log_destroy(storage_logger);
return EXIT_SUCCESS;
}
