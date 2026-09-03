#ifndef  STORAGE_CONFIG_H_ // Directivas del procesador 
#define  STORAGE_CONFIG_H_ // directivas del procesador
#include <commons/config.h> //Librerias que importan herramientas oficiales, so commons_library
#include <commons/log.h>

typedef struct{
    char * PUERTO_ESCUCHA;
    t_log_level log_level; // delega mensajes que van a salir, segun importancia en este caso le pasamos el info de config
    char * PATH_STORAGE;
    int CANT_BLOQUES;
    int TAM_BLOQUE;
    int BLOQUES_DIRECTORIO;
    int RETARDO_ACCESO_BLOQUE;
    int RETARDO_COMMIT;
} t_storage_config; // para luego poder invocar el struct de manera mas sencilla

extern t_storage_config * storage_config; // puntero el cual apuntara, a la memoria struct 
extern t_log * storage_logger; // puntero, el cual apunta a la instancia logger

int cargar_configuracion(char*path_config); // cargar config cuando inciemos el programa
void destruir_configuracion (void); // limpieza de memoria una vez que el programa finalice

#endif

