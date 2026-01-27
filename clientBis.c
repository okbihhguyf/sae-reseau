#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 69
#define MAX_BUF 516 // 512 octets de données + 4 octets d'en-tête

typedef struct {
    const char *ip;           // Adresse IP ou nom d'hôte du serveur
    int port;             // Port ( 69 par défaut ou celui d'argv[4])
    const char *fichier;       // Nom du fichier à lire ou écrire
    int is_get;           // Booleen : 1 pour GET (RRQ), 0 pour PUT (WRQ)
} requete_tftp_t;



void send_rrq(int sockfd, struct sockaddr_in *server_addr, socklen_t addr_len, const char *fichier) {
    //  [ CLIENT] construction d'un packet (opcode, nom fichier ,mode) 
    // -> [ SERVER ] en retour va faire la reponse au client avec le fichier divisé en plusieurs paquets 
    char buffer[MAX_BUF];
    int len = 0;

    // Opcode pour RRQ est 1
    buffer[len++] = 0;
    buffer[len++] = 1;

    strcpy(&buffer[len], fichier);
    len += strlen(fichier) + 1;

    // Ajouter le mode (octet)
    const char *mode = "octet";
    strcpy(&buffer[len], mode);
    len += strlen(mode) + 1;

    // Envoyer le paquet RRQ au serveur
    sendto(sockfd, buffer, len, 0, (struct sockaddr *)server_addr, addr_len);
}
void send_wrq(int sockfd, struct sockaddr_in *server_addr, socklen_t addr_len, const char *fichier) {
    //  [ CLIENT] construction d'un packet (opcode, nom fichier ,mode) 
    // -> [ SERVER ] en retour va faire la reponse au client avec le fichier divisé en plusieurs paquets 
    char buffer[MAX_BUF];
    int len = 0;

    // Opcode pour WRQ est 2
    buffer[len++] = 0;
    buffer[len++] = 2;

    strcpy(&buffer[len], fichier);
    len += strlen(fichier) + 1;

    // Ajouter le mode (octet)
    const char *mode = "octet";
    strcpy(&buffer[len], mode);
    len += strlen(mode) + 1;

    // Envoyer le paquet WRQ au serveur
    sendto(sockfd, buffer, len, 0, (struct sockaddr *)server_addr, addr_len);
}



int main(int argc, char const *argv[]) {
    int client_fd;
    requete_tftp_t config;

    //tester si les argements sont egal a
    if (argc < 4 || argc > 5) {
        printf("Usage: %s <ip> <get|put> <fichier> [port]\n", argv[0]);
        return 1;
    }
    config.is_get = (strcmp(argv[2], "get") == 0) ? 1 : 0;
    config.fichier = (char *)argv[3];
    config.port = (argc == 5) ? atoi(argv[4]) : PORT;
    config.ip = (char *)argv[3];

    struct sockaddr_in server_addr;
    
    // 1. Changement SOCK_STREAM -> SOCK_DGRAM (UDP) car TFTP utilise UDP et non TCP (etudier les différences entre les 2 protocoles)
    if ((client_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("ERREUR : generation socket");
        return 2;
    }
// Le système a besoin que vous convertissiez l'IP (texte) et le Port (entier) 
// dans un format compréhensible par la carte réseau.
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config.port);
    if (inet_pton(AF_INET, config.ip, &server_addr.sin_addr) <= 0) {
        perror("ERREUR : conversion IP");
        close(client_fd);
        return 3;
    }
    // requete get ou put
    if (config.is_get) {
        // code pour get 

    }else {
         // code pour put
         

    }
    //   gestion des timeouts ?
    close(client_fd);
    return 0;
}