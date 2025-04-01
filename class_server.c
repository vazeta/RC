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

#define BUFFER_SIZE 1024
#define MAX_USERS 100
#define MAX_USER_INFO_LENGTH 100

typedef struct {
    char username[MAX_USER_INFO_LENGTH];
    char password[MAX_USER_INFO_LENGTH];
    char role[MAX_USER_INFO_LENGTH];
} UserInfo;

struct ConnectionArgs {
    int client_socket;
    char filename[50];
};

typedef struct {
	char nome[BUFFER_SIZE];
}Aluno;

typedef struct {
    char name[MAX_USER_INFO_LENGTH];
    int n_alunos;
    int size;
    char multicast_address[16];
    Aluno alunos[100];
    char prof[BUFFER_SIZE];
} ClassInfo;

typedef struct{
    UserInfo users[100];
    ClassInfo classes[100];
    int n_users;
    int n_classes;
}sh_mem;

char login_name[100];
#define KEY 12345
int shmid;
sh_mem *shared_mem;

void init(){
    if ((shmid = shmget(KEY, sizeof(sh_mem), IPC_CREAT | 0666)) == -1)
    {
        perror("Error creating shared memory 1\n");
        exit(-1);
    }
    shared_mem = (sh_mem*)shmat(shmid, NULL,0);
    shared_mem->n_users=0;
    shared_mem->n_classes=0;
}

void load_user_info(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    while (fscanf(file, "%[^;];%[^;];%[^\n]%*c", shared_mem->users[shared_mem->n_users].username, shared_mem->users[shared_mem->n_users].password, shared_mem->users[shared_mem->n_users].role) != EOF) {
        shared_mem->n_users++;
        if (shared_mem->n_users >= MAX_USERS) {
            fprintf(stderr, "Maximum number of users reached\n");
            break;
        }
    }
    fclose(file);
}


int verify_login_tcp(char buffer[]) {
    char username[BUFFER_SIZE], password[BUFFER_SIZE];
    strcpy(username, strtok(buffer + 6, " "));
    strcpy(password, strtok(NULL, " "));
    for (int i = 0; i < shared_mem->n_users; i++) {
        if (strcmp(username, shared_mem->users[i].username) == 0 && strcmp(password, shared_mem->users[i].password) == 0 && strcmp("professor", shared_mem->users[i].role) == 0) {
            strcpy(login_name, username);
            return 2; // Login correto para professor
        } else if (strcmp(username, shared_mem->users[i].username) == 0 && strcmp(password, shared_mem->users[i].password) == 0 && strcmp("aluno", shared_mem->users[i].role) == 0) {
            strcpy(login_name, username);
            return 1; // Login correto para aluno
        }
    }
    return 0; // Login incorreto
}

int verify_login_udp(char buffer[]) {
    char username[BUFFER_SIZE], password[BUFFER_SIZE];
    strcpy(username, strtok(buffer + 6, " "));
    strcpy(password, strtok(NULL, " "));
    password[strlen(password) -1] = '\0';
    for (int i = 0; i < shared_mem->n_users; i++) {
        if (strcmp(username, shared_mem->users[i].username) == 0 && strcmp(password, shared_mem->users[i].password) == 0 && strcmp("administrador",shared_mem->users[i].role) == 0) {
            return 1; // Login correto (admin)
            
        }
    }
    return 0; // Login incorreto
}


int conta_espacos(char buffer[]){
	int count = 0;
	int len = strlen(buffer);
	for(int i = 0; i < len;i++ ){
		if(buffer[i] == ' '){
			count++;
		} 
	}
	return count;
}

int find_class_by_name(const char *name) {
    for (int i = 0; i < shared_mem->n_classes; i++) {
        if (strcmp(shared_mem->classes[i].name, name) == 0){
            return i; 
        }
    }
    return -1;
}

int is_multicast_assigned(const char *multicast_address) {
    for (int i = 0; i < shared_mem->n_classes; i++) {
        if (strcmp(shared_mem->classes[i].multicast_address, multicast_address) == 0) {
            return 1;
        }
    }
    return -1;
}


