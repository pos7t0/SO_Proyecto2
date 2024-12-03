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
    std::map<int, int> contadores; // Mapa de contadores de mensajes por cliente
    std::unordered_map<int, std::string> nombresClientes; // Mapa para almacenar los nombres de los clientes

    std::mutex bloqueoMutex;  // Mutex para sincronizar el acceso
    std::unordered_map<int, bool> bloqueados; // Mapa de clientes bloqueados
    std::unordered_map<int, std::chrono::time_point<std::chrono::steady_clock>> bloqueosTiempo; // Mapa de marcas de tiempo de bloqueo
    std::condition_variable bloqueoCV;  // Variable de condición para los clientes bloqueados

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

        contadores[sockCliente] = 0;  // Inicializar contador de mensajes
        bloqueados[sockCliente] = false;  // Inicializar como no bloqueado

        while (true) {
            memset(buffer, 0, BUFFERSIZE);
            int valread = read(sockCliente, buffer, BUFFERSIZE - 1);

            if (valread <= 0) {
                std::cout << "Cliente desconectado.\n";
                break;
            }

            buffer[valread] = '\0';  // Asegurar una cadena terminada en null

            // Verificar si el cliente está bloqueado
            {
                std::unique_lock<std::mutex> lock(bloqueoMutex);
                if (bloqueados[sockCliente]) {
                    // Calcular el tiempo restante de bloqueo
                    auto now = std::chrono::steady_clock::now();
                    auto tiempoBloqueo = std::chrono::duration_cast<std::chrono::seconds>(now - bloqueosTiempo[sockCliente]);
                    int segundosRestantes = 60 - tiempoBloqueo.count();

                    if (segundosRestantes > 0) {
                        std::string mensajeBloqueo = "Estás bloqueado. Tiempo restante: " + std::to_string(segundosRestantes) + " segundos.\n";
                        send(sockCliente, mensajeBloqueo.c_str(), mensajeBloqueo.size(), 0);
                        continue;
                    } else {
                        // Desbloquear al cliente si ha pasado el tiempo de bloqueo
                        bloqueados[sockCliente] = false;
                        contadores[sockCliente] = 0;  // Reiniciar el contador de mensajes
                    }
                }
            }

            if (primerMensaje) {
                nombreCliente = buffer;
                nombresClientes[sockCliente] = nombreCliente; // Almacenar el nombre del cliente
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

            // Bloquear al cliente si el contador de mensajes excede el límite
            if (contadores[sockCliente] > LIMITE_MENSAJES) {
                {
                    std::lock_guard<std::mutex> lock(bloqueoMutex);
                    bloqueados[sockCliente] = true;
                    bloqueosTiempo[sockCliente] = std::chrono::steady_clock::now();  // Almacenar la hora de inicio del bloqueo
                }
                std::string mensajeBloqueo = "Has alcanzado el límite de mensajes. Estás temporalmente bloqueado por 60 segundos.\n";
                send(sockCliente, mensajeBloqueo.c_str(), mensajeBloqueo.size(), 0);
                continue;  // No registrar ni procesar el mensaje
            }

            // Registrar y confirmar el mensaje con el nombre del cliente
            std::cout << "Mensaje recibido de " << nombreCliente << ": " << buffer << std::endl;

            std::string acknowledgement = "Mensaje recibido correctamente. Mensajes enviados por ti: " + std::to_string(contadores[sockCliente]) + "\n";
            send(sockCliente, acknowledgement.c_str(), acknowledgement.size(), 0);
        }

        {
            std::lock_guard<std::mutex> lock(bloqueoMutex);
            bloqueados.erase(sockCliente);
            contadores.erase(sockCliente);
            nombresClientes.erase(sockCliente);  // Limpiar el nombre del cliente
            bloqueosTiempo.erase(sockCliente);  // Limpiar la marca de tiempo de bloqueo
        }
        close(sockCliente);
    }

    void desbloquearClientes() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1)); // Verificar cada segundo para una temporización más precisa
            std::lock_guard<std::mutex> lock(bloqueoMutex);

            auto now = std::chrono::steady_clock::now();
            for (auto it = bloqueados.begin(); it != bloqueados.end(); /* no incrementar aquí */) {
                int cliente = it->first;

                if (it->second) { // Verificar si el cliente está actualmente bloqueado
                    auto tiempoBloqueo = std::chrono::duration_cast<std::chrono::seconds>(now - bloqueosTiempo[cliente]);

                    if (tiempoBloqueo.count() >= 60) {
                        std::cout << "Desbloqueando cliente " << cliente << "\n";
                        it = bloqueados.erase(it); // Eliminar cliente de la lista de bloqueados
                        contadores[cliente] = 0;  // Reiniciar el contador de mensajes
                        bloqueosTiempo.erase(cliente); // Limpiar la marca de tiempo de bloqueo
                    } else {
                        ++it; // Pasar al siguiente cliente
                    }
                } else {
                    ++it; // Pasar al siguiente cliente si no está bloqueado
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