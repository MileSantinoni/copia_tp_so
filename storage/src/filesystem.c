#include "filesystem.h"
#include "config.h" // storage config y el logger
#include <stdlib.h>          
#include <unistd.h>           
#include <sys/stat.h>        
#include <commons/string.h>   
#include <string.h>
#include <pthread.h>


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

// abrimos un volumen existente, sin borrar su contenido
//verificamos que tenga el formato (fat32_train)
static void abrir_volumen_existente(char *path){
    volumen = fopen(path, "r+b"); // lo abro en lectura y escritura
    if(volumen == NULL){
        log_error(logger, "No se pudo abrir el volumen en %s", path);
        exit(EXIT_FAILURE);
    }
    // leo los primero 11 bytes, que seran la firma
    char firma[TAM_FIRMA];
     fseek(volumen, 0, SEEK_SET);
    fread(firma, TAM_FIRMA, 1, volumen);
    if(memcmp(firma, FIRMA_FS, TAM_FIRMA)!=0){
       log_error(logger, "El archivo %s no es un volumen FAT32_TRAIN valido", path);
        exit(EXIT_FAILURE);
    }
     log_info(logger, "Volumen existente abierto: %s", path);
}

// Son las unicas funciones que leen o escriben el archivo durante la ejecucion.
// Cada acceso espera RETARDO_ACCESO_BLOQUE, simulara la demora de un disco real

static void esperar_acceso_bloque(void){
   usleep(storage_config->RETARDO_ACCESO_BLOQUE * 1000); //la demora representada en micro segundos
}
// posicion en el archivo de la entrada de la fat que corresponde a bloque
static long posicion_entrada_fat(uint32_t bloque){
  long inicio_fat = (long)geometria.primer_bloque_fat *geometria.tam_bloque;
  return inicio_fat + bloque * sizeof(uint32_t); 
}
//devuelve el valor de la fat para ese bloque: el siguiente de la cadena
// fat_fin, fat_libre o fat reservado

uint32_t leer_entrada_fat(uint32_t bloque){
  uint32_t valor;
  esperar_acceso_bloque();
  fseek(volumen, posicion_entrada_fat(bloque), SEEK_SET);
    fread(&valor, sizeof(uint32_t), 1, volumen);

    log_info(logger, "## Acceso FAT - Entrada: %u - Valor: 0x%08X", bloque, valor);
    return valor;
}
//escribimos en la fat el valor de ese bloque
void escribir_entrada_fat(uint32_t bloque, uint32_t valor){
  esperar_acceso_bloque();
  fseek(volumen, posicion_entrada_fat(bloque), SEEK_SET);
  fwrite(&valor, sizeof(uint32_t), 1, volumen);
  fflush(volumen);
   log_info(logger, "## Acceso FAT - Entrada: %u - Valor: 0x%08X", bloque, valor);
}
static long posicion_entrada_directorio(uint32_t indice){
  long inicio_directorio = (long)geometria.primer_bloque_directorio * geometria.tam_bloque;
  return inicio_directorio + indice *sizeof(t_entrada_directorio);
}
// leemos la entrada del indice de directorio
void leer_entrada_directorio (uint32_t indice, t_entrada_directorio*entrada){
  esperar_acceso_bloque();
   fseek(volumen, posicion_entrada_directorio(indice), SEEK_SET);
    fread(entrada, sizeof(t_entrada_directorio), 1, volumen);
}
void escribir_entrada_directorio(uint32_t indice, t_entrada_directorio * entrada){
  esperar_acceso_bloque();
  fseek(volumen, posicion_entrada_directorio(indice), SEEK_SET);
  fwrite(entrada, sizeof(t_entrada_directorio), 1, volumen);
  fflush(volumen);
}
// leemos el bloque de datos completo

void leer_bloque(uint32_t nro_bloque, void * buffer,  char*checkpoint){
  esperar_acceso_bloque();
  fseek(volumen,(long)nro_bloque *geometria.tam_bloque, SEEK_SET);
  fread(buffer, geometria.tam_bloque, 1, volumen);
  log_info(logger, "## Acceso Bloque - Checkpoint: %s - Bloque: %u", checkpoint, nro_bloque);
}
//escribimos el buffer
void escribir_bloque(uint32_t nro_bloque, void*buffer, char*checkpoint){
   esperar_acceso_bloque();
   fseek(volumen, (long)nro_bloque*geometria.tam_bloque, SEEK_SET);
   fwrite(buffer, geometria.tam_bloque, 1, volumen);
   fflush(volumen);
    log_info(logger, "## Acceso Bloque - Checkpoint: %s - Bloque: %u", checkpoint, nro_bloque);
}

