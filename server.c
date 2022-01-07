#include "utility/all.h"
#include "utility/gestoreConnessioni.h"
#include "utility/gestoreMessaggi.h"


/*
    Alcune variabili globali necessarie per la gestione del server. Purtroppo l'uso di variabili gloabli in questo modo
    mette dei paletti in merito alla possibilità di un server multiprocesso, ma in ogni caso, avendo a che fare con i file,
    si sarebbe presentato comunque il problema dell'accesso in mutua esclusione. Per tale ragione ho preferito comunque 
    usare delle variabili globali, per semplificare alcune operazioni.
*/

struct connessione* listaConnessioni = NULL;    // - Lista per le connessioni. Ad ogni connessione corrisponde un peer
                                                //   connesso, comprensivo di tutti i dati necessari per la sua gestione. 
struct entryAccessoUtente* registro = NULL;     // - Lista per il registro.
char fileName[MAX_LEN_UTN+20];                  // - Vettore contentente ./fileServer/server.txt, contente i messaggi
                                                //   che il server ha memorizzato e che devono ancora esssere inoltrati
                                                //   in questo modo si usano le funzioni sui messaggi sia per device che per
                                                //   server.
fd_set master;                                  // - Set master per l'IO multiplexing (in più punti del codice si dovranno)
                                                //   inserire socket da monitorare)
int listener;                                   // - socket listener.



/****************************************************************************
 *  Struttura dati e funzioni per la gestione del registro degli accessi    *
 ****************************************************************************/

struct entryAccessoUtente {
    char                        nomeUtente[MAX_LEN_UTN+1];
    uint16_t                    porta;
    time_t                      timestampLogin;
    time_t                      timestampLogout;
    struct entryAccessoUtente*  next;
};

// Funzione per l'apertura del registro degli accessi dal file ./fileServer/registroAccessi.txt
// La funzione prende i record presenti nel file e crea una lista di entryAccessoUtente
void apriRegistroAccessi(){
    FILE*                       file;
    char                        nomeUtente[MAX_LEN_UTN+1];
    uint16_t                    porta;
    time_t                      timestampLogin;
    time_t                      timestampLogout;
    struct entryAccessoUtente*  temp = NULL, *last = registro;

    file = fopen("./fileServer/registroAccessi.txt","r");

    if(file == NULL){
        printf("Errore! Non è stato possibile aprire il registro degli accessi!\n");
        exit(1);
    }

    // Ad ogni riga nel file corrisponde un entry, comprensivo di nomeUtente, 
    // porta di comunicazione, timestampLoigin e timestampLogout. 
    while(fscanf(file, "%s %hd %ld %ld", nomeUtente, &porta, &timestampLogin, &timestampLogout) != EOF){
        temp = (struct entryAccessoUtente*) malloc(sizeof(struct entryAccessoUtente));
        strcpy(temp->nomeUtente,nomeUtente);
        temp->porta = porta;
        temp->timestampLogin = timestampLogin;
        temp->timestampLogout = timestampLogout;
        temp->next = NULL;

        if(!registro){
            registro = temp;
            last = registro;
        }
        else{
            last->next = temp;
            last = temp;
        }
    }
    fclose(file);
}

// Funzione per salvare sul file il contenuto del registro degli accessi
// Si userà ogni volta che avviene una modifica al registro, in particolare quando si rileva un login,
// un logout, o quando un device invia il timestamp precedentemente salvato mentre il server era offline
void salvaRegistroAccessi(){
    FILE*                       file;
    struct entryAccessoUtente*  temp = registro;

    file = fopen("./fileServer/registroAccessi.txt","w");
    if(file == NULL){
        printf("Errore! Non è stato possibile aprire il registro degli accessi!\n");
        exit(1);
    }
    
    while(temp){
        fprintf(file, "%s %hd %ld %ld\n", temp->nomeUtente, temp->porta, temp->timestampLogin, temp->timestampLogout);
        temp = temp->next;
    }
    fclose(file);
}

