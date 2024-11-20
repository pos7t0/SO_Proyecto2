# Nombre de los ejecutables
CLIENTE = cliente
SERVIDOR = servidor

# Compilador y flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17

# Archivos fuente
CLIENTE_SRC = Cliente.cpp
SERVIDOR_SRC = Servidor.cpp

# Compilar cliente
cliente: $(CLIENTE_SRC)
	$(CXX) $(CXXFLAGS) -o $(CLIENTE) $(CLIENTE_SRC)

# Compilar servidor
servidor: $(SERVIDOR_SRC)
	$(CXX) $(CXXFLAGS) -o $(SERVIDOR) $(SERVIDOR_SRC)