// busquedas, devolveran posicion o en caso de no enconrar nada un -1
//buscamos la entrada ocupada de ese directorio con ese nombre
// si la encuentra, la copia en entrada y devolvera su indice
static int buscar_entrada_por_nombre(char*nombre, t_entrada_directorio *entrada){
  for(uint32_t i=0;i<geometria.entradas_directorio; i++ ){
    leer_entrada_directorio(i, entrada);
    if(entrada-> estado == 1 && strcmp(entrada-> nombre, nombre) == 0 )
    return i;
  }
  return -1;
}
// devolvemos el indice de la 1er entrada
static int buscar_entrada_libre(void){
  t_entrada_directorio entrada;
  for(uint32_t i=0; i<geometria.entradas_directorio; i++){
  leer_entrada_directorio(i, &entrada);
  if(entrada.estado==0)
    return i;}
  return -1; // indicamos directorio lleno
}
// Devuelve el primer bloque de datos libre fat==0
// Arranca en el primer bloque de datos: los anteriores estan reservados
static int buscar_bloque_libre(void){
  for (uint32_t bloque =geometria.primer_bloque_datos ; bloque< geometria.cant_bloques; bloque++){
    if(leer_entrada_fat(bloque)==FAT_LIBRE)
    return bloque;
  }
  return -1; //no hay espacio
}

// cada operacion se ejecuta completa con el mutex tomado
static pthread_mutex_t mutex_filesystem = PTHREAD_MUTEX_INITIALIZER;

//cuantos bloques hacen falta para el tamanio bytes
// Un checkpoint vacio igual ocupa 1 bloque, asi siempre tiene un primer bloque.
static uint32_t bloques_necesarios(uint32_t tamanio){
  if(tamanio ==0)
  return 1;
  return (tamanio+geometria.tam_bloque -1 ) /geometria.tam_bloque;
}
//recorre la cadena de la fat de un checkpoint y devolvemos sus bloques en orden
//cuantos son, los sabremos por su tamanio
static uint32_t * obtener_cadena(t_entrada_directorio*entrada, uint32_t *cantidad){
  *cantidad = bloques_necesarios(entrada-> tamanio);
  uint32_t *bloques = malloc(*cantidad * sizeof(uint32_t));
  uint32_t actual = entrada->primer_bloque;
    for (uint32_t i = 0; i < *cantidad; i++)
    {
        bloques[i] = actual;
        actual = leer_entrada_fat(actual); // la FAT dice cual sigue
    }
    return bloques;
}