// Funzione per ottenere l'ultimo timestamp di logine di un utente (si usa nella chiamata a list).
time_t ottieniTimestampConnessione(char* nomeUtente){
    struct entryAccessoUtente* temp =registro;
    time_t                     tsLogin=0;
    while(temp){
        if(strcmp(temp->nomeUtente,nomeUtente)==0){
            tsLogin = temp->timestampLogin;
        }
        temp = temp->next;
    }
    return tsLogin;
}

// Semplice funzione che rimuove la memoria utilizzata per la lista allocata 
void chiudiRegistroAccessi(){
    struct entryAccessoUtente* p = registro;
    struct entryAccessoUtente* q;
    while(p != NULL){
        q = p->next;
        free(p);
        p = q;
    }
}

// Funzione che aggiunge una entry al registro degli accessi secondo i parametri inseriti. In particolare
// prima crea il nodo da aggiungere sul fondo della lista degli accessi (in modo da tenerla ordinata),
// poi chiama la funzione per salvare il contenuto della lista sul file. Questa operazione viene svolta
// più volte, e potrebbe comportare overhead, ma in tal modo in caso di chiusura inaspetta del server
// i dati restano consistenti. 
void aggiungiEntryRegistroAccessi(char* username, uint16_t porta, time_t tsLogin){
    struct entryAccessoUtente*      newNode, *temp = registro;
    newNode = (struct entryAccessoUtente*) malloc(sizeof(struct entryAccessoUtente));
    strcpy(newNode->nomeUtente,username);
    newNode->porta = porta;
    newNode->timestampLogin = tsLogin;
    newNode->timestampLogout = 0;
    newNode->next = NULL;
    if(!registro){
        registro = newNode;
    }
    else{
        while(temp->next){
            temp = temp->next;
        }
        temp->next = newNode;
    }
    salvaRegistroAccessi();
}

// Funzione per aggiungere un timestamp di logout ad una entry della lista degli accessi. Dato un nome,
// cerca il primo record che non presenta un timestamp di uscita e lo modifica. Si può quindi usare sia 
// nel caso in cui è il server a riconoscere il logout del device, sia quando questo comunica al server
// un logout precedentemente memorizzato. 
void modificaEntryRegistroAccessi(char* username, time_t timestampLogout){
    struct entryAccessoUtente* temp = registro;
    while(temp){
        if(strcmp(username, temp->nomeUtente)==0 && temp->timestampLogout==0){
            break;
        }
        else{
            temp = temp->next;
        }
    }
    // Per qualche errore potrebbe accadere che il record richiesto non esista. 
    if(temp){
        temp->timestampLogout = timestampLogout;
        salvaRegistroAccessi();
    }
}

/***********************************************************
 *  Gestione delle operazioni di login, logout e signup    *
 ***********************************************************/

// Funzione per effettuare il login dato uno username e una password.
int login(char* username, char* password, uint16_t portaDevice){
    FILE*   file = fopen("./fileServer/utenti.txt","r");
    char    nomeUtenteLetto[MAX_LEN_UTN+1];
    char    passwordLetta[MAX_LEN_PSW+1];

    if(file == NULL){
        printf("Errore! Non è stato possibile aprire il registro degli accessi!\n");
        exit(1);
    } 

    // Cerco, nel file utenti.txt, una riga contenenti il dato username e password. Per semplicità i dati sono
    // passati in chiaro. Volendo realizzare le cose in maniera più sicura, nei limiti delle funzioni disponibili,
    // potremmo scambiare l'hash della password e memorizzare la stessa in fase di signup.
    while(fscanf(file, "%s %s", nomeUtenteLetto, passwordLetta) != EOF){
        if(strcmp(nomeUtenteLetto,username)==0 && strcmp(passwordLetta,password) == 0){
            fclose(file);
            return 0;
        }
    }
    // Arrivati a questo punto, il login non è andato a buon fine
    fclose(file);
    return 1;
}

