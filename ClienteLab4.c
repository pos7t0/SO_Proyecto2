#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

////
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define PORT 8000
#define BUFFERSIZE 1024

void crearSocket(int *sock){
    if ((*sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {        
        printf("Error Creación de Socket\n");
        exit(0);
    }
}

void configurarCliente(int sock, struct sockaddr_in *conf){
	conf->sin_family = AF_INET;
	conf->sin_port = htons(PORT);
	conf->sin_addr.s_addr = inet_addr("127.0.0.1");

	if (connect(sock, (struct sockaddr *)conf, sizeof(*conf)) < 0){
		printf("\nConnection Failed \n");
		exit(0);
	}
}

int main(int argc, char const *argv[]){
    if (argc < 2)
        return 0;
    
    char nombreClientes[100];
    strcpy(nombreClientes, argv[1]);

	///1. Crear Scoket
    int sockCliente;
    crearSocket(&sockCliente);

    ///2. Conectarse al Servior
    struct sockaddr_in confServidor;
    configurarCliente(sockCliente, &confServidor);
   
    int primerMensaje = 1; 
    ///3. Comunicarse
	while(1){
		char buffer[BUFFERSIZE]={0}, buffer2[BUFFERSIZE]={0};
			
		if (primerMensaje){
    	   send(sockCliente, nombreClientes, strlen(nombreClientes), 0);
		   primerMensaje = 0;

    	   int valread = read(sockCliente, buffer, BUFFERSIZE);
		   fputs(buffer, stdout);
		}else{
 			  printf("Mensaje: ");
			  fgets(buffer, BUFFERSIZE, stdin);

			  if (strcmp(buffer, "BYE\n")==0){
				  send(sockCliente, buffer, strlen(buffer), 0);
				  int valread = read(sockCliente, buffer2, BUFFERSIZE);
				  strcat(buffer2, nombreClientes);
   			      fputs(buffer, stdout);
				  break;

			  }else{
				  send(sockCliente, buffer, strlen(buffer), 0);
				  int valread = read(sockCliente, buffer, BUFFERSIZE);				 
				  printf("%s",buffer);
			  }
		}
	}

	close(sockCliente);

	return 0;
}