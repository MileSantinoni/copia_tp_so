#ifndef STORAGE_FILESYSTEM_H_
#define STORAGE_FILESYSTEM_H_


#include <stdint.h>
#include <stdbool.h>
#include <stdio.h> 
// el objetivo es que el modulo sepa donde empieza cada region del volumen

// valores fijos del formato FAT32_TRAIN

#define FIRMA_FS "FAT32_TRAIN"
#define TAM_FIRMA 11 // En el disco van exactamente 11 bytes sin contar el barra cero 

#define FAT_LIBRE  0x00000000 // El bloque esta totalmente libre
#define FAT_FIN 0x0FFFFFFF  // ultimo bloque
#define FAT_RESERVADO 0x0FFFFFFE // Superbloque, FAT y directorio nunca se asignan

#define TAM_NOMBRE_CHECKPOINT 23 // aca si incluimos el barra cero
#define NOMBRE_VOLUMEN "fat32_train.bin" // archivo del volumen dentro del path storage

// entrada de directorio: 32 bytes exactos campo de entrada= 1, nombre del checkpoint = 23, tam checkpoint = 4, primer bloque de datos = 4

typedef struct {
    uint8_t estado; //0 indica que esta libre | 1 indica que esta ocupado
    char nombre[TAM_NOMBRE_CHECKPOINT]; // termina en barra cero
    uint32_t tamanio;   // expresado enn bytes
    uint32_t primer_bloque; // donde empieza su cadena en la fat

}t_entrada_directorio;

// donde empieza cada region de volumen (se calculan del config)

typedef struct{
    uint32_t cant_bloques;
    uint32_t tam_bloque;

    uint32_t bloques_fat;
    uint32_t bloques_directorio;

    uint32_t primer_bloque_fat;  // siempre desde el 1, el 0 es el superbloque
    uint32_t primer_bloque_directorio;
    uint32_t primer_bloque_datos;

    uint32_t entradas_directorio; // cuantos checkpoints entran como maximo

}t_geometria;

extern t_geometria geometria; // variable del tipo de struct definido anteriormente
extern FILE *volumen;
void calcular_geometria(void); // prototipo de una funcion, que sirve para el archivo que incluya el .h
void iniciar_filesystem(void);
uint32_t leer_entrada_fat(uint32_t bloque);
void escribir_entrada_fat(uint32_t bloque, uint32_t valor);
void leer_entrada_directorio(uint32_t indice, t_entrada_directorio *entrada);
void escribir_entrada_directorio(uint32_t indice, t_entrada_directorio *entrada);
#endif