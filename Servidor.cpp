#include <iostream>
#include <cstring>
#include <thread>
#include <unistd.h>
#include <map>
#include <sstream>  // Para std::istringstream
#include <algorithm>  // Para std::reverse
#include <arpa/inet.h>
#include <netinet/in.h>

#define PORT 8000
#define BUFFERSIZE 1024
#define LIMITE_MENSAJES 5 

class Servidor {
private:
    int sockServidor;
    struct sockaddr_in confServidor;
    std::map<int, int> contadores; // Mapa de contadores por cliente

    void configurarServidor() {
        confServidor.sin_family = AF_INET;
        confServidor.sin_addr.s_addr = INADDR_ANY;
        confServidor.sin_port = htons(PORT);

        if (bind(sockServidor, (struct sockaddr*)&confServidor, sizeof(confServidor)) < 0) {
            std::cerr << "Error de enlace (bind)\n";
            exit(EXIT_FAILURE);
        }
    }

    void escucharClientes(int nClientes) {
        if (listen(sockServidor, nClientes) < 0) {
            std::cerr << "Error al escuchar (listen)\n";
            exit(EXIT_FAILURE);
        }
    }

    void manejarCliente(int sockCliente) {
        char buffer[BUFFERSIZE] = {0};
        bool primerMensaje = true;
        std::string nombreCliente;

        // Inicializa el contador para este cliente
        contadores[sockCliente] = 0;

        while (true) {
            memset(buffer, 0, BUFFERSIZE);
            int valread = read(sockCliente, buffer, BUFFERSIZE - 1);

            if (valread <= 0) {
                std::cout << "Cliente desconectado.\n";
                break;
            }

            buffer[valread] = '\0';  // Asegura terminación
            std::cout << "Mensaje recibido: " << buffer << std::endl;

            if (primerMensaje) {
                nombreCliente = buffer;
                std::string mensajeBienvenida = "Hola " + nombreCliente + "\n";
                send(sockCliente, mensajeBienvenida.c_str(), mensajeBienvenida.size(), 0);
                primerMensaje = false;
            } else if (strcmp(buffer, "BYE\n") == 0) {
                std::string despedida = "Adiós " + nombreCliente + "\n";
                send(sockCliente, despedida.c_str(), despedida.size(), 0);
                break;
            } else {

                    if (contadores[sockCliente] >= LIMITE_MENSAJES) {
                    std::string mensajeBloqueo = "Has alcanzado el límite de mensajes. No puedes enviar más.\n";
                    send(sockCliente, mensajeBloqueo.c_str(), mensajeBloqueo.size(), 0);
                    close(sockCliente);
                    break;  // Salir del bucle, ya no se procesan más mensajes
                }

                // Incrementar el contador de mensajes del cliente
                contadores[sockCliente]++;

                // Invertir palabras recibidas
                std::string mensajeInvertido = invertirPalabras(buffer);

                // Adjuntar el contador al mensaje
                mensajeInvertido += "\nMensajes enviados por ti: " + std::to_string(contadores[sockCliente]) + "\n";

                send(sockCliente, mensajeInvertido.c_str(), mensajeInvertido.size(), 0);
            }
        }

        // Elimina el contador del cliente al desconectarse
        contadores.erase(sockCliente);
        close(sockCliente);
    }

    std::string invertirPalabras(const std::string& mensaje) {
        std::string resultado, palabra;
        std::istringstream stream(mensaje);

        while (stream >> palabra) {
            std::reverse(palabra.begin(), palabra.end());
            resultado += palabra + " ";
        }

        if (!resultado.empty())
            resultado.pop_back();  // Elimina el último espacio sobrante

        return resultado;
    }

public:
    Servidor(int nClientes) {
        if ((sockServidor = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            std::cerr << "Error al crear el socket\n";
            exit(EXIT_FAILURE);
        }

        configurarServidor();
        escucharClientes(nClientes);

        std::cout << "Servidor iniciado y esperando clientes...\n";

        struct sockaddr_in confCliente;
        socklen_t tamannoConf = sizeof(confCliente);

        for (int i = 0; i < nClientes; ++i) {
            int sockCliente = accept(sockServidor, (struct sockaddr*)&confCliente, &tamannoConf);

            if (sockCliente < 0) {
                std::cerr << "Error aceptando cliente\n";
                continue;
            }

            std::cout << "Cliente conectado desde: " << inet_ntoa(confCliente.sin_addr) << std::endl;

            // Maneja cada cliente en un hilo separado
            std::thread(&Servidor::manejarCliente, this, sockCliente).detach();
        }
    }

    ~Servidor() {
        close(sockServidor);
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Uso: " << argv[0] << " <número de clientes>\n";
        return 1;
    }

    int nClientes = std::stoi(argv[1]);
    if (nClientes < 1) {
        std::cerr << "Número de clientes inválido.\n";
        return 1;
    }

    Servidor servidor(nClientes);
    return 0;
}