void create_class(char buffer[],int tcp_socket, int *flag3){
    if(*flag3 == 1){
    	if(strcmp(buffer, "CREATE_CLASS xfjsjic 1") == 0){
    		if (send(tcp_socket, "PROF", strlen("PROF"), 0) == -1) {
            	perror("Error sending message\n");
        	}
        	*flag3 = 0;
        	return;
        }
    }
    strtok(buffer, " ");
    char *name = strtok(NULL, " ");
    char *size = strtok(NULL, " ");
    int class_index = find_class_by_name(name);
    if (class_index != -1) {
        snprintf(buffer, BUFFER_SIZE, "Class already exists!\n");
        if (send(tcp_socket, buffer, strlen(buffer), 0) == -1) {
            perror("Error sending message\n");
        }
        return;
    }
    char multicast_address[16];
    do {
        snprintf(multicast_address, sizeof(multicast_address), "239.0.0.%d", rand() % 256);
    } while (is_multicast_assigned(multicast_address)==1);
    
    strcpy(shared_mem->classes[shared_mem->n_classes].name, name);
    shared_mem->classes[shared_mem->n_classes].size=atoi(size);
    shared_mem->classes[shared_mem->n_classes].n_alunos=0;
    strcpy(shared_mem->classes[shared_mem->n_classes].multicast_address, multicast_address);
    strcpy(shared_mem->classes[shared_mem->n_classes].prof, login_name);
    shared_mem->n_classes++;

    snprintf(buffer, BUFFER_SIZE, "OK %s\n", multicast_address);
    if (send(tcp_socket, buffer, strlen(buffer), 0)== -1) {
        perror("Error sending message\n");
    }
}


void list_classes(int tcp_socket) {
    char buffer2[BUFFER_SIZE];
    char all[BUFFER_SIZE];
    memset(all, 0, BUFFER_SIZE);
    strcat(all,"CLASS ");
      for (int i = 0; i < shared_mem->n_classes; i++) {
        memset(buffer2, 0, BUFFER_SIZE);
        snprintf(buffer2, BUFFER_SIZE, "%s", shared_mem->classes[i].name);
        
        strcat(all, buffer2);
        if (i < shared_mem->n_classes - 1) {
            strcat(all, ", ");
        }
    }
    strcat(all, "\n");
    if (send(tcp_socket, all, strlen(all), 0) == -1) {
        perror("Error sending message");
    }
}

void list_subscribed(char login_name[], int tcp_socket){
    char temp[BUFFER_SIZE];
	char subscribed_classes[BUFFER_SIZE]; 
    memset(subscribed_classes, 0, BUFFER_SIZE);
    strcat(subscribed_classes, "CLASS ");
	for (int i = 0; i < shared_mem->n_classes; i++) {
        for (int j = 0; j < shared_mem->classes[i].size; j++) {
            if ((strcmp(shared_mem->classes[i].alunos[j].nome, login_name) == 0) || (strcmp(shared_mem->classes[i].prof, login_name)==0)) { 
                memset(temp, 0 , BUFFER_SIZE);
                snprintf(temp, BUFFER_SIZE, "%s/%s ", shared_mem->classes[i].name, shared_mem->classes[i].multicast_address);
                strcat(subscribed_classes, temp);
                break;
            }
        }
    }
    strcat(subscribed_classes, "\n");

    if(send(tcp_socket , subscribed_classes, strlen(subscribed_classes), 0)==-1){
        perror("Error sending message\n");
    }
}


