#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

#define REPOSITORY ".tftp/"
#define PORT 69
#define MAX_BUF 516
#define TFTP_TIMEOUT_SEC 5
#define TFTP_MAX_ESSAI 5

void send_error(int sockfd, struct sockaddr_in *client_addr, socklen_t addr_len, uint16_t err_code, const char *err_msg) {
    char err_packet[MAX_BUF];
    uint16_t opcode = htons(5); 
    uint16_t error_code = htons(err_code); 
    memcpy(err_packet, &opcode, 2);
    memcpy(err_packet + 2, &error_code, 2);
    int len = 4 + sprintf(err_packet + 4, "%s", err_msg) + 1;
    sendto(sockfd, err_packet, len, 0, (struct sockaddr *)client_addr, addr_len);
}

void traitement_rrq(struct sockaddr_in *client_addr, socklen_t addr_len, const char *fichier) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0); 
    
    // Configurer le timeout sur la socket de transfert
    struct timeval tv = {TFTP_TIMEOUT_SEC, 0}; 
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (strstr(fichier, "..")) {
        printf("  [SERVER] Erreur : Tentative d'accès non autorisé '%s'.\n", fichier);
        send_error(sockfd, client_addr, addr_len, 2, "Access violation");
        close(sockfd);
        return;
    }

    char chemin[256];
    snprintf(chemin, sizeof(chemin), REPOSITORY "%s", fichier);
    FILE *f = fopen(chemin, "rb");
    
    if (!f) {
        printf("  [SERVER] Erreur : Fichier '%s' introuvable.\n", chemin);
        send_error(sockfd, client_addr, addr_len, 1, "File not found");
        close(sockfd);
        return;
    }

    char buffer[MAX_BUF];
    char ack_buf[4];
    uint16_t block_num = 1;
    size_t read_len;

    do {
        uint16_t opcode = htons(3);
        uint16_t block = htons(block_num);
        memcpy(buffer, &opcode, 2);
        memcpy(buffer + 2, &block, 2);
        read_len = fread(buffer + 4, 1, 512, f);

        int tentatives = 0;
        int ack_recu = 0;
        struct sockaddr_in peer_addr;
        socklen_t peer_len = sizeof(peer_addr);

        // Boucle de retransmission pour le bloc actuel
        while (tentatives < TFTP_MAX_ESSAI && !ack_recu) {
            if (sendto(sockfd, buffer, read_len + 4, 0, (struct sockaddr *)client_addr, addr_len) < 0) {
                perror("sendto");
                break;
            }
            printf("  [GET] ACK %d recu ,envoi du bloc %d (%zu octets)...\n", block_num, block_num, read_len);

            ssize_t r = recvfrom(sockfd, ack_buf, 4, 0, (struct sockaddr *)&peer_addr, &peer_len);
            if (r >= 4) {
                // Si paquet provenant d'une source inattendue, envoyer ERROR(5) "Unknown transfer ID" (RFC)
                if (peer_addr.sin_addr.s_addr != client_addr->sin_addr.s_addr || peer_addr.sin_port != client_addr->sin_port) {
                    printf("  [WARNING] Paquet inattendu depuis %s:%d (attendu %s:%d) -> envoi ERROR(5)\n",
                        inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port),
                        inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
                    send_error(sockfd, &peer_addr, peer_len, 5, "Unknown transfer ID");
                    continue; // attendre le bon paquet
                }
                uint16_t ack_val = ntohs(*(uint16_t *)(ack_buf + 2));
                if (ack_val == block_num) {
                    ack_recu = 1; // ACK valide reçu !
                }
            } else if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                tentatives++;
                printf("  [TIMEOUT] Pas d'ACK pour bloc %d, tentative %d/%d...\n", block_num, tentatives, TFTP_MAX_ESSAI);
            } else {
                if (r < 0) perror("recvfrom");
                break;
            }
        }

        if (!ack_recu) break; // On arrête tout si le client ne répond plus
        block_num++;
    } while (read_len == 512);

    printf("  [GET] Transfert de '%s' terminé.\n", fichier);
    fclose(f);
    close(sockfd);
}

