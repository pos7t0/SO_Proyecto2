#include <iostream>
#include <cstring>
#include <thread>
#include <unistd.h>
#include <map>
#include <sstream>  // Para std::istringstream
#include <algorithm>  // Para std::reverse
#include <arpa/inet.h>
#include <netinet/in.h>
#include <condition_variable>
#include <unordered_map>
#include <mutex>

#define PORT 8000
#define BUFFERSIZE 1024
#define LIMITE_MENSAJES 5 

class Servidor {
private:
    int sockServidor;
    struct sockaddr_in confServidor;
    std::map<int, int> contadores; // Mapa de contadores por cliente
    
    std::mutex bloqueoMutex;  // Mutex to synchronize access
    std::unordered_map<int, bool> bloqueados; // Map of blocked clients
    std::condition_variable bloqueoCV;  // Condition variable for blocked clients

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

        contadores[sockCliente] = 0;  // Initialize message counter
        bloqueados[sockCliente] = false;  // Initialize as not blocked

        while (true) {
            memset(buffer, 0, BUFFERSIZE);
            int valread = read(sockCliente, buffer, BUFFERSIZE - 1);

            if (valread <= 0) {
                std::cout << "Cliente desconectado.\n";
                break;
            }

            buffer[valread] = '\0';  // Ensure null-terminated string

            // Check if the client is blocked before processing or logging the message
            {
                std::unique_lock<std::mutex> lock(bloqueoMutex);
                if (bloqueados[sockCliente]) {
                    std::string mensajeBloqueo = "Has alcanzado el límite de mensajes. Espera un momento antes de enviar más.\n";
                    send(sockCliente, mensajeBloqueo.c_str(), mensajeBloqueo.size(), 0);
                    continue;
                }
            }

            if (primerMensaje) {
                nombreCliente = buffer;
                std::string mensajeBienvenida = "Hola " + nombreCliente + "\n";
                send(sockCliente, mensajeBienvenida.c_str(), mensajeBienvenida.size(), 0);
                primerMensaje = false;
                continue;
            }

            if (strcmp(buffer, "BYE\n") == 0) {
                std::string despedida = "Adiós " + nombreCliente + "\n";
                send(sockCliente, despedida.c_str(), despedida.size(), 0);
                break;
            }

            contadores[sockCliente]++;

            // Block the client if the message count exceeds the limit
            if (contadores[sockCliente] > LIMITE_MENSAJES) {
                {
                    std::lock_guard<std::mutex> lock(bloqueoMutex);
                    bloqueados[sockCliente] = true;
                }
                std::string mensajeBloqueo = "Has alcanzado el límite de mensajes. Estás temporalmente bloqueado.\n";
                send(sockCliente, mensajeBloqueo.c_str(), mensajeBloqueo.size(), 0);
                continue;  // Do not log or process the message
            }

            // Log and process the message only if the client is not blocked
            std::cout << "Mensaje recibido: " << buffer << std::endl;

            std::string mensajeInvertido = invertirPalabras(buffer);
            mensajeInvertido += "\nMensajes enviados por ti: " + std::to_string(contadores[sockCliente]) + "\n";
            send(sockCliente, mensajeInvertido.c_str(), mensajeInvertido.size(), 0);
        }

        {
            std::lock_guard<std::mutex> lock(bloqueoMutex);
            bloqueados.erase(sockCliente);
            contadores.erase(sockCliente);
        }
        close(sockCliente);
    }

    void desbloquearClientes() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(60));
            std::lock_guard<std::mutex> lock(bloqueoMutex);

            for (auto& [cliente, estado] : bloqueados) {
                if (estado) {
                    std::cout << "Desbloqueando cliente " << cliente << "\n";
                    estado = false;
                    contadores[cliente] = 0;  // Reset message counter
                }
            }

            bloqueoCV.notify_all();  // Notify any threads waiting on the condition variable
        }
    }

    std::string invertirPalabras(const std::string& mensaje) {
        std::string resultado, palabra;
        std::istringstream stream(mensaje);

        while (stream >> palabra) {
            std::reverse(palabra.begin(), palabra.end());
            resultado += palabra + " ";
        }

        if (!resultado.empty())
            resultado.pop_back();  // Remove the last space

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

        std::thread(&Servidor::desbloquearClientes, this).detach();

        struct sockaddr_in confCliente;
        socklen_t tamannoConf = sizeof(confCliente);

        for (int i = 0; i < nClientes; ++i) {
            int sockCliente = accept(sockServidor, (struct sockaddr*)&confCliente, &tamannoConf);

            if (sockCliente < 0) {
                std::cerr << "Error aceptando cliente\n";
                continue;
            }

            std::cout << "Cliente conectado desde: " << inet_ntoa(confCliente.sin_addr) << std::endl;

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