void subscribe_class(char buffer[], char login_name[], int tcp_socket) {
    char temp[BUFFER_SIZE];
    strtok(buffer, " ");
    char *class_name = strtok(NULL, " ");
    if (class_name == NULL) {
        snprintf(temp, BUFFER_SIZE, "REJECTED.ENTER THE CLASS NAME AFTER THE COMMAND.\n");
        if (send(tcp_socket, temp, strlen(temp), 0) == -1) {
            perror("Error sending message");
        }
        return;
    }
    // Encontar a turma
    int class_index = find_class_by_name(class_name);
    if (class_index == -1) {
        snprintf(temp, BUFFER_SIZE, "REJECTED.CLASS DONT EXIST.\n");
        if (send(tcp_socket, temp, strlen(temp), 0) == -1) {
            perror("Error sending message");
        }
        return;
    }
    // Verificar se a turma esta cheia
    if (shared_mem->classes[class_index].n_alunos >= shared_mem->classes[class_index].size) {
        snprintf(temp, BUFFER_SIZE, "REJECTED.CLASS IS FULL\n");
        if (send(tcp_socket, temp, strlen(temp), 0) == -1) {
            perror("Error sending message");
        }
        return;
    }
    // Verificar se o aluno já esta na turma
    for (int i = 0; i < shared_mem->classes[class_index].n_alunos; i++) {
        if (strcmp(shared_mem->classes[class_index].alunos[i].nome, login_name) == 0) {
            snprintf(temp, BUFFER_SIZE, "REJECTED.STUDENT ALREADY IN THIS CLASS.\n");
            if (send(tcp_socket, temp, strlen(temp), 0) == -1) {
                perror("Error sending message");
            }
            return;
        }
    }

    // Adicionar aluno a turma
    strcpy(shared_mem->classes[class_index].alunos[shared_mem->classes[class_index].n_alunos].nome, login_name);
    shared_mem->classes[class_index].n_alunos++;
    snprintf(temp, BUFFER_SIZE, "ACCEPTED %s\n", shared_mem->classes[class_index].multicast_address);
    if (send(tcp_socket, temp, strlen(temp), 0) == -1) {
        perror("Error sending message");
    }
}

void invalid(int tcp_socket){
    char erro[BUFFER_SIZE];
    snprintf(erro, BUFFER_SIZE, "INVALID COMMAND OR LOGIN NOT EXECUTED: TRY AGAIN\n");
    if(send(tcp_socket, erro, strlen(erro), 0)==-1){
        perror("Error sending message");
    }
}

void *enviarMulticastUDP(char multicast_atual[], const char *mensagem) {
    // Inicializar o socket UDP
    int sock;
    struct sockaddr_in addr;
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    // Configurar a estrutura de endereço multicast
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(multicast_atual);
    addr.sin_port = htons(5000);
    
    // Habilitar multicast no socket
    int enable = 3;  //GNS3 maximo 3 routers
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &enable, sizeof(enable)) < 0) {
        perror("setsockopt");
        exit(1);
    }
    
    // Enviar a mensagem multicast
    if (sendto(sock, mensagem, strlen(mensagem), 0, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("sendto");
        exit(1);
    }
    fflush(stdin);
	
    close(sock); // Close the socket
    return NULL;
}

void send_msg(char buffer[]) {
    strtok(buffer, " ");
    char *nome_turma = strtok(NULL, " ");
    char *conteudo = strtok(NULL, "");
    
    // Encontrar o endereço multicast correspondente à turma
    char address[BUFFER_SIZE];
    int turma_encontrada = 0;
    for (int i = 0; i < shared_mem->n_classes; i++) {
        if (strcmp(shared_mem->classes[i].name, nome_turma) == 0) {
            strcpy(address, shared_mem->classes[i].multicast_address);
            turma_encontrada = 1;
            break;
        }
    }

    if (!turma_encontrada) {
        fprintf(stderr, "Turma não encontrada\n");
        return;
    }

    enviarMulticastUDP(address, conteudo);
}