// Funzione simile a login, che si occupa di ricercare l'esistenza di un utente dato il suo username.
// Restituisce 0 in caso di successo, 1 in caso di assenza. 
int esisteUtente(char* username){
    FILE*   file = fopen("./fileServer/utenti.txt","r");
    char    nomeUtenteLetto[MAX_LEN_UTN+1];
    char    passwordLetta[MAX_LEN_PSW+1];

    if(file == NULL){
        printf("Errore! Non è stato possibile aprire il registro degli accessi!\n");
        exit(1);
    } 

    while(fscanf(file, "%s %s", nomeUtenteLetto, passwordLetta) != EOF){
        if(strcmp(nomeUtenteLetto,username)==0 ){
            fclose(file);
            return 0;
        }
    }
    fclose(file);
    return 1;
}

// Funzione per realizzare il signup al servizio. Prima si cerca che non ci sia nessuno con lo stesso username
// già registrato al servizio, dopo di che si aggiunge una riga sul fondo del file utenti.txt
int signup(char* username, char* password){
    FILE*   file = fopen("./fileServer/utenti.txt","r+");
    char    nomeUtenteLetto[MAX_LEN_UTN+1];
    char    passwordLetta[MAX_LEN_PSW+1];

    if(file == NULL){
        printf("Errore! Non è stato possibile aprire il registro degli accessi!\n");
        exit(1);
    } 

    while(fscanf(file, "%s %s", nomeUtenteLetto, passwordLetta) != EOF){
        if(strcmp(nomeUtenteLetto,username) == 0) {
            return -1;
        }
    }

    fprintf(file,"%s %s\n",username,password);
    fclose(file);
    return 0;
}   

// Il logout consiste semplicemente nell'aggiornare una entry nel registro degli accessi
// (la connessione con il device sarà chiusa assieme all'suo di questa funzione)
void logout(char* username){
    modificaEntryRegistroAccessi(username,time(NULL));
}

// Dalla parte del server, una chiamata ad hanging deve ricercare quanti sono i messaggi indirizzati
// al device in oggetto, distinguerli per mittente e inviare un messaggio in modo opportuno.
// Se il server invia il comando HANGING_NUOVA_RIGA allora successivamente invia una delle righe da mostrare,
// comprensivo di username, timestamp del messaggio più recente, e numero dei messaggi (funzione invioRigaHanging).
// La funzione ha senso per il modo con cui ordino i messaggi da inoltrare nel file server.txt, secondo l'ordine:
//   mittente->destinatario->timestamp
// In questo modo quando non ho lo stesso mittente del messaggio precedente, so che non ne avrò più per lo stesso destinatario,
// e l'ultimo messaggio che incontro della coppia è quello più recente. 
void hangingHandler(struct connessione* con){
    char        mittente[MAX_LEN_UTN+1];
    uint16_t    numeroMessaggi = 0;
    uint8_t     risposta;
    time_t      timestamp;
    struct      messaggio* temp = listaMessaggi;

    // Scorro tutti i messaggi
    while(temp){
        // Ne trovo uno che ha come destinatario l'utente che ha eseguito hanging, e come stato 0, a dire che
        // deve essere ancora inoltato.
        if(strcmp(temp->destinatario, con->nomeUtente)==0 && temp->stato == 0){
            // Caso in cui è il primo messaggio incontrato di tale utente. 
            if(numeroMessaggi==0){  
                strcpy(mittente, temp->mittente);
                timestamp = temp->timestamp;
                numeroMessaggi = numeroMessaggi+1;
                temp = temp->next;
            }
            // Caso in cui non è il primo messaggio incontrato dello stesso utente. Aggiorno il contatore
            // e il timestamp
            else if(strcmp(temp->mittente,mittente)==0){
                numeroMessaggi = numeroMessaggi+1;
                timestamp = temp->timestamp;
                temp = temp->next;
            }
            // Caso in cui non ci sono più messaggi dallo stesso autore per lo stesso destinatario da incontrare.
            // Si può inviare la riga
            else{
                risposta = HANGING_NUOVA_RIGA;
                inviaPacchetto(con->socketDescriptor, (void*)&risposta, sizeof(uint8_t));
                invioRigaHanging(con->socketDescriptor, mittente, timestamp,numeroMessaggi);
                numeroMessaggi = 0;
            }
        }
        else{
            temp = temp->next;
        }
    }
    // Quando ho terminato di scorrere la lista, potrei avere ancora una riga da inviare.
    if(numeroMessaggi != 0){
        risposta = HANGING_NUOVA_RIGA;
        inviaPacchetto(con->socketDescriptor, (void*)&risposta, sizeof(uint8_t));
        invioRigaHanging(con->socketDescriptor, mittente, timestamp,numeroMessaggi);
    }
    // Segnalo al device la fine dell'invio dei pacchetti. 
    risposta = HANGING_FINE;
    inviaPacchetto(con->socketDescriptor, (void*)&risposta, sizeof(uint8_t));
}

