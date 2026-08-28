#include "config.h"
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    if (argc < 2){
        printf("Error: debe ingresar la ruta del archivo de configuracion \n");
        printf("uso: [PATH_CONFIG]\n", argv[0]); // argv inicializado en la posicion 0
        return EXIT_FAILURE;
    }
// inicializo el logger
storage_logger = log_create ("storage.log", "STORAGE", true, LOG_LEVEL_INFO); // true para guardar archivos y mostrarlos por terminal, log_level info, mostrara mensajes de info/warnings/debug
 if(storage_logger ==NULL){
    perror("Error al iniciar el logger de storage");
    return EXIT_FAILURE;
 }
}
