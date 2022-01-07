#include "gestoreMessaggi.h"
#include "gestoreConnessioni.h"

// Funzione per ordinare i messaggi nella realizzazione di listaMessaggi. In particolare,
// il parametro ordine si usa per distinguere l'ordinamento effettuato nella lista dei device
// e dei server: per i device basta ordinare la lista per timestamp crescente, nel server occorre
// utilizzare accorgimenti maggiori affinché la funzione hanging si possa realizzare in maniera 
// efficiente. Ritorna 1 se m1<m2, 0 altrimenti
int ordina(struct messaggio* m1, struct messaggio* m2, int ordine){
    if(ordine==ORDINE_SERVER){
        if(strcmp(m1->destinatario,m2->destinatario)>0){
            return 1;
        }
        if(strcmp(m1->destinatario,m2->destinatario)<0){
            return 0;
        }
        if(strcmp(m1->mittente,m2->mittente)>0){
            return 1;
        }
        if(strcmp(m1->mittente,m2->mittente)<0){
            return 0;
        }
        if(m1->timestamp<m2->timestamp){
            return 1;
        }
        return 0;
    }
    // caso in cui ordine == ORDINE_DEVICE
    if(m1->timestamp<m2->timestamp){
        return 1;
    }
    else{
        return 0;
    }
}

// Effettua l'inserimento ordinato in listaMessaggi, secondo l'ordine inserito. 
void inserimentoOrdinatoListaMessaggi(struct messaggio* nuovoNodo, int ordine){
    struct messaggio    *p,*q;

    // inserimento da lista vuota
    if(listaMessaggi==NULL){
        listaMessaggi=nuovoNodo;
        return;
    }
    // Inserimento in testa
    if(ordina(listaMessaggi,nuovoNodo, ordine)==0){
        nuovoNodo->next = listaMessaggi;
        listaMessaggi = nuovoNodo;
        return;
    }
    // Inserimento più avanti nella lista
    p=listaMessaggi;
    q=listaMessaggi->next;
    while(q){
        if(ordina(q,nuovoNodo, ordine)==0) break;
        q=q->next;
        p=p->next;
    }
    p->next = nuovoNodo;
    nuovoNodo->next = q;
}

// Funzione che dealloca le strutture dati usate per realizzare la listaMessaggi. 
void chiudiListaMessaggi(){
    struct messaggio* p = listaMessaggi;
    struct messaggio* q;
    while(p != NULL){
        q = p->next;
        free(p);
        p = q;
    }
}

// Funzione che dealloca le strutture dati usate per realizzare la listaACK (lato server).
void chiudiListaACK(){
    struct ack* p = listaACK;
    struct ack* q;
    while(p != NULL){
        q = p->next;
        free(p);
        p = q;
    }
}

// Apre il file della lista dei messaggi
FILE* apriFileListaMessaggi(char* type, char* filename){
    FILE*               file;

    // Prima si apre in append in modo che, se il file non esiste, si possa creare senza alternarne il risultato.
    file = fopen(filename,"a");
    if(file == NULL){
        printf("Errore! Non è stato possibile aprire la lista dei messaggi!\n");
        exit(1);
    }
    fclose(file);
    // Dopo si apre secondo la modalità "type" e si ritorna il puntatore al file così aperto. 
    file = fopen(filename,type);
    if(file == NULL){
        printf("Errore! Non è stato possibile aprire la lista dei messaggi!\n");
        exit(1);
    }
    return file;
}

// Apre la lista dei messaggi 
void apriListaMessaggi(char* fileName, int ordine){
    FILE*               file;
    char                mittente[MAX_LEN_UTN+1];
    char                destinatario[MAX_LEN_UTN+1];
    int                 stato, i;
    time_t              timestamp;
    char                testo[MAX_LEN_MSG];
    struct messaggio*   temp = NULL;

    file = apriFileListaMessaggi("r", fileName);

    // Si scorrono le righe della lista dei messaggi, per ciascuna si realizza un elemento della listaMessaggi
    while(fscanf(file, "%s %s %d %ld\n", mittente, destinatario, &stato, &timestamp) != EOF){
        fgets(testo, MAX_LEN_MSG,file);
        temp = (struct messaggio*) malloc(sizeof(struct messaggio));
        if(temp == NULL){
            printf("Errore! Impossibile allocare memoria per la creazione della lista dei messaggi!");
            exit(1);
        }

        strcpy(temp->mittente,mittente);
        strcpy(temp->destinatario,destinatario);
        temp->stato = stato;
        temp->timestamp = timestamp;
        // Sosotituisco \n con \0, per non avere problemi nella gestione della stringa. 
        i=0; 
        strcpy(temp->testo,testo);
        while(1){
            if(temp->testo[i]=='\n'){
                temp->testo[i]='\0';
                break;
            }
            i = i+1;
        }

        temp->next = NULL;
        printf("%s", temp->testo);
        inserimentoOrdinatoListaMessaggi(temp, ordine);
    }
    fclose(file);
}