// Funzione per inviare un ACK di avvenuta lettura dei messaggi inoltrati da mittente verso destinatario. 
// Gli ACK in questione sono cumulativi, visto che usando la show il device osserva tutti i messaggi precedemente
// inoltrati dal mittente. 
void inviaACKlettura(char* mittente, char* destinatario){
    struct      connessione* conMittente;
    uint8_t     comando; 
    uint16_t    dimensione, dimensioneNET;

    // Ricerco il mittente, colui che deve ricevere l'ACK
    conMittente = ottieniConnessioneDevice(listaConnessioni,mittente);
    // Nel caso non sia connesso, allora aggiungo l'ACK alla lista di ack e salvo la lista nel file ACKlettura.txt,
    // dove si tiene traccia prorpio di questi dati. 
    if(conMittente==NULL){
        aggiungiACK(mittente, destinatario);
        salvaListaACK("./fileServer/ACKlettura.txt");
    }
    // In caso contrario colui che deve ricevere l'ACK è collegato: prima si invia il comando per l'invio, poi si invia
    // il nome del device che ha ricevuto i messaggi. 
    else{
        comando = COMANDO_ACK;
        inviaPacchetto(conMittente->socketDescriptor, (void*)&comando, sizeof(uint8_t));
        dimensione = strlen(destinatario)+1;
        dimensioneNET =htons(dimensione);
        inviaPacchetto(conMittente->socketDescriptor, (void*)&dimensioneNET, sizeof(uint16_t));
        inviaPacchetto(conMittente->socketDescriptor, (void*)destinatario, dimensione);
    }
}

// Funzione per la gestione del comando show. 
void showHandler(struct connessione* con){
    struct messaggio*   temp = listaMessaggi;
    uint16_t            dimensione;
    char                mittente[MAX_LEN_MSG+1];
    uint8_t             comando;
    int                 i=0;

    memset(mittente,0,MAX_LEN_MSG+1);
    //Si riceve innanzitutto il nome di colui di cui si vogliono trovare i messaggi. 
    riceviPacchetto(con->socketDescriptor, (void*)&dimensione, sizeof(uint16_t));
    dimensione = ntohs(dimensione);
    riceviPacchetto(con->socketDescriptor, (void*)mittente, dimensione);

    // Si scorrono i messaggi. Se se ne trova uno che ha come destinatario il device della richeista, come mittente
    // il device richiesto e come stato 0 (a dire che non è ancora stato inoltato) si invia, usando per prima cosa il comando
    // SHOW_NUOVO_MESSAGIO e la stessa funzione per inviare i messaggi. Si pone il suo stato a 2, per segnalare che è stato
    // inoltrato. 
    while(temp){
        if(strcmp(temp->destinatario, con->nomeUtente)==0 && strcmp(temp->mittente, mittente)==0 && temp->stato == 0){
            i=i+1;
            comando = SHOW_NUOVO_MESSAGGIO;
            inviaPacchetto(con->socketDescriptor, (void*)&comando, sizeof(uint8_t));
            inviaMessaggio(con->socketDescriptor, temp->mittente, temp->destinatario, temp->testo, temp->timestamp);
            temp->stato = 2;
        }
        temp = temp->next;
    }
    // Arrivati a questo punto, si invia il comando per cui non ci sono più messaggi da inviare
    comando = SHOW_FINE;
    inviaPacchetto(con->socketDescriptor, (void*)&comando, sizeof(uint8_t));
    // Se si è mandato almeno un messaggio, si gestisce l'ACK verso il mittente dei messaggi. 
    if(i!=0){
        aggiornaFileListaMessaggi(fileName,1);
        inviaACKlettura(mittente, con->nomeUtente);
    }
}

