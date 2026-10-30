#include "consola.h"
#include "filesystem.h"
#include "config.h" // logger

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <readline/readline.h>
#include <commons/string.h>

//recibe una linea ya escrita por el usuario y ejecuta comando que segun corresponda

static void procesar_comando(char*linea){
    // "CAT modelo 10" -> ["CAT", "modelo", "10", NULL]
    char **partes = string_split(linea, " ");
    char *comando = partes[0];

     if (comando == NULL || strlen(comando) == 0)
    {
        // linea vacia: no hacemos nada
    }
    else if (strcmp(comando, "INFO") == 0)
    {
        mostrar_superbloque();
    }
    else if (strcmp(comando, "LS") == 0)
    {
        listar_checkpoints();
    }
    else
    {
        printf("Comando desconocido: %s\n", comando);
        printf("Comandos: INFO, LS, CAT <nombre> <bytes>, WRITE <nombre> <hexa>, RM <nombre>\n");
    }

    string_array_destroy(partes);
}


// hilo de la consola, lee una linea por vez y la procesa, para siempre
static void * ejecutar_consola(void*arg){
    while(1){
        char * linea = readline("> ");

        if(linea == NULL)
        break;
    procesar_comando(linea);
    free(linea); // readline reservo memoria para cada linea
    }
    return NULL;
}

// Crea el hilo de la consola. Va en un hilo aparte porque el hilo principal
// se queda bloqueado esperando conexiones.
void iniciar_consola(void)
{
    pthread_t hilo_consola;
    pthread_create(&hilo_consola, NULL, ejecutar_consola, NULL);
    pthread_detach(hilo_consola);
}