#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 69
#define MAX_BUF 516

// --- Prototypes des fonctions de gestion ---
void traitement_rrq(struct sockaddr_in *client_addr, socklen_t addr_len, const char *fichier);
void traitement_wrq(struct sockaddr_in *client_addr, socklen_t addr_len, const char *fichier);

int main() {
    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[MAX_BUF];
    socklen_t addr_len = sizeof(client_addr);

    // 1. Création de la socket UDP
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 2. Configuration de l'adresse du serveur
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Écoute sur toutes les interfaces
    server_addr.sin_port = htons(PORT);

    // 3. Liaison (Bind) au port 69
    if (bind(server_fd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed (essayez avec sudo)");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("[SERVER] Serveur TFTP en attente sur le port %d...\n", PORT);

    // 4. Boucle infinie pour recevoir les requêtes
    while (1) {
        ssize_t n = recvfrom(server_fd, buffer, MAX_BUF, 0, (struct sockaddr *)&client_addr, &addr_len);
        if (n < 0) continue;

        // Lecture de l'Opcode (2 premiers octets)
        uint16_t opcode = ntohs(*(uint16_t *)buffer);
        char *fichier = buffer + 2; // Le nom du fichier commence après l'opcode

        if (opcode == 1) { // RRQ
            printf("[SERVER] RRQ reçu pour : %s\n", fichier);
            traitement_rrq(&client_addr, addr_len, fichier);
        } else if (opcode == 2) { // WRQ
            printf("[SERVER] WRQ reçu pour : %s\n", fichier);
            traitement_wrq(&client_addr, addr_len, fichier);
        }
    }

    close(server_fd);
    return 0;
}

// --- Implémentation simplifiée des handlers ---

void traitement_rrq(struct sockaddr_in *client_addr, socklen_t addr_len, const char *fichier) {
    // Étape 1 : Créer une nouvelle socket pour ce transfert (le TID)
    // Étape 2 : Ouvrir le fichier local en lecture ("rb")
    // Étape 3 : Boucle [Lire 512 octets -> Envoyer DATA -> Attendre ACK]
    printf("  -> Logique RRQ à implémenter pour %s\n", fichier);
}

void traitement_wrq(struct sockaddr_in *client_addr, socklen_t addr_len, const char *fichier) {
    // Étape 1 : Créer une nouvelle socket
    // Étape 2 : Envoyer ACK 0 au client pour dire "Prêt"
    // Étape 3 : Ouvrir le fichier local en écriture ("wb")
    // Étape 4 : Boucle [Attendre DATA -> Écrire -> Envoyer ACK]
    printf("  -> Logique WRQ à implémenter pour %s\n", fichier);
}