void traitement_wrq(struct sockaddr_in *client_addr, socklen_t addr_len, const char *fichier) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    // Configurer le timeout
    struct timeval tv = {TFTP_TIMEOUT_SEC, 0};
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char *buffer_final = NULL;
    size_t taille_totale = 0;
    uint16_t dernier_block_recu = 0;
    
    // ACK 0 initial
    char ack[4] = {0, 4, 0, 0};
    if (sendto(sockfd, ack, 4, 0, (struct sockaddr *)client_addr, addr_len) < 0) perror("sendto");
    printf("  [PUT] ACK 0 envoyé\n");

    char buffer_reception[MAX_BUF];
    ssize_t n;
    printf("  [PUT] Réception de '%s'...\n", fichier);

    struct sockaddr_in peer_addr;
    socklen_t peer_len = sizeof(peer_addr);
    int peer_set = 0;

    do {
        int tentatives = 0;
        int recu_ok = 0;

        while (tentatives < TFTP_MAX_ESSAI && !recu_ok) {
            ssize_t r = recvfrom(sockfd, buffer_reception, MAX_BUF, 0, (struct sockaddr *)&peer_addr, &peer_len);

            if (r >= 4) {
                // si peer non défini, on l'enregistre
                if (!peer_set) 
                    peer_set = 1;
                // Vérification stricte du TID (Transfer Identifier) : IP et Port doivent correspondre
                if (peer_addr.sin_addr.s_addr != client_addr->sin_addr.s_addr || peer_addr.sin_port != client_addr->sin_port) {
                    printf("  [WARNING] Paquet inattendu depuis %s:%d (attendu %s:%d) -> envoi ERROR(5)\n",
                        inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port),
                        inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
                    send_error(sockfd, &peer_addr, peer_len, 5, "Unknown transfer ID");
                    continue;
                }

                uint16_t block_recu = ntohs(*(uint16_t *)(buffer_reception + 2));
                if (block_recu == dernier_block_recu + 1) {
                    recu_ok = 1; // Nouveau bloc reçu
                    n = r;
                } else if (block_recu == dernier_block_recu) {
                    // Doublon : on renvoie juste l'ACK sans traiter les données
                    if (sendto(sockfd, ack, 4, 0, (struct sockaddr *)&peer_addr, peer_len) < 0) 
                        perror("sendto");
                    // ou sinon paquet hors séquence -> on ignore
                }
            } else if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                tentatives++;
                printf("  [TIMEOUT] Attente bloc %d, tentative %d/%d... Renvoi dernier ACK.\n", dernier_block_recu + 1, tentatives, TFTP_MAX_ESSAI);
                // renvoyer le dernier ACK connu
                if (peer_set) {
                    if (sendto(sockfd, ack, 4, 0, (struct sockaddr *)&peer_addr, peer_len) < 0) perror("sendto");
                } else {
                    if (sendto(sockfd, ack, 4, 0, (struct sockaddr *)client_addr, addr_len) < 0) perror("sendto");
                }
            } else {
                if (r < 0) perror("recvfrom");
                break;
            }
        }

        if (!recu_ok) break;

        // Traitement des données reçues
        size_t taille_donnees = n - 4;
        buffer_final = realloc(buffer_final, taille_totale + taille_donnees);
        memcpy(buffer_final + taille_totale, buffer_reception + 4, taille_donnees);
        taille_totale += taille_donnees;

        // Préparation et envoi du nouvel ACK
        dernier_block_recu = ntohs(*(uint16_t *)(buffer_reception + 2));
        memcpy(ack + 2, buffer_reception + 2, 2);
        if (peer_set) {
            if (sendto(sockfd, ack, 4, 0, (struct sockaddr *)&peer_addr, peer_len) < 0) perror("sendto");
        } else {
            if (sendto(sockfd, ack, 4, 0, (struct sockaddr *)client_addr, addr_len) < 0) perror("sendto");
        }
        printf("  [PUT] ACK %d envoyé\n", dernier_block_recu);

    } while (n == 516);

    // Sauvegarde identique à ton code
    mkdir(REPOSITORY, 0777);
    
    if (strstr(fichier, "..")) {
         printf("  [SERVER] Erreur : Tentative d'accès non autorisé '%s'.\n", fichier);
         // Note: socket déjà fermée à la fin de la fonction, mais on ne doit pas écrire.
         // cleanup
         free(buffer_final);
         close(sockfd);
         return;
    }

    char chemin[256];
    snprintf(chemin, sizeof(chemin), REPOSITORY "%s", fichier);
    FILE *f = fopen(chemin, "wb");
    if (f) { 
        fwrite(buffer_final, 1, taille_totale, f); 
        printf("  [PUT] Transfert de '%s' terminé.\n", fichier);
        fclose(f); }
    free(buffer_final);
    close(sockfd);
}

int main() {
    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[MAX_BUF];
    socklen_t addr_len = sizeof(client_addr);



    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    printf("[SERVER] En attente sur le port %d...\n", PORT);
    while (1) {
        ssize_t n = recvfrom(server_fd, buffer, MAX_BUF, 0, (struct sockaddr *)&client_addr, &addr_len);
        if (n < 4) continue; // tftp min 4 octets(2 opcode + 1+ nom fichier +1 + mode +1)
        uint16_t opcode = ntohs(*(uint16_t *)buffer);
        if (opcode == 1)
            traitement_rrq(&client_addr, addr_len, buffer + 2);
        else if 
            (opcode == 2) traitement_wrq(&client_addr, addr_len, buffer + 2);
    }
    return 0;
}