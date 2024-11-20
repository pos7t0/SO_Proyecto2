#include <iostream>
#include <cstring>
#include <string>
#include <unistd.h>
#include <cstdlib>
#include <arpa/inet.h>
#include <netinet/in.h>

#define PORT 8000
#define BUFFERSIZE 1024

class Cliente {
private:
    int sockCliente;
    struct sockaddr_in confServidor;
    std::string nombreCliente;

    void crearSocket() {
        if ((sockCliente = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            std::cerr << "Error al crear el socket\n";
            exit(EXIT_FAILURE);
        }
    }

    void configurarCliente() {
        confServidor.sin_family = AF_INET;
        confServidor.sin_port = htons(PORT);
        confServidor.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(sockCliente, (struct sockaddr*)&confServidor, sizeof(confServidor)) < 0) {
            std::cerr << "Error al conectar con el servidor\n";
            exit(EXIT_FAILURE);
        }
    }

public:
    Cliente(const std::string& nombre) : nombreCliente(nombre) {
        crearSocket();
        configurarCliente();
    }

    ~Cliente() {
        close(sockCliente);
    }

    void iniciarComunicacion() {
        bool primerMensaje = true;
        char buffer[BUFFERSIZE] = {0};
        char bufferRespuesta[BUFFERSIZE] = {0};

        while (true) {
            if (primerMensaje) {
                // Enviar el nombre del cliente al servidor
                send(sockCliente, nombreCliente.c_str(), nombreCliente.size(), 0);
                primerMensaje = false;

                // Leer la respuesta del servidor
                //int valread = read(sockCliente, buffer, BUFFERSIZE);
               memset(buffer, 0, BUFFERSIZE); // Limpiar el buffer 
               read(sockCliente, buffer, BUFFERSIZE);
                std::cout << buffer;
            } else {
                std::cout << "\nMensaje: ";
                std::string mensaje;
                std::getline(std::cin, mensaje);

                if (mensaje == "BYE\n") {
                    // Enviar mensaje de despedida
                    send(sockCliente, mensaje.c_str(), mensaje.size(), 0);

                    // Leer la respuesta del servidor
                    memset(buffer, 0, BUFFERSIZE); // Limpiar el buffer
                    /*int valread =*/ read(sockCliente, bufferRespuesta, BUFFERSIZE);
                    std::cout << bufferRespuesta << nombreCliente << std::endl;

                    break;
                } else {
                    // Enviar el mensaje al servidor
                    send(sockCliente, mensaje.c_str(), mensaje.size(), 0);
                    
                    // Leer la respuesta del servidor
                    memset(buffer, 0, BUFFERSIZE); // Limpiar el buffer antes de usarlo
                    int valread = read(sockCliente, buffer, BUFFERSIZE);
                    
                    if (valread <= 0) {
                        std::cout << "Conexión cerrada por el servidor.\n";
                        break;  // Salir del bucle si el servidor cerró la conexión
                    }

                    // Procesar solo los datos válidos leídos
                    std::string respuestaServidor(buffer, valread);
                    
                    // Mostrar la respuesta
                    std::cout << respuestaServidor;

                    // Verificar si el servidor indicó que se alcanzó el límite
                    if (respuestaServidor.find("Has alcanzado el límite de mensajes") != std::string::npos) {
                        std::cout << "No puedes enviar más mensajes. Intenta más tarde.\n";
                        break;  // Salir del bucle de comunicación
                    }
                }
            }
        }
    }
};

int main(int argc, char const* argv[]) {
    if (argc < 2) {
        std::cerr << "Uso: " << argv[0] << " <Nombre del Cliente>\n";
        return 0;
    }

    std::string nombreCliente = argv[1];

    // Crear el cliente e iniciar la comunicación
    Cliente cliente(nombreCliente);
    cliente.iniciarComunicacion();

    return 0;
}