// Aggiorno il file della lista di messsagi, copiandovi il contenuto di listaMessaggi secondo il formato opportuno
// Affinché possa essere letto al giro successivo. In particolare, se rimuoviLetti==1, allora non si trascrivono tutti
// i messaggi che hanno stato==2, usato nel caso del server per dire che un dato messaggio è stato recapitato al 
// destinatario (e, in quanto tale, non ha senso salvarlo). 
void aggiornaFileListaMessaggi(char* filename, int rimuoviLetti){
    FILE*             file;
    struct messaggio* temp = listaMessaggi;

    file = apriFileListaMessaggi("w",filename);
    while(temp){
        // Caso in cui non salvo il messaggio sul file.
        if(rimuoviLetti==1 && temp->stato==2){
            temp = temp->next;
        }
        // Aggiungo il messaggio in fondo al file. 
        else{
            fprintf(file, "%s %s %d %ld\n",temp->mittente, temp->destinatario, temp->stato, temp->timestamp);
            fprintf(file, "%s\n", temp->testo);
            temp = temp->next;
        }
    }
    fclose(file);
}

// Dati i parametri necessari, si crea un elemento della lista di messaggi, si agigunge alla struttura dati e si aggiorna
// il file dei messaggi. Per creare un messaggio sono necessari mittente, destinatario, stato (letto/non letto per il device),
// timestamp di invio, nome del file in cui salvarlo e ordine con cui salvare i messaggi (da distingure i casi di server e device)
void aggiungiMessaggio(char* mittente, char* destinatario, int stato, time_t timestamp, char* testo, char* filename, int ordine){
    struct messaggio*   temp;
    temp = (struct messaggio*) malloc(sizeof(struct messaggio));
    if(temp == NULL){
        printf("Errore! Impossibile allocare memoria per il nuovo messaggio!");
        exit(1);
    }
    strcpy(temp->destinatario, destinatario);
    strcpy(temp->mittente, mittente);
    temp->stato = stato;
    temp->timestamp = timestamp;
    strcpy(temp->testo, testo);
    inserimentoOrdinatoListaMessaggi(temp, ordine);
    aggiornaFileListaMessaggi(filename, 0);
}

// Apre la lista degli ack di avvenuta lettura, allo stesso modo di listaConnessione e listaMessaggi. Un ack è
// salvato nel file del server come [mittente_destinatario_timestamp], quindi si recuperano in tale ordine. 
void apriListaACK(char* pathFile){
    FILE*       file = fopen(pathFile, "r");
    char        mittente[MAX_LEN_UTN+1];
    char        destinatario[MAX_LEN_UTN+1];
    int         stato;
    struct ack  *last, *temp;

    while(fscanf(file, "%s %s %d\n", mittente, destinatario, &stato)!=EOF){
        temp = (struct ack*) malloc(sizeof(struct ack));
        strcpy(temp->mittente, mittente);
        strcpy(temp->destinatario, destinatario);
        temp->inviato = stato;
        temp->next = NULL;
        if(listaACK == NULL){
            listaACK = temp;
            last = temp;
        }
        else{
            last->next = temp;
            last = temp;
        }
    }
    fclose(file);
}

// Funzione per vedere se esiste un ack realitvo alla coppia [mittente, destinatario] (mittente
// è colui che ha inviato il messaggio, quindi colui che deve ricevere l'ack quando destinatario usa
// show). 
struct ack* trovaACK(char* mittente, char* destinatario){
    struct ack      *temp = listaACK;
    while(temp){
        if(strcmp(temp->mittente, mittente)==0 && strcmp(temp->destinatario, destinatario)==0 && temp->inviato==0) return temp;
        temp = temp->next;
    }
    return NULL;
}

// Funzione per salvare gli ack della lista dentro un file. Scorre la lista e scrive una riga per ogni
// ack da memorizzare. 
void salvaListaACK(char* pathFile){
    FILE        *file = fopen(pathFile, "w");
    struct ack  *temp = listaACK;
    while(temp){
        if(temp->inviato==0){
            fprintf(file, "%s %s %d\n", temp->mittente, temp->destinatario, temp->inviato);
        }
        temp = temp->next;
    }
    fclose(file);
}

// Funzione per aggiunere un ack alla lista degli ack. 
void aggiungiACK(char* mittente, char*destinatario){
    struct ack      *newNode = (struct ack*) malloc(sizeof(struct ack));
    struct ack      *temp = listaACK;

    strcpy(newNode->mittente, mittente);
    strcpy(newNode->destinatario, destinatario);
    newNode->inviato = 0;
    newNode->next = NULL;

    if(!listaACK){
        listaACK = newNode;
        return;
    }
    while(temp->next){
        temp = temp->next;
    }
    temp->next = newNode;
}

