#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#define PORT 69
#define MAX_BUF 516 // 512 octets de données + 4 octets d'en-tête
#define REPERTOIRE "SAE/TFTP/"

typedef struct {
    const char *ip;           
    int port;             
    const char *fichier;       
    int is_get;           
} requete_tftp_t;

// --- Fonctions de construction de paquets ---
void send_rrq(int sockfd, struct sockaddr_in *server_addr, socklen_t addr_len, const char *fichier) {
    char buffer[MAX_BUF];
    int len = 0;

    // Opcode RRQ = 1 (en Big-Endian)
    buffer[len++] = 0;
    buffer[len++] = 1;

    // Nom du fichier + \0
    strcpy(&buffer[len], fichier);
    len += strlen(fichier) + 1;

    // Mode "octet" + \0
    const char *mode = "octet";
    strcpy(&buffer[len], mode);
    len += strlen(mode) + 1;

    if (sendto(sockfd, buffer, len, 0, (struct sockaddr *)server_addr, addr_len) < 0) {
        perror("Erreur lors de l'envoi du RRQ");
    } else {
        printf("[CLIENT] RRQ envoyé pour le fichier : %s\n", fichier);
    }
}

void send_wrq(int sockfd, struct sockaddr_in *server_addr, socklen_t addr_len, const char *fichier) {
    char buffer[MAX_BUF];
    int len = 0;

    // Opcode WRQ = 2 (en Big-Endian)
    buffer[len++] = 0;
    buffer[len++] = 2;

    strcpy(&buffer[len], fichier);
    len += strlen(fichier) + 1;

    const char *mode = "octet";
    strcpy(&buffer[len], mode);
    len += strlen(mode) + 1;

    if (sendto(sockfd, buffer, len, 0, (struct sockaddr *)server_addr, addr_len) < 0) {
        perror("Erreur lors de l'envoi du WRQ");
    } else {
        printf("[CLIENT] WRQ envoyé pour le fichier : %s\n", fichier);
    }
}

// --- Programme Principal ---

int main(int argc, char const *argv[]) {
    int client_fd;
    requete_tftp_t config;
    struct sockaddr_in server_addr;

    if (argc < 4 || argc > 5) {
        fprintf(stderr, "Usage: %s <ip_serveur> <get|put> <nom_fichier> [port]\n", argv[0]);
        return 1;
    }

    config.ip = argv[1]; 
    config.is_get = (strcmp(argv[2], "get") == 0);
    config.fichier = argv[3];
    config.port = (argc == 5) ? atoi(argv[4]) : PORT;

    // 3. Création de la socket UDP
    if ((client_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("ERREUR : Impossible de créer la socket");
        return 2;
    }

    // 4. Préparation de l'adresse du serveur
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config.port);
    
    if (inet_pton(AF_INET, config.ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "ERREUR : Adresse IP '%s' invalide\n", config.ip);
        close(client_fd);
        return 3;
    }

    // 5. Exécution de la requête initiale

    if (config.is_get) {
        send_rrq(client_fd, &server_addr, sizeof(server_addr), config.fichier);
        // Ici viendra la boucle de réception (Étape suivante)
    } else {
        send_wrq(client_fd, &server_addr, sizeof(server_addr), config.fichier);
        // Ici viendra la boucle d'envoi (Étape suivante)
    }

    printf("[INFO] Requête initiale terminée. Fermeture de la socket.\n");
    close(client_fd);
    return 0;
}