void verifica_comando_cliente(char buffer[], char filename[], int tcp_socket, socklen_t client_addr_len, struct sockaddr_in client_addr, int flag2) {
    // Verifica o comando capturado usando um switch
    int count_espacos = conta_espacos(buffer);
    int flag3 = 1;
    switch(buffer[0]) {
        case 'C':
            if(strncmp(buffer, "CREATE_CLASS", strlen("CREATE_CLASS")) == 0 && count_espacos == 2 && flag2 == 2) {
                create_class(buffer, tcp_socket, &flag3);
                break;
            } 
            else {
                invalid(tcp_socket);
                break;
            }
            break;
        case 'L':
            if(strcmp(buffer, "LIST_CLASSES") == 0 && flag2 != 0) {
                list_classes(tcp_socket);
                break;
            }
            else if(strncmp(buffer, "LOGIN", strlen("LOGIN")) == 0 && count_espacos == 2 && flag2 == 0) {
                if ((flag2 = verify_login_tcp(buffer))) {
                    strcpy(buffer, "OK\n");
                    if (send(tcp_socket, buffer, strlen(buffer), 0) == -1) {
                        perror("TCP send failed");
                    }
                    // Lidar com os comandos do cliente
                    int flag;
                    while (1) {
                        // Receber comando do cliente
                        memset(buffer, 0, BUFFER_SIZE);
                        if ((flag = recv(tcp_socket, buffer, BUFFER_SIZE, 0)) == -1) {
                            perror("TCP receive failed");
                            close(tcp_socket);
                            pthread_exit(NULL);
                        }
                        if (flag == 0) {
                            break;
                        }
                        // Lidar com o comando recebido
                        verifica_comando_cliente(buffer,filename,tcp_socket,client_addr_len,client_addr,flag2);
                    }
                } else {
                    strcpy(buffer, "REJECTED\n");
                    if (send(tcp_socket, buffer, strlen(buffer), 0) == -1) {
                        perror("TCP send failed");
                    }
                }
            }
            else if(strcmp(buffer, "LIST_SUBSCRIBED") == 0 && flag2 != 0) {
                list_subscribed(login_name, tcp_socket);
                break;
            }
            else {
                invalid(tcp_socket);
                break;
            }
            break;
            
        case 'S':
            if(strncmp(buffer, "SUBSCRIBE_CLASS", strlen("SUBSCRIBE_CLASS")) == 0 && flag2 == 1 && count_espacos == 1) {
                subscribe_class(buffer, login_name, tcp_socket);
                break;
            }
            else if(strncmp(buffer, "SEND", strlen("SEND")) == 0 && count_espacos >= 2 && flag2 == 2) {
                send_msg(buffer);
                break;
            }
            else if(strcmp(buffer, "SAIR") == 0) {
                close(tcp_socket);
                
                pthread_exit(NULL);
                break;
            }
            else {
                invalid(tcp_socket);
                break;
            }
            break;
        default:
            invalid(tcp_socket);
            break;
    }
}


void add_user(char filename[], char buffer[], int udp_socket,socklen_t client_addr_len,struct sockaddr_in client_addr){
    FILE *file = fopen(filename, "a");
    if (file == NULL) {
        printf("Erro ao abrir o arquivo.\n");
        return;
    }
    strtok(buffer, " ");
    char *nome = strtok(NULL, " ");
    for(int i=0;i<shared_mem->n_users;i++){
        if(strcmp(nome, shared_mem->users[i].username)==0){
        	if (sendto(udp_socket, "Comando inválido. Utilizador já existe\n", strlen("Comando inválido. Utilizador já existe\n"), 0, (struct sockaddr *)&client_addr, client_addr_len) == -1) {
         		perror("UDP send failed");
      		}
            return;
        }
    }
    char *pass = strtok(NULL, " ");
    char *cargo = strtok(NULL, " ");
    if(strcmp(cargo,"professor")!=0 && strcmp(cargo,"aluno")!=0 && strcmp(cargo,"administrador")!=0){
    	if (sendto(udp_socket, "Comando inválido. Cargo introduzido inválido\n", strlen("Comando inválido. Cargo introduzido inválido\n"), 0, (struct sockaddr *)&client_addr, client_addr_len) == -1) {
         	perror("UDP send failed");
      	}
        return;
    }
    fprintf(file, "%s;%s;%s\n", nome, pass, cargo);
    fflush(file);
    fclose(file);

    // Atualiza a memória partilhada
    strcpy(shared_mem->users[shared_mem->n_users].username, nome);
    strcpy(shared_mem->users[shared_mem->n_users].password, pass);
    strcpy(shared_mem->users[shared_mem->n_users].role, cargo);
    shared_mem->n_users++;
    if (sendto(udp_socket, "Utilizador adicionado com sucesso\n", strlen("Utilizador adicionado com sucesso\n"), 0, (struct sockaddr *)&client_addr, client_addr_len) == -1) {
        perror("UDP send failed");
    }
}


