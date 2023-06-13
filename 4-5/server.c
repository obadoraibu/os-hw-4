#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <netinet/in.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

void* handle_client(void* arg);
void* seller(void* arg);

int server_socket;

typedef struct {
    struct sockaddr_in client_addr;
    char buffer[BUFFER_SIZE];
    int sock;   // socket descriptor
} packet_data;

typedef struct node {
    int value;
    packet_data *data;
    struct node *next;
} Node;

typedef struct queue {
    Node *head;
    Node *tail;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} Queue;

void enqueue(Queue *q, int value, packet_data *data) {
    Node *newNode = malloc(sizeof(Node));
    newNode->value = value;
    newNode->data = data;
    newNode->next = NULL;

    pthread_mutex_lock(&q->lock);
    if (q->tail) {
        q->tail->next = newNode;
    } else {
        q->head = newNode;
    }
    q->tail = newNode;
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->lock);
}

Node dequeue(Queue *q) {
    pthread_mutex_lock(&q->lock);
    while (!q->head)
        pthread_cond_wait(&q->cond, &q->lock);

    Node *temp = q->head;
    Node returnValue = *temp;
    q->head = q->head->next;
    if (!q->head) {
        q->tail = NULL;
    }
    pthread_mutex_unlock(&q->lock);
    free(temp);

    return returnValue;
}

Queue q1 = { .head = NULL, .tail = NULL, .lock = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER };
Queue q2 = { .head = NULL, .tail = NULL, .lock = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER };
Queue q3 = { .head = NULL, .tail = NULL, .lock = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER };

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }


    struct sockaddr_in server_address, client_address;
    socklen_t client_address_len;
    int opt = 1;
    pthread_t thread_id, seller1, seller2, seller3;

    // Create server socket
    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket == -1) {
        perror("Failed to create server socket");
        exit(EXIT_FAILURE);
    }


    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("Failed to set socket option");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(atoi(argv[1]));

    // Bind 
    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        perror("Failed to bind server socket");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %s...\n", argv[1]);

    pthread_t seller1_thread, seller2_thread, seller3_thread;
    pthread_create(&seller1_thread, NULL, seller, &q1);
    pthread_create(&seller2_thread, NULL, seller, &q2);
    pthread_create(&seller3_thread, NULL, seller, &q3);


    while (1) {
        packet_data* data = malloc(sizeof(packet_data));
        memset(data, 0, sizeof(*data));
        data->client_addr.sin_family = AF_INET;
        data->sock = server_socket;  // Assign the socket descriptor
        socklen_t client_addr_size = sizeof(data->client_addr);
        ssize_t received_size = recvfrom(server_socket, data->buffer, BUFFER_SIZE - 1, 0, (struct sockaddr*)&data->client_addr, &client_addr_size);
        if (received_size < 0) {
            perror("recvfrom failed");
            free(data);
            continue;
        }


        data->buffer[received_size] = '\0';
        // new thread to handle the client
        if (pthread_create(&thread_id, NULL, handle_client, data) != 0) {
            perror("Failed to create thread");
            exit(EXIT_FAILURE);
        }

        pthread_detach(thread_id);
    }


    pthread_join(seller1_thread, NULL);
    pthread_join(seller2_thread, NULL);
    pthread_join(seller3_thread, NULL);
    return 0;
}

void *seller(void *arg) {
    Queue *q = (Queue*)arg;

    while (1) {
        Node node = dequeue(q);
        char clientIP[INET_ADDRSTRLEN];
        int clientPort = ntohs(node.data->client_addr.sin_port);
        inet_ntop(AF_INET, &(node.data->client_addr.sin_addr), clientIP, INET_ADDRSTRLEN);
        sleep(5);
        printf("Client %s:%d has been served\n", clientIP, clientPort);
        if (sendto(node.data->sock, "you have been served\n", strlen("you have been served\n"), 0, (struct sockaddr*)&node.data->client_addr, sizeof(node.data->client_addr)) < 0) {
            perror("sendto failed in seller");
            close(node.data->sock);
            return NULL;
        }
        free(node.data);
    }
    return NULL;
}

void* handle_client(void* arg) {

    packet_data* data = (packet_data*) arg;
    char clientIP[INET_ADDRSTRLEN];
    int clientPort = ntohs(data->client_addr.sin_port);
    inet_ntop(AF_INET, &(data->client_addr.sin_addr), clientIP, INET_ADDRSTRLEN);


        if (strcmp(data->buffer, "1\n") == 0) {
            printf("Client %s:%d got in line 1\n", clientIP, clientPort);
            
            if (sendto(data->sock, "you got in line 1\n", strlen("you got in line 1\n"), 0, (struct sockaddr*)&data->client_addr, sizeof(data->client_addr)) < 0) {
                perror("sendto failed");
                close(data->sock);
            return NULL;
            }

            enqueue(&q1, 1, data);
        } else if (strcmp(data->buffer, "2\n") == 0) {
            printf("Client %s:%d got in line 2\n", clientIP, clientPort);
            
            if (sendto(data->sock, "you got in line 2\n", strlen("you got in line 2\n"), 0, (struct sockaddr*)&data->client_addr, sizeof(data->client_addr)) < 0) {
                perror("sendto failed");
                close(data->sock);
                return NULL;
            }

            enqueue(&q2, 2, data);
        } else if (strcmp(data->buffer, "3\n") == 0) {
            printf("Client %s:%d got in line 3\n", clientIP, clientPort);
            
            if (sendto(data->sock, "you got in line 3\n", strlen("you got in line 3\n"), 0, (struct sockaddr*)&data->client_addr, sizeof(data->client_addr)) < 0) {
                perror("sendto failed");
                close(data->sock);
                return NULL;
            }

            enqueue(&q3, 3, data);
        } else if (strcmp(data->buffer, "exit\n") == 0) {
            printf("Client %s:%d left\n", clientIP, clientPort);

            if (sendto(data->sock, "exit\n", strlen("exit\n"), 0, (struct sockaddr*)&data->client_addr, sizeof(data->client_addr)) < 0) {
                perror("sendto failed");
                close(data->sock);
                return NULL;
            }

        }

    pthread_exit(NULL);
}