// Funzione che viene chiamata all'interno della select ogni volta che c'è bisongo di gestire un socket che non sia
// il listener (su cui il server accetta le richieste di connessione) o STDIN. Per prima cosa si riceve il comando
// da parte del device, e sulla base di quello si entra nei diversi blocchi if per capire come gestire la richiesta. 
void connectionHandler(int sd){
    uint8_t         comando, risposta;
    char            messaggio[MAX_LEN_MSG];
    char            mittente[MAX_LEN_UTN];
    char            destinatario[MAX_LEN_UTN];
    time_t          timestamp;
    uint16_t        portaDestinatario, dimensione;
    int             ret; 
    struct          connessione* conn = NULL, *conDestinatario = NULL, *temp;
    struct          ack* acktosend = NULL;

    memset(messaggio, 0, LEN_BUFFER); 
    memset(mittente, 0, MAX_LEN_UTN); 
    memset(destinatario, 0, MAX_LEN_UTN); 
    
    // Ottengo la connessione del device. Visto l'uso della select, la lista connessione è sempre
    // consistente con i peer effettivamente connessi al server. 
    conn = ottieniConnessioneDeviceSD(listaConnessioni, sd);
    // Caso inusuale ma che è bene trattare.
    if(conn == NULL){
        return;
    }

    // Si ricevono gli 8 bit di comando, come da protocollo nell'interazione con il server
    ret = riceviPacchetto(sd, (void*)&comando, sizeof(uint8_t));

    // Se la recv resituisce 0, allora il device si è disconnnesso. Si gestisce di conseguenza 
    // mantenendo consistenti le diverse strutture dati e smettendo di controllare il socket associato
    // (motivo per cui, ad esempio, è risultato comodo avere il SET master globale).
    if(ret==0){
        logout(conn->nomeUtente);
        chiudiConnessione(&listaConnessioni,sd);
        FD_CLR(sd, &master);
        return;
    }
    // Comando per ricevere l'ultimo logout salvato da parte del device mentre il server era offline.
    if(comando == COMANDO_TSOUT){
        riceviPacchetto(sd, (void*)&timestamp, sizeof(time_t));
        modificaEntryRegistroAccessi(conn->nomeUtente, ntohl(timestamp));
        return;
    }
    // Comando di hanging
    else if(comando == COMANDO_HANGING){
        hangingHandler(conn);
    }
    // Con COMANDO_ACK il device chiede, con riferimento ad un altro utente, di inviare gli ack di avvenuta
    // lettura dei messaggi inoltrati, come da specifica. Il server riceve il nome dell'utente in questione,
    // e cerca degli ACK nella lista che corrispondano alla coppia mittente-destinatario. Se esiste invia
    // tale ACK.   
    else if(comando == COMANDO_ACK){
        riceviPacchetto(sd, (void*)&dimensione, sizeof(uint16_t));
        dimensione = ntohs(dimensione);
        riceviPacchetto(sd, (void*)destinatario, dimensione);
        acktosend =  trovaACK(conn->nomeUtente, destinatario);
        if(acktosend == NULL){
            return;
        }
        acktosend->inviato = 1; 
        inviaACKlettura(conn->nomeUtente, destinatario);
        salvaListaACK("./fileServer/ACKlettura.txt");
    }
    // Quando un device A invia tale comando al server, vuol dire che vuole mandare un messaggio a B
    // senza che tra i due ci sia ancora una connessione. IL server fa allora da tramite, rigirando il 
    // messaggio e scambiando il numero di porta nel caso in cui B sia online, salvando in locale il
    // messaggio se è offline. 
    else if(comando == COMANDO_MESSAGGIO){
        riceviMessaggio(sd, mittente, destinatario, messaggio, &timestamp);
        ret = esisteUtente(destinatario);
        // caso in cui il destinatario del messaggio non esiste. 
        if(ret == 1){
            risposta = UTENTE_INESISTENTE;
            inviaPacchetto(sd, (void*)&risposta, sizeof(uint8_t));
        }
        // Il destinatario essite
        else{
            // Ottengo la connessione del destinatario
            conDestinatario = ottieniConnessioneDevice(listaConnessioni, destinatario);
            //Il destinatario non è connesso 
            if(conDestinatario == NULL){
                // Comunico la meorizzazione del messaggio e aggiungo il messaggio nella lista dei messaggi salvati. 
                risposta = RISPOSTA_MEMORIZZAZIONE;
                inviaPacchetto(sd, (void*)&risposta, sizeof(uint8_t));
                aggiungiMessaggio(mittente, destinatario, 0, timestamp, messaggio, fileName, ORDINE_SERVER);
            }
            // Il destinatario è connesso
            else{
                // Comunico l'avvenuto recapito del messaggio
                risposta = RISPOSTA_RECAPITO;
                inviaPacchetto(sd, (void*)&risposta, sizeof(uint8_t));
                // Ottengo la porta del destinatario e la invio al mittente, in modo che questo possa
                // richiedere all'altro una connessione TCP.
                portaDestinatario = htons(conDestinatario->porta);
                inviaPacchetto(sd, (void*)&portaDestinatario, sizeof(uint16_t));
                // Invio al destinatario il messaggio, con lo stesso protocollo usato dal device A per inviare
                // il messaggio al server.
                comando = COMANDO_MESSAGGIO;
                inviaPacchetto(conDestinatario->socketDescriptor, (void*)&comando, sizeof(uint8_t));
                inviaMessaggio(conDestinatario->socketDescriptor, mittente, destinatario, messaggio, timestamp);
            }
        }
        return;
    }
    // Richiesta del comando show
    else if(comando==COMANDO_SHOW){
        showHandler(conn);
    }
    // Comando inviato dal device al server quando vuole sapere la lista degl utenti online 
    // che possono essere aggiunti alla chat. Il protocollo prevede il segnale di 'nuova riga'
    // per ogni utente online inviato, di cui si invia dimensione del nome e nome. 
    else if(comando == COMANDO_LISTA){
        temp = listaConnessioni;
        while(temp){
            comando = NUOVA_RIGA_LISTA;
            inviaPacchetto(sd, (void*)&comando, sizeof(uint8_t));
            dimensione = strlen(temp->nomeUtente)+1;
            dimensione = htons(dimensione);
            inviaPacchetto(sd, (void*)&dimensione, sizeof(uint16_t));
            inviaPacchetto(sd, (void*)temp->nomeUtente, ntohs(dimensione));
            temp = temp->next;
        }
        // Dopo aver scorso la lista, si invia il pacchetto che indica la fine della comunicazione. 
        comando = FINE_LISTA;
        inviaPacchetto(sd, (void*)&comando, sizeof(uint8_t));
    }
    // Comando usato quando il client vuole aggiungere un device ad una conversazione. Per realizzare l'operazione
    // ha bisogno del suo numero di porta, che gli viene recapitato dal server. Nel caso in cui il server invii 0,
    // l'utente richiesto non è connesso (o non esiste, ma non ha senso distinguere, in ogni caso non si può inviare nulla).
    else if(comando == COMANDO_NUOVA_CONNESIONE){
        riceviPacchetto(sd, (void*)&dimensione, sizeof(uint16_t));
        dimensione = ntohs(dimensione);
        riceviPacchetto(sd, (void*)destinatario, dimensione);
        temp = ottieniConnessioneDevice(listaConnessioni, destinatario);
        if(temp == 0){
            portaDestinatario = 0;
        }
        else{
            portaDestinatario = temp->porta;
        }
        portaDestinatario = htons(portaDestinatario);
        inviaPacchetto(sd, (void*)&portaDestinatario, sizeof(uint16_t));
    }
}

