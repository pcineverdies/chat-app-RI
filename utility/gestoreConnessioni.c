#include "gestoreConnessioni.h"

// Funzione da usare ogni volta che si deve verificare che un valore sia diverso da -1
// (utile in tutti i casi di primitive legate alla connessione). Nel caso di errore termina
// il processo.
void errorHandler(int value, const char * errMsg){
    if(value == -1){
        if(errMsg == NULL) perror("Error: ");
        else printf("%s \n",errMsg);
        exit(1);
    }
}

// Funzione per inviare un pacchetto dato socket di destinazione, puntatore e dimensione
int inviaPacchetto(int sd, void* puntatore, int dimensione){
    // All'interno di questa funzione si dovrebbe implementare il meccanismo di controllo per quando
    // la send non invia il numero di byte che ci si aspetta; in questa sede ignoriamo tale
    // eventualità
    int ret;
    ret = send(sd, puntatore, dimensione, 0);
    errorHandler(ret, NULL);
    return ret;
}

// Funzione per ricevere un pacchetto dato socket mittente, puntatore e dimensione
int riceviPacchetto(int sd, void* puntatore, int dimensione){
    // All'interno di questa funzione si dovrebbe implementare il meccanismo di controllo per quando
    // la recv non riceve il numero di byte che ci si aspetta; in questa sede ignoriamo tale
    // eventualità
    int ret;
    ret = recv(sd, puntatore, dimensione, 0);
    errorHandler(ret, NULL);
    return ret;
}

// Funzione per aggiungere una connessione alla lista delle conessioni attive. Questa struttura dati
// è mantenuta sempre consistente nel momento in cui si modificano le connessioni attive, in modo da poterne
// far uso per rintracciare i peer a cui il device è connesso (server compreso).
struct connessione* aggiungiConnessione(struct connessione** listaConnessioni, uint16_t porta, int socketDescriptor, char* nomeUtente, int isServer){
    struct connessione* temp = (struct connessione*) malloc(sizeof(struct connessione));
    if(temp == NULL){
        printf("Errore! Impossibile allocare memoria per la creazione di una connessione!");
        exit(1);
    }
    strcpy(temp->nomeUtente,nomeUtente);
    temp->socketDescriptor = socketDescriptor;
    temp->porta = porta;
    temp->next = *listaConnessioni;
    temp->isServer = isServer;          // Si pone ad 1 nel caso in cui sia la connessione del server
    temp->gruppo = 0;                   // Token identificativo del gruppo di appartenenza
    *listaConnessioni = temp;
    return temp;
}

// Si usa, in fase di chiusura corretta del device, per terminare tutte le connessioni del device.
// Semplicemente dealloca la struttura dati e chiude i descrittori dei socket. 
void terminaConnessioni(struct connessione** listaConnessioni){
    struct connessione* p= (*listaConnessioni);
    struct connessione* q;
    int ret;
    
    if(p){
        ret = close(p->socketDescriptor);
        errorHandler(ret, NULL);
        q = p->next;
        memset(p, 0, sizeof(struct connessione));
        free(p);
        p=q;
    }
}

// Si usa per chiudere una specifica connessione dato il suo socket descriptor.
void chiudiConnessione(struct connessione** listaConnessioni, int socketDescriptor){
    struct connessione* p;
    struct connessione* q;
    int ret;

    // Caso in cui la lista è vuota
    if(*listaConnessioni == NULL) return;

    // Caso in cui si deve rimuovere la testa della lsita
    if((*listaConnessioni)->socketDescriptor == socketDescriptor){
        p = *listaConnessioni;
        *listaConnessioni = (*listaConnessioni)->next;
        ret = close(p->socketDescriptor);
        errorHandler(ret, NULL);
        memset(p, 0, sizeof(struct connessione));
        free(p);
        return;
    }

    // L'elemento da rimuovere è all'interno della lista
    p = (*listaConnessioni);
    q = (*listaConnessioni)->next;
    while(q){
        if(q->socketDescriptor == socketDescriptor){
            p->next = q->next;
            ret = close(q->socketDescriptor);
            errorHandler(ret, NULL);
            memset(q, 0, sizeof(struct connessione));
            free(q);
            return;
        }
        p = q;
        q = q->next;
    }
}

// Si usa per recuperare un puntatore alla struct connessione associata al server. 
struct connessione* ottieniConnessioneServer(struct connessione* listaConnessioni){
    struct connessione* temp = listaConnessioni;
    // Scorro tutti gli elementi della lista cercando quello che ha settato isServer
    while(temp){
        if(temp->isServer==1) break;
        else temp = temp->next;
    }
    if(!temp) return NULL;
    return temp;
}

// Si usa per recuperare un puntatore alla struct connessione associata ad un certo nomeUtente
struct connessione* ottieniConnessioneDevice(struct connessione* listaConnessioni, char* nomeUtente){
    struct connessione* temp = listaConnessioni;
    // Scorro tutti gli elementi della lista cercando quello che ha come nomeUtente quello inserito.
    while(temp){
        if(strcmp(temp->nomeUtente,nomeUtente)==0) break;
        else temp = temp->next;
    }
    if(!temp) return NULL;
    return temp;
}

// Si usa per recuperare un puntatore alla struct connessione associata ad un certo descrittore
// Nel caso in cui, ad esempio, si conosce il socket che ha fatto una richiesta e si vuole risalire
// alle informazioni sull'utente associato
struct connessione* ottieniConnessioneDeviceSD(struct connessione* listaConnessioni, int socketDescriptor){
    struct connessione* temp = listaConnessioni;
    while(temp){
        if(temp->socketDescriptor == socketDescriptor) break;
        else temp = temp->next;
    }
    if(!temp) return NULL;
    return temp;
}

// Apre una connessione TCP con la porta inserita, restituisce -1 nel caso in cui la connect non sia
// andata a buon fine (ad esempio, se si prova ad instaurare una connessione con il server quando questo
// è offline)
int apriConnessioneTCP(uint16_t portaDestinatario){
    int sd, ret;
    struct sockaddr_in destAddr;

    sd = socket(AF_INET, SOCK_STREAM, 0);
    errorHandler(sd, NULL);
    
    memset(&destAddr, 0, sizeof(portaDestinatario)); 
    destAddr.sin_family = AF_INET ;
    destAddr.sin_port = htons(portaDestinatario);
    inet_pton(AF_INET, "127.0.0.1", &destAddr.sin_addr);

    ret = connect(sd, (struct sockaddr*)&destAddr, sizeof(destAddr));
    
    if(ret==-1){
        close(sd);
        return -1;
    }
    else return sd;
}
