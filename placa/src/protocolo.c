#include "protocolo.h"

// Esto ya esta en utils, no deberia estar aca
/*t_paquete* crear_paquete(op_code codigo)
{
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = codigo;
    paquete->tamanio_payload = 0;
    paquete->payload = NULL;
    return paquete;
}

void agregar_a_paquete(t_paquete* paquete, void* valor, uint32_t tamanio)
{
    paquete->payload = realloc(paquete->payload, paquete->tamanio_payload + tamanio + sizeof(uint32_t));
    memcpy(paquete->payload + paquete->tamanio_payload, &tamanio, sizeof(uint32_t));
    memcpy(paquete->payload + paquete->tamanio_payload + sizeof(uint32_t), valor, tamanio);
    paquete->tamanio_payload += tamanio + sizeof(uint32_t);
}

void agregar_string_a_paquete(t_paquete* paquete, char* string)
{
    agregar_a_paquete(paquete, string, (uint32_t)(strlen(string) + 1));
}

void enviar_paquete(t_paquete* paquete, int socket_cliente)
{
    int tamanio_total = sizeof(op_code) + sizeof(uint32_t) + paquete->tamanio_payload;
    void* buffer_envio = malloc(tamanio_total);
    int desplazamiento = 0;

    memcpy(buffer_envio + desplazamiento, &(paquete->codigo_operacion), sizeof(op_code));
    desplazamiento += sizeof(op_code);

    memcpy(buffer_envio + desplazamiento, &(paquete->tamanio_payload), sizeof(uint32_t));
    desplazamiento += sizeof(uint32_t);

    if (paquete->tamanio_payload > 0)
        memcpy(buffer_envio + desplazamiento, paquete->payload, paquete->tamanio_payload);

    send(socket_cliente, buffer_envio, tamanio_total, 0);

    free(buffer_envio);
}

t_paquete* recibir_paquete(int socket_cliente)
{
    t_paquete* paquete = malloc(sizeof(t_paquete));

    if (recv(socket_cliente, &(paquete->codigo_operacion), sizeof(op_code), MSG_WAITALL) <= 0)
    {
        free(paquete);
        return NULL; // Desconexión
    }

    if (recv(socket_cliente, &(paquete->tamanio_payload), sizeof(uint32_t), MSG_WAITALL) <= 0)
    {
        free(paquete);
        return NULL; // Error o corte a mitad de paquete
    }

    if (paquete->tamanio_payload > 0)
    {
        paquete->payload = malloc(paquete->tamanio_payload);
        if (recv(socket_cliente, paquete->payload, paquete->tamanio_payload, MSG_WAITALL) <= 0)
        {
            free(paquete->payload);
            free(paquete);
            return NULL; // Falló al recibir el cuerpo
        }
    }
    else
    {
        paquete->payload = NULL;
    }

    return paquete;
}

void eliminar_paquete(t_paquete* paquete)
{
    if (paquete != NULL)
    {
        if (paquete->payload != NULL)
            free(paquete->payload);
        free(paquete);
    }
}

void enviar_mensaje(char* mensaje, int socket_cliente)
{
    t_paquete* paquete = crear_paquete(MENSAJE);
    agregar_string_a_paquete(paquete, mensaje);
    enviar_paquete(paquete, socket_cliente);
    eliminar_paquete(paquete);
}

char* recibir_mensaje(int socket_cliente)
{
    t_paquete* paquete = recibir_paquete(socket_cliente);
    if (paquete == NULL) return NULL;

    // En el payload viene: [4 bytes de tamaño][string con \0]
    char* mensaje = strdup((char*)(paquete->payload + sizeof(uint32_t)));
    eliminar_paquete(paquete);
    return mensaje;
}*/