void mostraHome(){
    system("clear");
    printf("****************** SERVER STARTED ******************\n");
    printf("Digita un comando:\n\n");
    printf("1) help --> mostra i dettagli dei comandi\n");
    printf("2) list --> mostra un elenco degli utenti connessi\n");
    printf("3) esc --> chiude il server\n");
}

void helpHandler(){
    system("clear");
    printf("Dettagli sui comandi: \n");
    printf("  -> Il comando help è quello selezionato.\n");
    printf("     Permette di ottenere questa descrizione dei comandi\n\n");
    printf("  -> Il comando list mostra gli utenti attualmente online\n\n");
    printf("  -> Il comando esc termina il server, inviando la notifica di\n");
    printf("     tale terminazione a tutt i device attualmente collegati\n");
    sleep(5);
    mostraHome();
}

// Funzione per la realizzazione del comando list. Scorre le connessioni attive e per ciascuna
// stampa una riga nel formato
//      nomeUtente*timestampConnessione*porta
void listHandler(){
    struct connessione* temp = listaConnessioni;
    system("clear");

    if(listaConnessioni==NULL){
        printf("Non ci sono attualmente utenti online!\n");
    }
    else{
        printf("Utenti attualmente online: \n\n");
        while(temp){
            printf("%s*%ld*%hd\n",temp->nomeUtente,ottieniTimestampConnessione(temp->nomeUtente),temp->porta);
            temp = temp->next;
        }
    }
    sleep(5);
    mostraHome();
}

