#include "cliente.h"
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <utils/comunicacion.h>
#include <utils/protocolo.h>


int socket_placa;
int socket_storage;

pthread_t hilo_escucha;

//Envía el handshake del Planificador a Placa.

static void enviar_handshake_placa(void)
{
    t_paquete* paquete = crear_paquete(CONEXION);

    t_modulo modulo = PLANIFICADOR;

    agregar_a_paquete(paquete, &modulo, sizeof(t_modulo));
    enviar_paquete(paquete, socket_placa);
    eliminar_paquete(paquete);

    log_info(logger, "Handshake de Planificador enviado a Placa");
}

//Inicializa las conexiones del Planificador con Placa y Storage.
void iniciar_conexiones(t_config* config)
{
    char* ip_placa = config_get_string_value(config, "IP_PLACA");

    char* puerto_placa = config_get_string_value(config, "PUERTO_PLACA");

    char* ip_storage = config_get_string_value(config, "IP_STORAGE");

    char* puerto_storage = config_get_string_value(config, "PUERTO_STORAGE");

 //Conexión con Placa
    socket_placa = crear_conexion(ip_placa, puerto_placa);

    if (socket_placa == -1){
        log_error(logger, "No se pudo conectar con Placa: %s:%s", ip_placa, puerto_placa);
        exit(EXIT_FAILURE);
    }

    log_info(logger, "Conectado a Placa: %s:%s", ip_placa, puerto_placa);
    
    enviar_handshake_placa();    //Handshake con Placa
    
    // Conexión con Storage
    socket_storage = crear_conexion(ip_storage, puerto_storage);

    if (socket_storage == -1){
        log_error(logger, "No se pudo conectar con Storage: %s:%s", ip_storage, puerto_storage);
        close(socket_placa);
        exit(EXIT_FAILURE);
    }

    log_info(logger, "Conectado a Storage: %s:%s", ip_storage, puerto_storage);
//No enviamos handshake a Storage todavía porque su servidor actualmente acepta la conexión y la cierra sin recibir ningún paquete.
}

pthread_t hilo_escucha;

//Hilo que escucha a Placa y Storage, y procesa los mensajes que lleguen de ellos.
void iniciar_hilo_escucha(void){

    if (pthread_create(&hilo_escucha, NULL, escuchar_placa_storage, NULL) != 0){ //chequear si el hilo se creo correctamente
        log_error(logger, "No se pudo crear el hilo de escucha");
        exit(EXIT_FAILURE);
    }

    log_info(logger,"Hilo de escucha de Placa y Storage iniciado");
}

void* escuchar_placa_storage(void* arg){//args no se usan, pero son necesarios para la firma de la función
    //hilo que escuche a la placa y al storage, y que procese los mensajes que lleguen de ellos. Por ahora, solo vamos a escuchar y loguear los mensajes que lleguen.
    //select() o poll() para escuchar a ambos sockets, y luego loguear los mensajes que lleguen. 
         while (1){                         // TODO: recibir y procesar mensajes de Placa y Storage (falta implementar)
            fd_set sockets;

           FD_ZERO(&sockets);

           FD_SET(socket_placa, &sockets);
           FD_SET(socket_storage, &sockets);

           int max_socket = socket_placa;

           if (socket_storage > max_socket){
            max_socket = socket_storage;
            }

           int resultado = select(max_socket + 1, &sockets, NULL, NULL, NULL);

          if (resultado == -1){
            if (errno == EINTR){
                continue;
            }

            log_error(logger, "Error en select()");
            break;
          }

          if (FD_ISSET(socket_placa, &sockets)){
            log_info(logger, "Hay datos disponibles desde Placa");
            recibir_paquete(socket_placa);
          }

         if (FD_ISSET(socket_storage, &sockets)){
            log_info(logger, "Hay datos disponibles desde Storage");    
            recibir_paquete(socket_storage);
         }

  }
  return NULL;
}