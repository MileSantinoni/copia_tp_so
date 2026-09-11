#include "registro.h"
#include <string.h>

t_registros_cpu registros_cpu;

void incializar_registros(void){
    memset(&registros_cpu, 0, sizeof(t_registros_cpu)); // inicializo todos los registros declarados anteriormente en cero 
}

void cargar_contexto(t_registros_cpu * registros_nuevos){
    memcpy(&registros_cpu, registros_nuevos , sizeof(t_registros_cpu)); // destino -> origen -> cant de bytes 
}

uint32_t obtener_valor_registro(char * nombre_registro){
    if(strcmp(nombre_registro, "PC")== 0) return registros_cpu.PC; // si la cadena que llega son identicas, retornamos el registro correspondiente
    if(strcmp(nombre_registro, "AX")== 0) return registros_cpu.AX;
    if(strcmp(nombre_registro, "BX")== 0) return registros_cpu.BX;
    if(strcmp(nombre_registro, "CX" )== 0) return registros_cpu.CX;
    if(strcmp(nombre_registro, "DX")== 0) return registros_cpu.DX;
    if(strcmp(nombre_registro, "P0")== 0) return registros_cpu.P0;
    if(strcmp(nombre_registro, "P1")== 0) return registros_cpu.P1;
    if(strcmp(nombre_registro, "P2")== 0) return registros_cpu.P2;
    if(strcmp(nombre_registro, "P3")== 0) return registros_cpu.P3;
    if(strcmp(nombre_registro, "P4")== 0) return registros_cpu.P4;
    if(strcmp(nombre_registro, "P5")== 0) return registros_cpu.P5;
    if(strcmp(nombre_registro, "E1")== 0) return registros_cpu.E1;
    if(strcmp(nombre_registro, "E2")== 0) return registros_cpu.E2;
    if(strcmp(nombre_registro, "E3")== 0) return registros_cpu.E3;
    return 0;
}

bool asignar_valor_registro (char * nombre_registro, uint32_t valor){
    if (strcmp(nombre_registro, "PC") == 0) { registros_cpu.PC = valor; return true; } // si coincide el registro, entocnes a ese registro  se le asigna el valor correspondiente
    if (strcmp(nombre_registro, "AX") == 0) { registros_cpu.AX = valor; return true; }
    if (strcmp(nombre_registro, "BX") == 0) { registros_cpu.BX = valor; return true; }
    if (strcmp(nombre_registro, "CX") == 0) { registros_cpu.CX = valor; return true; }
    if (strcmp(nombre_registro, "DX") == 0) { registros_cpu.DX = valor; return true; }
    if (strcmp(nombre_registro, "P0") == 0) { registros_cpu.P0 = valor; return true; }
    if (strcmp(nombre_registro, "P1") == 0) { registros_cpu.P1 = valor; return true; }
    if (strcmp(nombre_registro, "P2") == 0) { registros_cpu.P2 = valor; return true; }
    if (strcmp(nombre_registro, "P3") == 0) { registros_cpu.P3 = valor; return true; }
    if (strcmp(nombre_registro, "P4") == 0) { registros_cpu.P4 = valor; return true; }
    if (strcmp(nombre_registro, "P5") == 0) { registros_cpu.P5 = valor; return true; }
    if (strcmp(nombre_registro, "E1") == 0) { registros_cpu.E1 = valor; return true; }
    if (strcmp(nombre_registro, "E2") == 0) { registros_cpu.E2 = valor; return true; }
    if (strcmp(nombre_registro, "E3") == 0) { registros_cpu.E3 = valor; return true; }
    return false;
}