// Funzione per il comando esc: termina tutte le connessioni e dealloca la memoria usata per le diverse
// struttura dati. 
void escHandler(){
    int ret;
    terminaConnessioni(&listaConnessioni);

    ret = close(listener);
    errorHandler(ret, NULL);

    chiudiListaACK();
    chiudiListaMessaggi();

    printf("Bye...\n");
    exit(0);
}

// Funzione che si attiva quando la listen reagisce ad una modifica del file descriptor stdin. 
void inputHandler(){
    char buffer[LEN_BUFFER];
    int i=0;

    memset(buffer, 0, LEN_BUFFER);
    fgets(buffer, LEN_BUFFER, stdin);

    // Sostituisce al carattere \n prelevato da fgets \0
    while(1){
        if(buffer[i]=='\n'){
            buffer[i]='\0';
            break;
        }
        i=i+1;
    }
    if(     strcmp(buffer,"help")==0)   helpHandler();
    else if(strcmp(buffer,"list")==0)   listHandler();
    else if(strcmp(buffer,"esc")==0)    escHandler();
    else{
        printf("Comando inserito non valido!\n");
        sleep(3);
        mostraHome();
    }
}

int main(int argc, char* argv[]){
    fd_set      read_fds;
    uint16_t    dimensioneMessaggio, dimensioneMessaggioNET, portaServer, portaDevice;
    uint8_t     comando, risposta;
    int         fdmax, ret, newfd, addrlen, i;
    struct      sockaddr_in serverAddr; 
    struct      sockaddr_in cl_addr; 
    char        password[MAX_LEN_PSW];
    char        nomeUtente[MAX_LEN_UTN];
    char        buffer[LEN_BUFFER];


    setvbuf (stdout, NULL, _IONBF, BUFSIZ);

    FD_ZERO(&master); 
    FD_ZERO(&read_fds);

    // Si preleva la porta inserita dall'utente
    if(argc==2) portaServer = atoi(argv[1]);
    else portaServer = 4242;

    // Si apre il socket di ascolto 
    listener = socket(AF_INET, SOCK_STREAM, 0);
    errorHandler(listener,NULL);
    // Si prepara l'indirizzo 
    memset(&serverAddr, 0, sizeof(serverAddr)); 
    serverAddr.sin_family = AF_INET ;
    serverAddr.sin_port = htons(portaServer);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    ret = bind(listener, (struct sockaddr*)& serverAddr, sizeof(serverAddr));
    errorHandler(ret,NULL);
    ret = listen(listener, 10);
    errorHandler(ret,NULL);

    FD_SET(listener,     &master);
    FD_SET(STDIN_FILENO, &master); //stdin
    fdmax = listener;
    
    apriRegistroAccessi();

    // Si apre la lista dei messaggi e degli ack. 
    strcpy(fileName,"./fileServer/server.txt");
    apriListaMessaggi(fileName, ORDINE_SERVER);
    apriListaACK("./fileServer/ACKlettura.txt");

    // Si mostra la home all'avvio 
    mostraHome();

    for(;;){
        read_fds = master; 
        select(fdmax + 1, &read_fds, NULL, NULL, NULL);
        for(i=0; i<=fdmax; i++) { 
            if(FD_ISSET(i, &read_fds)) {  
                // Caso in cui il socket che ha risvegliato la listen è quello di ascolto. Questo avviene quando un 
                // device cerca di fare la signup (dopo la quale la connessione è chiusa) o la login (dopo la quale
                // la connessione resta aperta).
                if(i == listener) { 
                    addrlen = sizeof(cl_addr); 
                    newfd = accept(listener, (struct sockaddr *)&cl_addr, (socklen_t*)&addrlen) ;

                    memset(buffer, 0, LEN_BUFFER); 
                    memset(nomeUtente, 0, MAX_LEN_UTN); 
                    memset(password, 0, MAX_LEN_PSW); 

                    riceviPacchetto(newfd, (void*)&comando, sizeof(uint8_t));
                    //Per effettuale il signup, si ricevono, nell'ordine:
                    //dimensione del nome utente, nome utente, dimensione password, password.
                    if(comando == COMANDO_SIGNUP){
                        riceviPacchetto(newfd, (void*)&dimensioneMessaggioNET, sizeof(uint16_t));
                        dimensioneMessaggio = ntohs(dimensioneMessaggioNET);
                        riceviPacchetto(newfd, (void*)nomeUtente, dimensioneMessaggio);
                        riceviPacchetto(newfd, (void*)&dimensioneMessaggioNET, sizeof(uint16_t));
                        dimensioneMessaggio = ntohs(dimensioneMessaggioNET);
                        riceviPacchetto(newfd, (void*)password, dimensioneMessaggio);

                        if(signup(nomeUtente,password)==0){
                            risposta = SUCCESS_CODE;
                        }
                        else{
                            risposta = FAIL_CODE;
                        }
                        inviaPacchetto(newfd, (void*)&risposta, sizeof(uint8_t));
                        // Sia in caso di successo che di fallimento, si chiude la connessione
                        close(newfd);
                    }
                    //Per effettuale la logine, si ricevono, nell'ordine:
                    //dimensione del nome utente, nome utente, dimensione password, password.
                    else if(comando == COMANDO_LOGIN){
                        riceviPacchetto(newfd, (void*)&portaDevice, sizeof(uint16_t));
                        portaDevice = ntohs(portaDevice);
                        riceviPacchetto(newfd, (void*)&dimensioneMessaggioNET, sizeof(uint16_t));
                        dimensioneMessaggio = ntohs(dimensioneMessaggioNET);
                        riceviPacchetto(newfd, (void*)nomeUtente, dimensioneMessaggio);
                        riceviPacchetto(newfd, (void*)&dimensioneMessaggioNET, sizeof(uint16_t));
                        dimensioneMessaggio = ntohs(dimensioneMessaggioNET);
                        riceviPacchetto(newfd, (void*)password, dimensioneMessaggio);

                        if(login(nomeUtente,password,portaDevice)==0){
                            risposta = SUCCESS_CODE;
                        }
                        else{
                            risposta = FAIL_CODE;
                        }
                        inviaPacchetto(newfd, (void*)&risposta, sizeof(uint8_t));
                        // IN caso di successo, si gestiscono opportunamente le strutture dati: si aggiorna il registro
                        // degli accessi, si inserisce il socket tra quelli da monitorare e si aggiunge la connessione
                        // alla lista associata. 
                        if(risposta==SUCCESS_CODE){
                            aggiungiEntryRegistroAccessi(nomeUtente, portaDevice, time(NULL));
                            FD_SET(newfd, &master); 
                            aggiungiConnessione(&listaConnessioni,portaDevice,newfd,nomeUtente,0);
                            if(newfd>fdmax){
                                fdmax=newfd;
                            }
                        }
                    }
                }
                if(i==STDIN_FILENO){
                    inputHandler();
                }
                // In tutti gli altri casi si gestisce la singola richiesta con connectionHandler
                else{
                    connectionHandler(i);
                }
            } 
        }
    }
}