void list(int udp_socket, socklen_t client_addr_len, struct sockaddr_in client_addr) {
    char buffer[BUFFER_SIZE];
    for(int i=0;i<shared_mem->n_users;i++){
        memset(buffer, 0,BUFFER_SIZE);
        snprintf(buffer, BUFFER_SIZE, "%s;%s;%s\n", shared_mem->users[i].username, shared_mem->users[i].password,shared_mem->users[i].role);
        if (sendto(udp_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&client_addr, client_addr_len) == -1) {
         perror("UDP send failed");
      }

    }
}


void quit_server(){
	if (shmdt(shared_mem) == -1) {
    	perror("shmdt");
    	exit(1);
	}
	if (shmctl(shmid, IPC_RMID, NULL) == -1) {
    	perror("shmctl");
    	exit(1);
	}
	exit(0);
}

int retorna_index(char buffer[]){
    char buffer_copy[BUFFER_SIZE];
    strcpy(buffer_copy, buffer);
    strtok(buffer_copy, " ");
    char *nome = strtok(NULL, " ");
    for(int i = 0; i < shared_mem->n_users; i++){
        if(strcmp(nome, shared_mem->users[i].username) == 0){
            return i;
        }
    }
    return -1;
}

void delete(char filename[], char buffer[], int index, int udp_socket,socklen_t client_addr_len,struct sockaddr_in client_addr) {
    FILE *file = fopen(filename, "r+");
    if (file == NULL) {
        printf("Erro ao abrir o arquivo.\n");
        return;
    }

    char buffer_copy[BUFFER_SIZE];
    strcpy(buffer_copy, buffer);
    strtok(buffer_copy, " ");
    char *nome = strtok(NULL, " ");

    char linha[BUFFER_SIZE];
    FILE *temp = fopen("temp.txt", "w");
    if (temp == NULL) {
        printf("Erro ao abrir o arquivo temporário.\n");
        fclose(file);
        return;
    }

    while (fgets(linha, sizeof(linha), file) != NULL) {
        if (strncmp(linha, nome, strlen(nome)) != 0) {
            fputs(linha, temp);
        }
    }

    fclose(file);
    fclose(temp);

    remove(filename);
    rename("temp.txt", filename);

    for(int i = index; i < shared_mem->n_users - 1; i++){
        shared_mem->users[i] = shared_mem->users[i + 1];
    }
    memset(&shared_mem->users[shared_mem->n_users - 1], 0, sizeof(UserInfo));
    shared_mem->n_users--;
    if (sendto(udp_socket, "Utilizador apagado\n", strlen("Utilizador apagado\n"), 0, (struct sockaddr *)&client_addr, client_addr_len) == -1) {
         	perror("UDP send failed");
    }
}