// Funzione per ricevere un messaggio dal socket sd.
void riceviMessaggio(int sd, char* mittente, char* destinatario, char* messaggio, time_t* timestamp){
    uint16_t    dimensione, dimensioneNET;
    // Dimensione del nome utente di mittente
    riceviPacchetto(sd, (void*)&dimensioneNET, sizeof(uint16_t));
    dimensione = ntohs(dimensioneNET);
    // Ricezione di mittente
    riceviPacchetto(sd, (void*)mittente, dimensione);
    // Dimensione del nome utente di destinatario
    riceviPacchetto(sd, (void*)&dimensioneNET, sizeof(uint16_t));
    dimensione = ntohs(dimensioneNET);
    // Ricezine di destinatario
    riceviPacchetto(sd, (void*)destinatario, dimensione);
    // Ricezione del timestamp di invio e trasporizione di endianess
    riceviPacchetto(sd, (void*)timestamp, sizeof(time_t));
    (*timestamp) = ntohl((*timestamp));
    // Ricezione della dimensione del messaggio
    riceviPacchetto(sd, (void*)&dimensioneNET, sizeof(uint16_t));
    dimensione = ntohs(dimensioneNET);
    // Ricezione del messaggio
    riceviPacchetto(sd, (void*)messaggio, dimensione);
}

// Funzione per inviare un messaggio al socket sd.
void inviaMessaggio(int sd, char* mittente, char* destinatario, char* messaggio, time_t timestamp){
    uint16_t dimensione, dimensioneNET;
    time_t timestampNET = htonl(timestamp);
    // Calcolo della dimenisone del nome utente del mittente
    dimensione = strlen(mittente)+1;
    dimensioneNET = htons(dimensione);
    // Invio dimensione del nome utente del mittente
    inviaPacchetto(sd,(void*)&dimensioneNET,sizeof(uint16_t));
    // Invio nome untete del mittente
    inviaPacchetto(sd, (void*)mittente,dimensione);
    // Calcolo della dimenisone del nome utente del destinatario
    dimensione = strlen(destinatario)+1;
    dimensioneNET = htons(dimensione);
    // Invio dimensione del nome utente del destinatario
    inviaPacchetto(sd,(void*)&dimensioneNET,sizeof(uint16_t));
    // Invio nome utente del destinatario
    inviaPacchetto(sd, (void*)destinatario,dimensione);
    // invio timestamp del messaggio
    inviaPacchetto(sd, (void*)&timestampNET, sizeof(time_t));
    // Calcolo della dimensione del testo di un messaggio
    dimensione = strlen(messaggio)+1;
    dimensioneNET = htons(dimensione);
    // Invio dimensioen del testo di un messaggio
    inviaPacchetto(sd,(void*)&dimensioneNET,sizeof(uint16_t));
    // Invio messaggio
    inviaPacchetto(sd, (void*)messaggio,dimensione);
}

// Invio di una riga di hanging, inviando mittente, timestamp del messaggio più recente e numero di messaggi 
void invioRigaHanging(int sd, char* mittente, time_t timestamp, uint16_t numeroChat){
    uint16_t    dimensione, dimensioneNET, numeroMessaggiNET;
    time_t      timestampNET;
    // Calcolo dimensione del nome utente del mittente
    dimensione = strlen(mittente)+1;
    dimensioneNET = htons(dimensione);
    // Invio dimensione del nome utente del mittente
    inviaPacchetto(sd, (void*)&dimensioneNET, sizeof(uint16_t));
    // Invio nome utente del mittente
    inviaPacchetto(sd, (void*)mittente, dimensione);
    // Invio timesstamp del messaggio più recente
    timestampNET = htonl(timestamp);
    inviaPacchetto(sd, (void*)&timestampNET, sizeof(time_t));
    // Invio del numero dei messaggi 
    numeroMessaggiNET = htons(numeroChat);
    inviaPacchetto(sd, (void*)&numeroMessaggiNET, sizeof(uint16_t));
}

// Ricezione di una riga di hanging, con mittente, timestamp del messaggio più recente e numero di messaggi 
void riceviRigaHanging(int sd, char* mittente, time_t* timestamp, uint16_t* numeroChat){
    uint16_t    dimensione, dimensioneNET;
    // Ricevo la dimensione del nome utente del mittente
    riceviPacchetto(sd, (void*)&dimensioneNET, sizeof(uint16_t));
    dimensione = ntohs(dimensioneNET);
    // Ricevoil nome utente del mittente
    riceviPacchetto(sd, (void*)mittente, dimensione);
    // Ricevo il timestamp
    riceviPacchetto(sd, (void*)timestamp, sizeof(time_t));
    (*timestamp) = ntohl((*timestamp));
    // Ricevo il numero di messaggi 
    riceviPacchetto(sd, (void*)numeroChat, sizeof(uint16_t));
    *numeroChat = ntohs((*numeroChat));
}