#include "filesystem.h"
#include "config.h" // storage config y el logger
#include <stdlib.h>          
#include <unistd.h>           
#include <sys/stat.h>        
#include <commons/string.h>   
#include <string.h>

t_geometria geometria;
FILE *volumen = NULL;

// primero se calcula donde empieza cada region del volumen a partir del config
//bloque 0 -> superbloque
//bloque 1 y los que siguen -> bloque fat
// siguientes bloques_directorio 

// el resto son bloques de datos

void calcular_geometria(void){
    geometria.cant_bloques = storage_config -> CANT_BLOQUES;
    geometria.tam_bloque = storage_config -> TAM_BLOQUE;
    geometria.bloques_directorio = storage_config -> BLOQUES_DIRECTORIO;

    // la fat tiene un tamaño de uint32 por cada bloque 
    // si sobra un pedazo de bloque, igualmente se lo ocupa entero

    uint32_t bytes_fat = geometria.cant_bloques * sizeof(uint32_t); //entrada por cada bloque del volumen y cada entrada es uint_32
    geometria.bloques_fat = (bytes_fat + geometria.tam_bloque -1 ) / geometria.tam_bloque; // hay que repartir los 4096 en 64 bloques

    geometria.primer_bloque_fat = 1; // el bloque 0 es el superbloque, por eso lo seteamos en 1
    geometria.primer_bloque_directorio = geometria.primer_bloque_fat + geometria.bloques_fat; // arranca en 1 y ocupa 64, termina en el 64 y el directorio arranca en el 65
    geometria.primer_bloque_datos = geometria.primer_bloque_directorio + geometria.bloques_directorio; // el directorio arranca en 65  ocupa 64 por lo que , los datos empiezan en 129

    geometria.entradas_directorio = geometria.bloques_directorio * geometria.tam_bloque / sizeof(t_entrada_directorio); // checkpoints que entran, cada entrada mide 32 bytes por lo tanto son 128 entradas

    log_debug(logger, "Geometria: FAT en bloques %u-%u, directorio en %u-%u, datos desde %u (%u entradas de directorio)", // muestra los resultados para verificar las cuentas 
     geometria.primer_bloque_fat, geometria.primer_bloque_directorio -1, // los -1 indican que el ultimo bloque de una region es el anterior al primero de la siguiente la fat termina en 64 y directorio en 128
     geometria.primer_bloque_directorio, geometria.primer_bloque_datos - 1,
     geometria.primer_bloque_datos, geometria.entradas_directorio);
}
static void escibir_superbloque(void){ // escribe el superbloque en el bloque 0, campo por campo
 void *bloque = malloc(geometria.tam_bloque);
 memset(bloque, 0, geometria.tam_bloque);

 int desplazamiento =0;
  // firma exactamente 11 bytes sin el barra cero del final
  memcpy(bloque+desplazamiento, FIRMA_FS, TAM_FIRMA);
  desplazamiento += TAM_FIRMA;

  // tamanio del bloque 
  memcpy(bloque+desplazamiento, &geometria.tam_bloque, sizeof(uint32_t));
  desplazamiento+= sizeof(uint32_t); 

  // cantidad de bloques del filesystem
  memcpy(bloque+desplazamiento, &geometria.cant_bloques, sizeof(uint32_t));
  desplazamiento += sizeof(uint32_t);

  // cantidad de bloques del directorio
  memcpy(bloque+desplazamiento, &geometria.bloques_directorio, sizeof(uint32_t));
  desplazamiento+= sizeof(uint32_t);

  // el superbloque es el bloque 0 que empieza en el byte 0 del archivo
  fseek(volumen,0, SEEK_SET);
  fwrite(bloque, geometria.tam_bloque,1,volumen);

  free(bloque);
}
// marcamos en la fat como reservados(0fffffe)los bloques Que NO son de datos
// el superBloque, la fat misma y el directorio asi nunca se los asigna a un checkpoint
static void marcar_bloques_reservados(void){

  // en primer lugar guardamos la direccion del dato,en una variable
  uint32_t reservado = FAT_RESERVADO;

  // bloque en donde empieza el archivo de la fat, byte 64
   long inicio_fat = (long)geometria.primer_bloque_fat * geometria.tam_bloque;

   // bloques 0 a 128, anteriores al primero de datos
   for(uint32_t bloque = 0; bloque < geometria.primer_bloque_datos; bloque++){
    // nos paramos en la entrada de este bloque y escribimos sus 4 byte
     fseek(volumen, inicio_fat + bloque * sizeof(uint32_t), SEEK_SET);
        fwrite(&reservado, sizeof(uint32_t), 1, volumen);
   }



} 
static void crear_y_formatear_volumen(char*path){
    volumen = fopen(path, "w+b");
    if(volumen == NULL){
         log_error(logger, "No se pudo crear el volumen en %s", path);
        exit(EXIT_FAILURE);
    }
    // agrandamos el archivo al tamanio total. los bytes nuevos quedan en 0
    long tamanio_total = (long)geometria.cant_bloques * geometria.tam_bloque;
    ftruncate(fileno(volumen), tamanio_total); // truncara segun tamaño indicado por el usuario
    escibir_superbloque();
    marcar_bloques_reservados();
    fflush(volumen); // fuerza que lo escrito llegue al archivo
    log_info(logger, "Volumen creado y formateado: %s (%ld bytes)", path, tamanio_total);
}


void iniciar_filesystem(void){ // se inicia el filesystem una vez al iniciar el storage
// se decide si hay que abrir un volumen que ya existe o si hay que crear uno nuevo
 calcular_geometria(); // todo lo que viene usa la geometria, por lo que debemos hacer los calculos correspondientes primero
 
   // nos aseguramos que exista la carpeta del storage
   // en caso de existir mkdir falla sin hacer nada
   mkdir(storage_config->PATH_STORAGE, 0777); // 0777 maximo de permisos

   // se arma la ruta completa carpeta y nombre del archivo
   char *path = string_from_format ("%s%s", storage_config->PATH_STORAGE, NOMBRE_VOLUMEN);

   // verifico si el archivo existia anteriormente

   if (access(path, F_OK)== 0){
    log_info(logger, "El volumen existe: %s", path);
   }
   else{
    crear_y_formatear_volumen(path);
   }
free(path); // string_from_format reserva memoria nueva, por lo tanto la liberamos

}