void verifica_comando_admin(char buffer[],char filename[], int udp_socket,socklen_t client_addr_len,struct sockaddr_in client_addr,int flag){
    // Verifica o comando capturado usando um switch
    int count_espacos = conta_espacos(buffer);
    switch(buffer[0]) {
        case 'A':
            if(strncmp(buffer, "ADD_USER", strlen("ADD_USER")) == 0 && count_espacos == 3 && flag == 1) {
                add_user(filename, buffer,udp_socket,client_addr_len, client_addr);
                break;
            } 
            else{
        		if (sendto(udp_socket, "Comando inválido. Tente novamente\n", strlen("Comando inválido. Tente novamente\n"), 0, (struct sockaddr *)&client_addr, client_addr_len) == -1) {
         			perror("UDP send failed");
      			}
            	break;
            }
        case 'D':
            if(strncmp(buffer, "DEL", strlen("DEL")) == 0 && count_espacos == 1 && flag == 1) {
                int index = retorna_index(buffer);
                if(index >= 0){
                    delete(filename, buffer, index,udp_socket,client_addr_len, client_addr);
                    break;
                }
            }
            else{
            	if (sendto(udp_socket, "Comando inválido. Tente novamente\n", strlen("Comando inválido. Tente novamente\n"), 0, (struct sockaddr *)&client_addr, client_addr_len) == -1) {
         			perror("UDP send failed");
      			}
            	break;
            }
            
        case 'L':
            if(strcmp(buffer, "LIST") == 0 && flag == 1) {
                list(udp_socket, client_addr_len, client_addr);
                break;
            } else if(strncmp(buffer, "LOGIN", strlen("LOGIN")) == 0 && count_espacos == 2 && flag == 0) {
                	// Verificar login
        			if ((flag = verify_login_udp(buffer))) {
    					memset(buffer, 0, BUFFER_SIZE);
    					strcpy(buffer, "OK\n");
    					if (sendto(udp_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&client_addr, client_addr_len) == -1) {
        					perror("UDP send failed");
    					}
    					// Lidar com os comandos do admin
    					while (1) {
        					memset(buffer, 0, strlen(buffer));
        					if (recvfrom(udp_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_addr_len) == -1) {
            					perror("UDP receive failed");
            					close(udp_socket);
            					pthread_exit(NULL);
        					}
							buffer[strlen(buffer) - 1] = '\0';
							verifica_comando_admin(buffer,filename,udp_socket,client_addr_len,client_addr,flag);
        					memset(buffer, 0, strlen(buffer));
    					}
					}else {
            			strcpy(buffer, "REJECTED. You don't have admin permission\n");
            			if (sendto(udp_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&client_addr, client_addr_len) == -1) {
                			perror("UDP send failed");
            			}
        			}
        			break;
            	}
            	
            else{
            	if (sendto(udp_socket, "Comando inválido. Tente novamente\n", strlen("Comando inválido. Tente novamente\n"), 0, (struct sockaddr *)&client_addr, client_addr_len) == -1) {
         			perror("UDP send failed");
      			}
            	break;	
            }
        case 'Q':
            if(strcmp(buffer, "QUIT_SERVER") == 0 && flag == 1) {
            	quit_server();
                break;
            } 
            else{
            	if (sendto(udp_socket, "Comando inválido. Tente novamente\n", strlen("Comando inválido. Tente novamente\n"), 0, (struct sockaddr *)&client_addr, client_addr_len) == -1) {
         			perror("UDP send failed");
      			}
            	break;
            }
               
        default:
        	if (sendto(udp_socket, "Comando inválido. Tente novamente\n", strlen("Comando inválido. Tente novamente\n"), 0, (struct sockaddr *)&client_addr, client_addr_len) == -1) {
         		perror("UDP send failed");
      		}
      		break;
         
    }
}

// Função executada por cada thread para lidar com uma conexão de cliente TCP
void *handle_connection_tcp(void *arg) {
    struct ConnectionArgs *args = (struct ConnectionArgs *)arg;

    int client_socket = args->client_socket;
    char filename[50];
    strcpy(filename, args->filename);
    
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // Obter informações do cliente
    getpeername(client_socket, (struct sockaddr *)&client_addr, &client_addr_len);
    
    int flag2 = 0;
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        // Receber comando do cliente
        if (recv(client_socket, buffer, BUFFER_SIZE, 0) == -1) {
            perror("TCP receive failed");
            close(client_socket);
            pthread_exit(NULL);
        }
        
        verifica_comando_cliente(buffer,filename,client_socket,client_addr_len,client_addr,flag2);
    }
}

