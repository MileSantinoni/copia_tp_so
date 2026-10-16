#include "filesystem.h"
#include "config.h" // storage config y el logger

t_geometria geometria;

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
