#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <arpa/inet.h>
#include "dict.h"

#define BUFFSIZE 1024	// Tamanho do buffer

int fd, fd1;
struct sockaddr_in addr, addrP2P, addrReceive;
struct ip_mreq group;
char buffer[BUFFSIZE];
pthread_t receive, receiveGrp;
socklen_t slen = sizeof(addr);
char whoIam[512];

void erro(char *msg);
void *receiveMsg(void *arguments);
void userInteration();
void getMyName(char *aux);
void handleP2P(char *input, node *dict);
//void handleGrupo(char *input);

int main(int argc, char *argv[]) {
	if (argc != 3) {
		printf("cliente <server host> <port>\n");
		exit(-1);
	}
	
	int porto = (int) strtol(argv[2], (char **) NULL, 10);
	
	struct hostent *hostPtr;
	char endServer[100];

	strcpy(endServer, argv[1]);
	if ((hostPtr = gethostbyname(endServer)) == 0)
		erro("Não consegui obter endereço");

	bzero((void *) &addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
	addr.sin_port = htons(porto);

	if ((fd = socket(AF_INET,SOCK_DGRAM,0)) == -1)
		erro("socket");

	if ((fd1 = socket(AF_INET,SOCK_DGRAM,0)) == -1)
		erro("socket");
		
	printf("----------------------COMANDOS--------------------\n\n");
	printf("AUTH - AUTENTICAR NO SERVER\n");
	printf("SEND - MANDAR MENSAGEM PARA UM UTILIZADOR ATRAVES DO SERVER\n");
	printf("SP2P - MANDAR MENSAGEM PARA UTILIZADOR ATRAVES DE P2P\n");
	printf("QUIT - DESCONECTAR DO SERVIDOR E TERMINAR PROGRAMA\n\n\n");
			
	userInteration();
	
	close(fd);
	return 0;
}

void userInteration() {
	char *string, *found, aux[BUFFSIZE];
	node *dict;
	dict = malloc(sizeof(node)*100);
	while(1) {
		printf("Insira dados de autenticacao (AUTH <username> <password>):\n");
		fgets(buffer, sizeof(buffer), stdin);
		string = strdup(buffer);
		found = strsep(&string, " ");
		if (strcmp(found, "AUTH")==0)
			break;
	}
	buffer[strcspn(buffer, "\n")] = 0;
	sendto(fd, buffer, strlen(buffer)+1, 0, (struct sockaddr *) &addr, slen);
	strncpy(aux, buffer, strlen(buffer)+1);
	recvfrom(fd, buffer, BUFFSIZE, 0, (struct sockaddr *) &addr, (socklen_t *) &slen);
	if (buffer[0]=='A') {
		getMyName(aux);
		printf("%s\n", buffer);
		pthread_create(&receive, NULL, receiveMsg, NULL);
	
		while(1) {
			//send messsage to server
			fgets(buffer, sizeof(buffer), stdin);
			
			buffer[strcspn(buffer, "\n")] = 0;
			string = strdup(buffer);
			found = strsep(&string, " ");

			if(strcmp(found, "QUIT")==0) {
				sendto(fd, buffer, strlen(buffer)+1, 0, (struct sockaddr *) &addr, slen);
				pthread_cancel(receive);
				break;
			} else if (strcmp(found, "SP2P")==0) {
				handleP2P(buffer, dict);
			} /* else if (strcmp(found, "SGRUPO")==0) {
				handleGrupo(buffer);
			} else if (strcmp(found, "IPGRUPO")==0) {
				sendto(fd, buffer, strlen(buffer)+1, 0, (struct sockaddr *) &addr, slen);
				usleep(100000);
				char *aux = buffer;

			} */ else 
				sendto(fd, buffer, strlen(buffer)+1, 0, (struct sockaddr *) &addr, slen);
		}
		pthread_join(receive, NULL);
	} else
		printf("%s\n", buffer);
	
	free(dict);
}

void *receiveMsg(void *arguments) {
	while(1) {
		recvfrom(fd, buffer, BUFFSIZE, 0, (struct sockaddr *) &addrReceive, (socklen_t *) &slen);
		printf("%s\n", buffer);
	}
}

void handleP2P(char *input, node *dict) {
	int temp, count;
	char *found, *string, info[3][500], aux1[500], ipPort[2][50];
	string = strdup(input);
	count=0;
	while((found = strsep(&string, " ")) != NULL) {	//criar substrings
		strcpy(info[count], found);
		count++;
	}
	procurar_ip_port(dict, info[1], aux1, sizeof(aux1));
	
	if (aux1[0] == '\0') {
		snprintf(buffer, BUFFSIZE, "REQUEST %s", info[1]);
		sendto(fd, buffer, strlen(buffer)+1, 0, (struct sockaddr *) &addr, slen);
		usleep(100000);
		string = strdup(buffer);
		
		found = strsep(&string, " ");
		if (strcmp(found, "REQUESTED")==0) {
			count=0;
			while((found = strsep(&string, ":")) != NULL) {	//criar substrings
				strcpy(ipPort[count], found);
				count++;
			}
			temp = (int) strtol(ipPort[1], (char **) NULL, 10);
			printf("%d\n", temp);
			adicionar_dict(dict, info[1], ipPort[0], temp);
			addrP2P.sin_addr.s_addr = inet_addr(ipPort[0]);
			addrP2P.sin_port=htons(temp);
			snprintf(buffer, BUFFSIZE, "%s SENT %s", whoIam, info[2]);
			sendto(fd, buffer, strlen(buffer)+1, 0, (struct sockaddr *) &addrP2P, slen);
		} else
			printf("REQUEST perdido, por favor envie outra vez a mensagem\n");
	} else {
		string = strdup(aux1);
		count=0;
		while((found = strsep(&string, ":")) != NULL) {	//criar substrings
			strcpy(ipPort[count], found);
			count++;
		}
		temp = (int) strtol(ipPort[1], (char **) NULL, 10);
		addrP2P.sin_addr.s_addr = inet_addr(ipPort[0]);
		addrP2P.sin_port=htons(temp);
		snprintf(buffer, BUFFSIZE, "%s SENT %s", whoIam, info[2]);
		sendto(fd, buffer, strlen(buffer)+1, 0, (struct sockaddr *) &addrP2P, slen);
	}
	free(string);
}

/*
void handleGrupo(char *input) {
	
}
*/

void erro(char *msg) {
	printf("Erro: %s\n", msg);
	exit(-1);
}

void getMyName(char *aux) {
	int count=0;
	char *found, *string, info[3][512];
	string = strdup(aux);
	while((found = strsep(&string, " ")) != NULL) {	//criar substrings
		strncpy(info[count], found, strlen(found)+1);
		count++;
	}
	strncpy(whoIam, info[1], strlen(info[1])+1);
	free(string);
}
