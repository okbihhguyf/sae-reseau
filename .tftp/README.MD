Voici les 4 règles d'or à vérifier dans tes fichiers :

    Le TID (Transfer Identifier) :

        Le client envoie à 69, mais reçoit d'un port X. Il doit envoyer tous ses futurs paquets vers X.

        Si un paquet arrive d'un port Y différent de X, il faut envoyer un paquet d'erreur (Code 5) à Y et continuer le transfert avec X.

    La fin de fichier (EOF) :

        Un transfert se termine toujours par un paquet DATA de moins de 512 octets de données (entre 0 et 511 octets).

        Si ton fichier fait exactement 512 ou 1024 octets, tu dois envoyer un dernier paquet DATA vide (0 octet de données) pour signaler la fin.

    Le mode "Octet" :

        Ta requête inclut bien la chaîne "octet". C'est parfait, cela signifie "binaire 8-bit".

    Décodage des Erreurs :

        Si tu reçois un Opcode 5, tu ne dois pas simplement arrêter, tu dois afficher le message d'erreur contenu dans le paquet pour le debug.

Résumé des fonctions à ajouter/modifier :

    send_error : Déjà présente dans ton serveur, copie-la dans ton client pour répondre aux paquets venant de mauvais ports.

    Vérification de sin_port : À ajouter dans chaque boucle recvfrom après le premier échange réussi.





CLIENT :
// --- Dans la fonction get() ---
struct sockaddr_in current_peer; // Pour stocker le TID (port de transfert) du serveur
int tid_set = 0;

// ... dans la boucle de réception ...
n = recvfrom(sockfd, buffer, MAX_BUF, 0, (struct sockaddr *)&peer_addr, &peer_len);

if (n >= 4) {
    if (!tid_set) {
        // PREMIER PAQUET REÇU : On fixe le TID
        memcpy(&current_peer, &peer_addr, peer_len);
        tid_set = 1;
    } else {
        // CONFORMITÉ RFC : Vérifier que le port (TID) n'a pas changé
        if (peer_addr.sin_port != current_peer.sin_port) {
            send_error(sockfd, &peer_addr, peer_len, 5, "Unknown TID"); // Erreur 5 : TID inconnu
            continue; // On ignore ce paquet parasite
        }
    }
    
    uint16_t block_num = ntohs(*(uint16_t *)(buffer + 2));
    // GESTION DES DOUBLONS : Si on reçoit un bloc déjà traité, on renvoie juste l'ACK
    if (block_num == dernier_lock_recu) {
        char ack[4] = {0, 4, buffer[2], buffer[3]};
        sendto(sockfd, ack, 4, 0, (struct sockaddr *)&current_peer, peer_len);
        continue; 
    }
    // ... traitement normal du nouveau bloc ...
}


SERVER :
// --- Dans traitement_rrq() ---
// GESTION DES DOUBLONS (ACK perdu)
// Si le client renvoie l'ACK du bloc N-1, le serveur doit renvoyer le bloc N.
// Si le client renvoie l'ACK du bloc N, on passe au bloc N+1.

if (r >= 4) {
    uint16_t ack_val = ntohs(*(uint16_t *)(ack_buf + 2));
    if (ack_val == block_num) {
        ack_recu = 1; // Succès
    } else if (ack_val == block_num - 1) {
        // Le client n'a peut-être pas reçu notre bloc DATA, il renvoie l'ancien ACK
        // On ne fait rien ici, la boucle de retransmission renverra le bloc block_num
    }
}

// --- Dans le main() ---
// CONFORMITÉ : Envoyer une erreur si l'opcode est inconnu
uint16_t opcode = ntohs(*(uint16_t *)buffer);
if (opcode == 1) traitement_rrq(...);
else if (opcode == 2) traitement_wrq(...);
else {
    send_error(server_fd, &client_addr, addr_len, 4, "Illegal TFTP operation");
}