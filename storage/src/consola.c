#include "consola.h"
#include "filesystem.h"
#include "config.h" 
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <readline/readline.h>
#include <commons/string.h>

// Convierte un texto hexadecimal en bytes: "0A1BFF" -> [0x0A, 0x1B, 0xFF].
// Deja en *tamanio cuantos bytes son. Devuelve NULL si el texto no es valido.
static uint8_t *hexa_a_bytes (char*hexa, uint32_t *tamanio){
    size_t largo = strlen(hexa);
    //cada byte son 2 caracteres
     if (largo == 0 || largo % 2 != 0 || largo > 40)
        return NULL;
    *tamanio = largo/2;
    uint8_t *bytes = malloc(*tamanio);
    for(uint32_t i = 0; i<*tamanio; i++){
         // Tomamos los 2 caracteres de este byte: "0A", "1B"
        char par[3] = {hexa[2 * i], hexa[2 * i + 1], '\0'};
        char *fin;
        long valor = strtol(par, &fin, 16); // 16 = base hexadecimal
        if(*fin != '\0') //habia un caracter que no es hexadecimal
        {
            free(bytes);
            return NULL;
        }
         bytes[i] = (uint8_t)valor;
    }
    return bytes;
}

// WRITE <nombre> <hexa>
static void comando_write(char**partes){
    if(partes[1]==NULL || partes[2]== NULL){
         printf("Uso: WRITE <nombre> <hexa>   (ej: WRITE modelo 0A1BFF)\n");
        return;
    }
    uint32_t tamanio;
    uint8_t *bytes = hexa_a_bytes(partes[2], &tamanio);
    if(bytes==NULL){
      printf("Contenido invalido: tiene que ser hexadecimal, de a pares, hasta 20 bytes\n");
        return;
    }
    if(guardar_checkpoint(partes[1],bytes,tamanio))
     printf("Checkpoint %s guardado (%u bytes)\n", partes[1], tamanio);
    else
        printf("No se pudo guardar %s (ver el log)\n", partes[1]);

    free(bytes);
}
// CAT <nombre> <cantBytes>
static void comando_cat(char**partes){
    if(partes[1]== NULL || partes[2]==NULL){
        printf("Uso: CAT <nombre> <cantBytes>\n");
        return;
    }
    uint32_t tamanio;
    uint8_t * contenido = leer_checkpoint(partes[1], &tamanio);
    if (contenido == NULL){
        printf("El checkpoint %s no existe\n", partes[1]);
        return;
    }
    // no muestro mas bytes de los que tiene el checkpoint
    uint32_t cantidad = atoi(partes[2]);
    if(cantidad>tamanio)
    cantidad =tamanio;
     printf("%s (%u de %u bytes):", partes[1], cantidad, tamanio);
     for(uint32_t i = 0; i<cantidad; i++)
       printf(" %02X", contenido[i]);
    printf("\n");

    free(contenido);
}

// RM <nombre>
static void comando_rm(char ** partes){
    if(partes[1]==NULL){
         printf("Uso: RM <nombre>\n");
        return;
    }
      if (eliminar_checkpoint(partes[1]))
        printf("Checkpoint %s eliminado\n", partes[1]);
    else
        printf("El checkpoint %s no existe\n", partes[1]);
}


//recibe una linea ya escrita por el usuario y ejecuta comando que segun corresponda

static void procesar_comando(char*linea){
    while(*linea == ' ')
    linea++;
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
     else if (strcmp(comando, "WRITE") == 0)   
    {
        comando_write(partes);
    }
    else if (strcmp(comando, "CAT") == 0)     
    {
        comando_cat(partes);
    }
        else if (strcmp(comando, "RM") == 0)
    {
        comando_rm(partes);
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