//guardamos tamanio de bytes de contenido en el checkpoint nombre
// si no exsite lo crea, sino lo reemplaza su contenido
// devuelve false si no hay lugar
bool guardar_checkpoint(char*nombre,void*contenido, uint32_t tamanio){
  if(strlen(nombre)>=TAM_NOMBRE_CHECKPOINT){
      log_error(logger, "Nombre de checkpoint demasiado largo: %s", nombre);
        return false;
  }
  pthread_mutex_lock(&mutex_filesystem);
  // numero 1, la entrada del checkpoint si ya existe o una libre si es nuevo
  t_entrada_directorio entrada;
  bool ya_existia = true;
  int indice = buscar_entrada_por_nombre(nombre, &entrada);
  if(indice == -1){
    ya_existia = false;
    indice = buscar_entrada_libre();
    if(indice ==-1){
        log_error(logger, "## ERROR - Directorio lleno al intentar guardar: %s", nombre);
            pthread_mutex_unlock(&mutex_filesystem);
            return false;
    }
  }






  //paso 2, los bloques que ya tenia (en caso que existian) y cuantos necesita ahora
  uint32_t cant_viejos=0;
  uint32_t *viejos = ya_existia ?  obtener_cadena(&entrada, &cant_viejos) : NULL;
  uint32_t cant_nuevos = bloques_necesarios(tamanio);
  uint32_t *nuevos= malloc(cant_nuevos * sizeof(uint32_t));

  //paso 3, elegir los bloques
  // primero reuso los que ya tenia

  uint32_t cant_reusados = (cant_viejos < cant_nuevos) ? cant_viejos : cant_nuevos;

// primero reuso los que ya tenia
for (uint32_t i = 0; i < cant_reusados; i++)
    nuevos[i] = viejos[i];

// y si faltan, tomo libres (cada uno lo marco FIN apenas lo tomo)
for (uint32_t i = cant_reusados; i < cant_nuevos; i++)
{
    int libre = buscar_bloque_libre();
    if (libre == -1)
    {
        // como no alcanza, devuelvo solo los que tome en este intento
        for (uint32_t j = cant_reusados; j < i; j++)
            escribir_entrada_fat(nuevos[j], FAT_LIBRE);

        log_error(logger, "## ERROR - Falta de espacio en FAT al intentar guardar: %s", nombre);
        free(viejos);
        free(nuevos);
        pthread_mutex_unlock(&mutex_filesystem);
        return false;
    }
    escribir_entrada_fat(libre, FAT_FIN);
    nuevos[i] = libre;
}

  //paso 4, encadenamos en la fat, cada bloque apunta al siguiente
  for(uint32_t i = 0; i<cant_nuevos;i++){
    uint32_t siguiente = (i+1<cant_nuevos) ? nuevos[i+1]:FAT_FIN;
    escribir_entrada_fat(nuevos[i],siguiente);
  }
  //paso 5  si ahora ocupa menos libero los bloques viejos que sobran
 for (uint32_t i = cant_reusados; i < cant_viejos; i++)
    escribir_entrada_fat(viejos[i], FAT_LIBRE);

  //paso 6 escribo el contenido, un bloque a la vez
  void * buffer= malloc(geometria.tam_bloque);
  for(uint32_t i = 0; i<cant_nuevos; i++){
    uint32_t desde = i*geometria.tam_bloque;
    uint32_t restante = tamanio - desde;
    uint32_t a_copiar = (restante < geometria.tam_bloque) ? restante : geometria.tam_bloque;
       memset(buffer, 0, geometria.tam_bloque); // lo que no se usa del bloque queda en 0
        memcpy(buffer, contenido + desde, a_copiar);
        escribir_bloque(nuevos[i], buffer, nombre);
  }
  free(buffer);

  //paso 7 actualizar la entrada del directorio
  entrada.estado = 1;
  memset(entrada.nombre, 0,TAM_NOMBRE_CHECKPOINT);
  strcpy(entrada.nombre, nombre);
    entrada.tamanio = tamanio;
    entrada.primer_bloque = nuevos[0];
    escribir_entrada_directorio(indice, &entrada);

    log_info(logger, "## Guardado Checkpoint: %s - Tamaño: %u completada", nombre, tamanio);

    free(viejos);
    free(nuevos);
    pthread_mutex_unlock(&mutex_filesystem);
    return true;
}
 //lee el checkpoint nombre completo
  // devolvemos buffer con su contenido y dejar en *tamanio
  // cuantos bytes son datos, si el checkpoint no existe devolvemos NULL

  void * leer_checkpoint(char*nombre, uint32_t *tamanio){
    pthread_mutex_lock(&mutex_filesystem);
    // buscamos la entrada
    t_entrada_directorio entrada;
    if(buscar_entrada_por_nombre(nombre, &entrada) == -1){
      log_warning(logger, "El checkpoint %s no existe", nombre);
      pthread_mutex_unlock(&mutex_filesystem);
      return NULL;
    }
    // paso 2 sus bloques en orden
    uint32_t cant_bloques;
    uint32_t * bloques = obtener_cadena(&entrada, &cant_bloques);
    // leo cada bloque uno detras del otro
    void *contenido = malloc(cant_bloques * geometria.tam_bloque);
    for(uint32_t i = 0; i<cant_bloques; i++)
    leer_bloque(bloques[i], contenido+i*geometria.tam_bloque, nombre);

    // cuantos de esos bytes son datos de verdad
    *tamanio = entrada.tamanio;
      log_info(logger, "## Carga Checkpoint: %s - Tamaño: %u completada", nombre, entrada.tamanio);

    free(bloques);
    pthread_mutex_unlock(&mutex_filesystem);
    return contenido;
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
     abrir_volumen_existente(path);
   }
   else{
    crear_y_formatear_volumen(path);
   }
free(path); // string_from_format reserva memoria nueva, por lo tanto la liberamos
    // PRUEBA TEMPORAL: borrar despues
    uint32_t tam;
    char *leido = leer_checkpoint("modelo", &tam);
    if (leido != NULL)
    {
        log_info(logger, "PRUEBA leer modelo: tamanio=%u contenido=%.*s", tam, tam, leido);
        free(leido);
    }

    char *nada = leer_checkpoint("nada", &tam);
    log_info(logger, "PRUEBA leer nada: %s", nada == NULL ? "NULL (no existe)" : "encontrado");

}