void *handle_connection_udp(void *arg) {
    struct ConnectionArgs *args = (struct ConnectionArgs *)arg;
    int udp_socket = args->client_socket;
    char filename[50];
    strcpy(filename, args->filename);
    struct sockaddr_in client_addr;
    char buffer[BUFFER_SIZE];
    int bytes_received;
    socklen_t client_addr_len = sizeof(client_addr);
	int flag = 0;
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        // Receber dados do cliente UDP
        bytes_received = recvfrom(udp_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if (bytes_received < 0) {
            perror("UDP receive failed");
            continue;  // Continuar a execução após erro de recebimento
        }
        verifica_comando_admin(buffer,filename,udp_socket,client_addr_len,client_addr,flag);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    init();
    int tcp_socket, udp_socket;
    struct sockaddr_in server_tcp_addr, server_udp_addr;

    if (argc != 4) {
        printf("Usage: %s <TCP_PORT> <UDP_PORT> <CONFIG_FILE>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int TCP_PORT = atoi(argv[1]);
    int UDP_PORT = atoi(argv[2]);
    char CONFIG_FILE[50];
    strcpy(CONFIG_FILE, argv[3]);
    load_user_info(CONFIG_FILE);

    // Create TCP socket
    if ((tcp_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("TCP socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure TCP server address
    memset(&server_tcp_addr, 0, sizeof(server_tcp_addr));
    server_tcp_addr.sin_family = AF_INET;
    server_tcp_addr.sin_addr.s_addr = INADDR_ANY;
    server_tcp_addr.sin_port = htons(TCP_PORT);

    // Bind TCP socket
    if (bind(tcp_socket, (struct sockaddr *)&server_tcp_addr, sizeof(server_tcp_addr)) == -1) {
        perror("TCP bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for TCP connections
    if (listen(tcp_socket, 5) == -1) {
        perror("TCP listen failed");
        exit(EXIT_FAILURE);
    }
	 // Create UDP socket
    if ((udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("UDP socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure UDP server address
    memset(&server_udp_addr, 0, sizeof(server_udp_addr));
    server_udp_addr.sin_family = AF_INET;
    server_udp_addr.sin_addr.s_addr = INADDR_ANY;
    server_udp_addr.sin_port = htons(UDP_PORT);

    // Bind UDP socket
    if (bind(udp_socket, (struct sockaddr *)&server_udp_addr, sizeof(server_udp_addr)) == -1) {
        perror("UDP bind failed");
        exit(EXIT_FAILURE);
    }
	int flag = 1;
    printf("SERVER STARTED.....\n");
    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(tcp_socket, &readfds);
        FD_SET(udp_socket, &readfds);
		
        int max_fd = (tcp_socket > udp_socket) ? tcp_socket : udp_socket;

        // Esperar por eventos de leitura nos sockets TCP e UDP
        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) == -1) {
            perror("select");
            continue;
        }
		// Verificar se há conexões UDP
        if (flag == 1 && FD_ISSET(udp_socket, &readfds)) {
            // Criar uma nova thread para lidar com a conexão UDP
            pthread_t udp_thread;
            struct ConnectionArgs args;
        	args.client_socket = udp_socket;
        	strcpy(args.filename, CONFIG_FILE);
            if (pthread_create(&udp_thread, NULL, handle_connection_udp, (void *)&args) != 0) {
                perror("Thread creation failed");
                continue;
            }
            pthread_detach(udp_thread);
            flag = 0;
        }
        // Verificar se há conexões TCP

        if (FD_ISSET(tcp_socket, &readfds)) {
            // Accept TCP connection
        	struct sockaddr_in client_addr;
        	socklen_t client_addr_len = sizeof(client_addr);
        	int client_socket;
        	
        	if ((client_socket = accept(tcp_socket, (struct sockaddr *)&client_addr, &client_addr_len)) == -1) {
            	perror("TCP accept failed");
            	continue;
        	}
            
        	struct ConnectionArgs args;
        	args.client_socket = client_socket;
        	strcpy(args.filename, CONFIG_FILE);
            int id=fork();
            if(id==0){
                handle_connection_tcp((void *)&args);
                exit(0);
            }
        }
    }
    close(tcp_socket);
	close(udp_socket);
    return 0;
}
