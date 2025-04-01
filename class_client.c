#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <errno.h>
//CLIENTE 1

#define BUF_SIZE 1024
pthread_t array[100];
int count = 0;
int sock;

void erro(char *msg);

void *multicast_receber(void *arg) {
    char *multicast = arg;
   
    int sock;
    struct sockaddr_in addr; 
    char msg[BUF_SIZE]; 
    socklen_t addrlen;
    struct ip_mreq mreq;
    int reuse=1;

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(sock);
        exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(multicast);
    addr.sin_port = htons(5000);
    

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    mreq.imr_multiaddr.s_addr = inet_addr(multicast);
    mreq.imr_interface.s_addr = INADDR_ANY;
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("setsockopt");
        exit(1);
    }
    
    addrlen = sizeof(addr);
    while (1) {
        int nbytes = recvfrom(sock, msg, sizeof(msg) - 1, 0, (struct sockaddr *)&addr, &addrlen);
        if (nbytes < 0) {
            perror("recvfrom");
            close(sock);
            exit(1);
        }
        msg[nbytes] = '\0';
        printf("Mensagem recebida: %s\n", msg);
        fflush(stdout);
    }
        
    if (setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("setsockopt");
        exit(1);
    }
    close(sock);
    return NULL;
}
void inicializar_turmas(char *buffer){
    char *token = strtok(buffer, " ");
	while ((token = strtok(NULL, " ")) != NULL) {
        char *slash_pos = strchr(token, '/');
        if (slash_pos != NULL) {
            *slash_pos = '\0';
            
            char *ip = slash_pos + 1;

            char *ip_copy = malloc(100);
            if (ip_copy == NULL) {
                perror("malloc");
                exit(EXIT_FAILURE);
            }
            strncpy(ip_copy, ip, 100);
            ip_copy[strcspn(ip_copy, "\n")] = '\0';
			char *arg = ip_copy;
            if (pthread_create(&array[count], NULL, multicast_receber, (void *)arg) != 0) {
                perror("pthread_create");
                free(ip_copy);
                continue;
            }

            count++;
        }
    }   
}
int main(int argc, char *argv[])
{
  int fd;
  struct sockaddr_in addr;
  struct hostent *hostPtr;
  char buffer[BUF_SIZE];
  
  if (argc != 3)
  {
    erro("Argumentos inválidos");
    exit(-1);
  }
  
  char endServer[100];
  char port[10];
  strcpy(endServer, argv[1]);
  strcpy(port,argv[2]);
  
  if ((hostPtr = gethostbyname(endServer)) == 0)
    erro("Não consegui obter endereço");

  bzero((void *)&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
  addr.sin_port = htons((short)atoi(port));

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    erro("socket");
  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    erro("Connect");
  printf("Faça login no servidor!\n");
  while (1) {
    fgets(buffer, sizeof(buffer), stdin);
    if (strlen(buffer) > 0) {
        buffer[strlen(buffer) - 1] = '\0';
    } else {
        erro("Login inválido");
    }

    write(fd, buffer, strlen(buffer));
    
    int nread = read(fd, buffer, BUF_SIZE - 1);
    buffer[nread] = '\0';
    printf("%s", buffer);
    if (strcmp(buffer, "OK\n") == 0) {
    	write(fd, "CREATE_CLASS xfjsjic 1", strlen("CREATE_CLASS xfjsjic 1"));
    	memset(buffer, 0 ,BUF_SIZE);
    	int nread = read(fd, buffer, BUF_SIZE - 1);
    	if(strcmp(buffer, "PROF") == 0){
    		break;
    	}
    	write(fd, "LIST_SUBSCRIBED", strlen("LIST_SUBSCRIBED"));
    	nread = read(fd, buffer, BUF_SIZE - 1);
    	buffer[nread] = '\0';   	
        printf("Login efetuado com sucesso!\n");
        char *buffer2 = buffer;
        inicializar_turmas(buffer2);
        break;
    } else if (strcmp(buffer, "REJECTED\n") == 0) {
        printf("Nome de utilizador ou password incorretos. Tente novamente\n");
    }
  }
  printf("Pode efetuar outros comandos!\n");
 
  while (1) {
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strcspn(buffer, "\n")] = '\0';

    if (strcmp(buffer, "SAIR") == 0) {
        break;
    }
    write(fd, buffer, strlen(buffer));
    if(strncmp(buffer ,"SEND " , strlen("SEND ")) != 0){
      int nread = read(fd, buffer, BUF_SIZE - 1);
      if (nread <= 0) {
        erro("Erro ao ler do servidor");
      }
      buffer[nread] = '\0';
      printf("%s", buffer);
    }
    if(strncmp(buffer,"ACCEPTED", strlen("ACCEPTED")) == 0){  
    	array[count] = count;
        strtok(buffer, " ");
        char *multicast = strtok(NULL, " ");
        multicast[strcspn(multicast, "\n")]='\0';
        char *arg = multicast;
        if (pthread_create(&array[count], NULL, multicast_receber, (void *)arg) != 0) {
            erro("Erro ao criar a thread");
        }
        count++; 
    }
    // Cleanup and exit...
  }
  close(fd);
  exit(0);
}


void erro(char *msg)
{
  printf("Erro: %s\n", msg);
  exit(-1);
}
