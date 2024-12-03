#include <iostream>
#include <cstring>
#include <thread>
#include <unistd.h>
#include <map>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <condition_variable>
#include <unordered_map>
#include <mutex>
#include <chrono>

#define PORT 8000
#define BUFFERSIZE 1024
#define LIMITE_MENSAJES 5

class Servidor {
private:
    int sockServidor;
    struct sockaddr_in confServidor;
    std::map<int, int> contadores; // Map of message counters per client
    std::unordered_map<int, std::string> nombresClientes; // Map to store the names of the clients

    std::mutex bloqueoMutex;  // Mutex to synchronize access
    std::unordered_map<int, bool> bloqueados; // Map of blocked clients
    std::unordered_map<int, std::chrono::time_point<std::chrono::steady_clock>> bloqueosTiempo; // Map of block timestamps
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

            // Check if the client is blocked
            {
                std::unique_lock<std::mutex> lock(bloqueoMutex);
                if (bloqueados[sockCliente]) {
                    // Calculate remaining block time
                    auto now = std::chrono::steady_clock::now();
                    auto tiempoBloqueo = std::chrono::duration_cast<std::chrono::seconds>(now - bloqueosTiempo[sockCliente]);
                    int segundosRestantes = 60 - tiempoBloqueo.count();

                    if (segundosRestantes > 0) {
                        std::string mensajeBloqueo = "Estás bloqueado. Tiempo restante: " + std::to_string(segundosRestantes) + " segundos.\n";
                        send(sockCliente, mensajeBloqueo.c_str(), mensajeBloqueo.size(), 0);
                        continue;
                    } else {
                        // Unblock the client if the block duration has elapsed
                        bloqueados[sockCliente] = false;
                        contadores[sockCliente] = 0;  // Reset message counter
                    }
                }
            }

            if (primerMensaje) {
                nombreCliente = buffer;
                nombresClientes[sockCliente] = nombreCliente; // Store the client's name
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
                    bloqueosTiempo[sockCliente] = std::chrono::steady_clock::now();  // Store block start time
                }
                std::string mensajeBloqueo = "Has alcanzado el límite de mensajes. Estás temporalmente bloqueado por 60 segundos.\n";
                send(sockCliente, mensajeBloqueo.c_str(), mensajeBloqueo.size(), 0);
                continue;  // Do not log or process the message
            }

            // Log and acknowledge the message with the client's name
            std::cout << "Mensaje recibido de " << nombreCliente << ": " << buffer << std::endl;

            std::string acknowledgement = "Mensaje recibido correctamente. Mensajes enviados por ti: " + std::to_string(contadores[sockCliente]) + "\n";
            send(sockCliente, acknowledgement.c_str(), acknowledgement.size(), 0);
        }

        {
            std::lock_guard<std::mutex> lock(bloqueoMutex);
            bloqueados.erase(sockCliente);
            contadores.erase(sockCliente);
            nombresClientes.erase(sockCliente);  // Clean up the client's name
            bloqueosTiempo.erase(sockCliente);  // Clean up block timestamp
        }
        close(sockCliente);
    }

    void desbloquearClientes() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Check every second for more precise timing
        std::lock_guard<std::mutex> lock(bloqueoMutex);

        auto now = std::chrono::steady_clock::now();
        for (auto it = bloqueados.begin(); it != bloqueados.end(); /* no increment here */) {
            int cliente = it->first;

            if (it->second) { // Check if client is currently blocked
                auto tiempoBloqueo = std::chrono::duration_cast<std::chrono::seconds>(now - bloqueosTiempo[cliente]);

                if (tiempoBloqueo.count() >= 60) {
                    std::cout << "Desbloqueando cliente " << cliente << "\n";
                    it = bloqueados.erase(it); // Remove client from blocked list
                    contadores[cliente] = 0;  // Reset message counter
                    bloqueosTiempo.erase(cliente); // Clean up block timestamp
                } else {
                    ++it; // Move to the next client
                }
            } else {
                ++it; // Move to the next client if not blocked
            }
        }
    }
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

    int nClientes = 1 + std::stoi(argv[1]);
    if (nClientes < 2) {
        std::cerr << "Número de clientes inválido.\n";
        return 1;
    }

    Servidor servidor(nClientes);
    return 0;
}