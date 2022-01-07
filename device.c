#include "./utility/all.h"
#include "./utility/gestoreConnessioni.h"
#include "./utility/gestoreMessaggi.h"
#include "./utility/scambioFile.h"

/***********************************************************
 *  Variabili globali per la gestione dell'applicazione    *
 ***********************************************************/

int posizioneTerminale;                         // Permette di memorizzare l'ultima posizone in cui si era 
                                                // dell'applicazione, distinguendo tra home e chat. Ad esempio,
                                                // dopo aver ricevuto una notifica, fa comodo poter toranre al
                                                // punto in cui si era.

char chatAttuale[MAX_LEN_UTN];                  // Mantiene il nome dell'utente con cui si è attualmente aperta
                                                // una chat.

char nomeUtenteLog[MAX_LEN_UTN];                // Mantiene il nome dell'utente con cui si è fatto il login.

char fileName[MAX_LEN_UTN+20];                  // Come nel caso del server, si usa tale variabile per mantenere il nome
                                                // del file contenente la lista dei messaggi (si scrive una volta,
                                                // al login dell'utente, poi non si modifica più).

struct connessione* listaConnessioni = NULL;    // Mantiene la lsita delle connessioni attive 

fd_set master;                                  // master per l'uso della select. Anche in questo caso, i file descriptor
int fdmax;                                      // si dovranno modificare in punti diversi del codice, quindi fa comodo
int listener;                                   // avere tale variabile gloable, così come le variabili fdmax e listener
                                                // (si deve chiudere il socket di ascolto al termine dell'applicazione).
uint16_t portaConnessione;

// Funzione per visualizzare un messaggio sullo schermo, e lasciarlo in vista per tre secondi 
void alert(char* msg){
    printf("%s",msg);
    sleep(3);
}

// Funzione per ottenere il timestamp attuale 
time_t timestampAttuale(){
    time_t now = time(NULL);
    return now;
}

// Funzione per mostrare una notifica a schermo del tipo
// **********************************
//
//  Testo notifica
//
// **********************************

void mostraChat();
void mostraHome();

void mostraNotifica(const char* msg, const char* argomento, int cancellaInput){
    if(cancellaInput==1){
        printf("\b\b\b\b\b");
    }
    if(argomento == NULL){
        printf("%s", msg);
    }
    else{
        printf(msg, argomento);
    }
    if(cancellaInput==1){
        printf(">> ");
    }
}

void mostraNotificaFullScreen(char* msg){
    system("clear");
    printf("\n\n\n**********************************\n\n");
    printf("%s",msg);
    printf("\n**********************************\n\n");
    sleep(3);
    if(posizioneTerminale==POSIZIONE_TERM_HOME) mostraHome();
    else if(posizioneTerminale==POSIZIONE_TERM_CHAT) mostraChat();
    else mostraHome();
}

// Funzioe per sostituire, all'interno di un buffer, il primo \n
// che si incontra con \0.
void sostituisciNL(char* buffer){
    int i=0; 
    while(1){
        if(buffer[i]=='\n'){
            buffer[i]='\0';
            return;
        }
        i=i+1;
    }   
}

// Funzione per cercare un nome utente nella rubrica
int RicercaInRubrica(char* nomeUtente){
    char nomeFile[MAX_LEN_UTN+40];
    char nomeLetto[MAX_LEN_UTN];
    FILE* file;

    strcpy(nomeFile,"./ListaMessaggi/Rubrica-");
    strcat(nomeFile,nomeUtenteLog);
    strcat(nomeFile,".txt");

    // Se non esistente, creo il file
    file = fopen(nomeFile,"a");
    if(file==NULL){
        printf("Impossibile aprire la rubrica");
        return 0;
    }
    fclose(file);

    file = fopen(nomeFile,"r");
    while(fgets(nomeLetto,MAX_LEN_UTN,file)!=NULL){
        sostituisciNL(nomeLetto);
        // Trovato il nome richiesto
        if(strcmp(nomeLetto,nomeUtente)==0){      
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

// Funzione per aggiungere alla rubrica lo username di un utente.
void aggiungiARubrica(char* nomeUtente){
    char nomeFile[MAX_LEN_UTN+40];
    char nomeLetto[MAX_LEN_UTN];
    FILE* file;

    strcpy(nomeFile,"./ListaMessaggi/Rubrica-");
    strcat(nomeFile,nomeUtenteLog);
    strcat(nomeFile,".txt");

    file = fopen(nomeFile,"a");
    if(file==NULL){
        printf("Impossibile aprire la rubrica");
        return;
    }
    fclose(file);

    file = fopen(nomeFile,"r");
    while(fgets(nomeLetto,MAX_LEN_UTN,file)!=NULL){
        sostituisciNL(nomeLetto);
        if(strcmp(nomeLetto,nomeUtente)==0){        // Se il nome utente è già presente, non lo aggiungo
            fclose(file);
            return;
        }
    }
    fclose(file);
    file = fopen(nomeFile,"a");
    fputs(nomeUtente,file);                         // Aggiungo il nome utente in fondo alla rubrica
    fputs("\n",file);
    fclose(file);
    return;
}


// Funzione per effettuare il login o il signup (le differenze erano tanto minime che non valeva la pena scriverne due)
int logHandler(uint8_t comando, uint16_t portaDevice, uint16_t portaServer, char* nomeUtente, char* password){
    uint16_t    dimensioneMessaggio, dimensioneMessaggioNET;
    uint8_t     risposta;
    char        buffer[LEN_BUFFER];
    int         sd;

    memset(buffer, 0, LEN_BUFFER); 

    // Apre la connessione TCP con la porta specificata
    sd = apriConnessioneTCP(portaServer);
    mostraNotifica("[Aperta connessione con il server]\n",NULL, 0);
    if(sd == -1){
        alert("Non è stato possibile connettersi al server!\nIl server potrebbe non essere attualmente connesso.\nRiprovare!\n");
        return -1;
    }
    // Invio il comando, sia esso di login o signup
    inviaPacchetto(sd, (void*)&comando, sizeof(uint8_t));
    // Nel caso in cui il comando era di login, si invia per prima cosa la porta con cui il device si è collegato
    if(comando == COMANDO_LOGIN){
        portaDevice = htons(portaDevice);
        inviaPacchetto(sd, (void*)&portaDevice, sizeof(uint16_t));
    }
    // Si invia la dimensione del nomeUtente 
    dimensioneMessaggio = strlen(nomeUtente)+1;
    dimensioneMessaggioNET = htons(dimensioneMessaggio);
    inviaPacchetto(sd, (void*)&dimensioneMessaggioNET, sizeof(uint16_t));
    // Si invia il nome utente
    inviaPacchetto(sd, (void*)nomeUtente, dimensioneMessaggio);
    // Si inva la dimensione della password
    dimensioneMessaggio = strlen(password)+1;
    dimensioneMessaggioNET = htons(dimensioneMessaggio);
    inviaPacchetto(sd, (void*)&dimensioneMessaggioNET, sizeof(uint16_t));
    // Si invia la password
    inviaPacchetto(sd, (void*)password, dimensioneMessaggio);
    // Si aspetta una risposta dal server
    riceviPacchetto(sd, (void*)&risposta, sizeof(uint8_t));
    // Successo con signup
    if(comando == COMANDO_SIGNUP && risposta == SUCCESS_CODE){
        alert("Signup avvenuta con successo!");
        close(sd);
    }
    // fail con signup
    if(comando == COMANDO_SIGNUP && risposta != SUCCESS_CODE){
        alert("Non è stato possibile creare l'account!\nRiprovare\n");
        close(sd);
    }
    // Fail con login
    if(comando == COMANDO_LOGIN && risposta!=SUCCESS_CODE){
        alert("Il nome utente o la password inseriti non sono corretti!\nRiprovare!\n");
        close(sd);
    }
    // Successo con login
    if(comando == COMANDO_LOGIN && risposta == SUCCESS_CODE){
        // Invece di chiudere la connessione come in tutti gli altri casi, si aggiunge la connessione del server
        aggiungiConnessione(&listaConnessioni, portaServer,sd,"\0",1);
        // Si copia il nome utete con cui si è fatto un login
        strcpy(nomeUtenteLog,nomeUtente);
        alert("Il login è avvenuto con successo!\n");
        return 0;
    }
    return 1;
}

// Funzione per inviare, nel caso in cui sia memorizzato, l'ultimo timestamp di disconnessione salvato
// mentre il server era offile
void inviaUltimoTimestamp(){
    FILE*               file;
    char                filename[MAX_LEN_UTN+30];
    time_t              timestamp;
    int                 sd, ret;
    uint8_t             comando;
    // Si apre il file, prima in append, eventualmente per crearlo.
    // Il file in questione ha come nome ts-{NomeUtente}.txt
    strcpy(filename, "./ListaMessaggi/ts-");
    strcat(filename, nomeUtenteLog);
    strcat(filename, ".txt\0");
    file = fopen(filename,"a");
    if(file == NULL){
        printf("Errore! Non è stato possibile aprire la lista dei messaggi!\n");
        exit(1);
    }
    fclose(file);
    // Si apre il file in lettura
    file = fopen(filename,"r");
    if(file == NULL){
        printf("Errore! Non è stato possibile aprire la lista dei messaggi!\n");
        exit(1);
    }
    // Si legge il timestamp in esso eventulamente salvato
    ret = fscanf(file, "%ld", &timestamp);
    fclose(file);
    // Se non è presente, allora si esce
    if(ret == EOF || timestamp == 0){
        return;
    }
    // Negli altri casi, si invia il itmestamp al server
    if(ottieniConnessioneServer(listaConnessioni)==NULL){
        return;
    }
    sd = ottieniConnessioneServer(listaConnessioni)->socketDescriptor;
    // Invio il comando per comunicare al server che sto per inviare un timestamp di logout
    comando = COMANDO_TSOUT;
    inviaPacchetto(sd, (void*)&comando, sizeof(uint8_t));
    // Invio il timestamp di logout
    timestamp = htonl(timestamp);
    inviaPacchetto(sd, (void*)&timestamp, sizeof(time_t));
    // Una volta prelevato il timestamp, si scrive 0 nel file, a dire che non c'è, al momento, nessun 
    // altro timestamp da inviare 
    file = fopen(filename,"w");
    if(file == NULL){
        printf("Errore! Non è stato possibile aprire la lista dei messaggi!\n");
        exit(1);
    }
    fprintf(file, "0");
    fclose(file);
}

// Funzione per ciclare all'interno del menu di login/signup finché non si esegue l'operazione corretta.
void pollingLog(uint16_t portaDevice){
    char        inputUtente[MAX_DIM_INPUT];
    char        azione[MAX_DIM_INPUT];
    char        nomeUtente[MAX_DIM_INPUT];
    char        password[MAX_DIM_INPUT];
    uint8_t     comando;
    uint16_t    portaServer;

    memset(inputUtente,0,MAX_DIM_INPUT);
    memset(azione,0,MAX_DIM_INPUT);
    memset(nomeUtente,0,MAX_DIM_INPUT);
    memset(password,0,MAX_DIM_INPUT);

    while(1){
        system("clear;");
        printf("***** APPLICAZIONE DI INSTANT MESSAGING *****\n");
        printf("Per procedere, entra nell'applicazione con il comando:\n");
        printf("  -> in srv_port username password\n");
        printf("Oppure registrati con il comando:\n");
        printf("  -> signup srv_port username password\n");
        printf(">> ");
        fgets(inputUtente, MAX_DIM_INPUT, stdin);

        sscanf(inputUtente, "%s %hd %s %s", azione, &portaServer, nomeUtente, password);
        // Se il comando è in, allora si effetuta una login
        if(strcmp(azione, "in")==0){
            comando = COMANDO_LOGIN;
            // Se l'operazioen di login è andata a buon fine, si esce dal loop e si esegue l'applicazione
            // con la funzione app
            if(logHandler(comando, portaDevice, portaServer, nomeUtente, password)==0){
                return;
            }
        }    
        // Se il comando è signup, si effettua un signup
        else if(strcmp(azione, "signup")==0){ 
            comando = COMANDO_SIGNUP;
            logHandler(comando, portaDevice, portaServer, nomeUtente, password);
        }
        else alert("Comando inserito non corretto, riprovare!\n");
    }
}

// Funzione per mostrare la home dell'applicazione
void mostraHome(){
    system("clear;");
    posizioneTerminale = POSIZIONE_TERM_HOME;

    printf("Benvenuto, %s!\n",nomeUtenteLog);
    printf("Usa i seguenti comandi:\n");
    printf("  -> hanging\n");
    printf("  -> show username\n");
    printf("  -> chat username\n");
    printf("  -> share file_name\n");
    printf("  -> out\n");
    printf(">> ");
}

// Funzione per mostrare una chat con l'utente il cui nome è nella stringa chatAttuale globale
void mostraChat(){
    struct connessione*     tempCon = listaConnessioni;
    // Recupero eventuale delle connessioene con l'utente 'chatAttuale'
    struct connessione*     destCon = ottieniConnessioneDevice(listaConnessioni,chatAttuale);
    // Variabile per scorrere i messaggi
    struct messaggio*       temp = listaMessaggi;
    // Vettore per recuperare i messaggi appartenenti ad una chat di gruppo (hanno come destinatario 
    // lo stesso token presente nel campo gruppo della connessione con il destinatario)
    char                    stringGroup[MAX_LEN_UTN];
    system("clear");
    posizioneTerminale = POSIZIONE_TERM_CHAT;
    // Nel caso in cui non ci sia connessione con il destiantario, oppure il gruppo sia 0 (a dire che l'utente
    // non afferisce a nessun gruppo) si stampa la chat normalemte
    if(destCon==NULL || destCon->gruppo==0){
        printf("***** Stai chattando con: %s *****\n\n", chatAttuale);
        // Si scorrono tutti i messaggi
        while(temp){
            // Si considerano i messaggi aventi come destinatario chatAttuale e si stampano, con il relativo stato
            if(strcmp(temp->destinatario,chatAttuale)==0){
                printf("@%s: ", nomeUtenteLog);
                if(temp->stato == 1){
                    printf("* ");
                }
                if(temp->stato == 2){
                    printf("**");
                }
                printf("\n\t%s\n",temp->testo);
            }
            // Si considerano i messaggi aventi come mittente chatAttuale e si stampano
            if(strcmp(temp->mittente,chatAttuale)==0 && strcmp(temp->destinatario,nomeUtenteLog)==0){
                    printf("@%s:\n\t%s\n",chatAttuale,temp->testo);
            }
            temp = temp -> next;
        }
        printf(">> ");
        return;
    }
    // Caso in cui si ha a che fare con una chat di gruppo: si cercano i messaggi che 
    // afferiscono a tale chat e si stampano
    printf("CHAT DI GRUPPO\n");
    printf("******************************\n");
    sprintf(stringGroup, "%ld", destCon->gruppo);
    // Si stampano i nomi degli utenti che afferiscono a quel gruppo attualmente connessi
    while(tempCon){
        if(tempCon->gruppo==destCon->gruppo){
            printf("- %s \n", tempCon->nomeUtente);
        }
        tempCon = tempCon->next;
    }
    printf("******************************\n");
    while(temp){
        if(strcmp(temp->destinatario, stringGroup)==0){
            printf("@%s: ", temp->mittente);
            printf("\n\t%s\n",temp->testo);
        }
        temp = temp->next;
    }
    printf(">> ");
}

// Funzione per richiedere al server la lista degli utenit online (si usa quando
// si invia \u in una chat, per conoscere gli utenti che è possibile aggiungervi).
int richiediListaUtentiOnline(){
    char                    utente[MAX_LEN_UTN+1];
    uint8_t                 comando, risposta;
    uint16_t                dimensione, dimensioneNET;
    struct connessione      *server, *dest;
    int                     sd, i=0;

    memset(utente, 0, MAX_LEN_UTN+1);

    system("clear");
    server = ottieniConnessioneServer(listaConnessioni);
    if(server == NULL){
        alert("\nIl server non è connesso, non è possibile formare una chat di gruppo");
        mostraChat();
        return 1;
    }

    // Invio il comando per richiedere la lista degli utenti
    sd = server->socketDescriptor;
    comando = COMANDO_LISTA;
    inviaPacchetto(sd, (void*)&comando, sizeof(uint8_t));   
    while(1){
        // Il server mi invia per prima cosa 1 byte contenente il codice NUOVA_RIGA_LISTA
        // se c'è un nuovo elemento da ricevere, altro in caso contrario
        riceviPacchetto(sd, (void*)&risposta, sizeof(uint8_t));
        if(risposta!=NUOVA_RIGA_LISTA){
            break;
        }
        // Ricevo la dimensione e il nome utente da stampare 
        riceviPacchetto(sd, (void*)&dimensioneNET, sizeof(uint16_t));
        dimensione = ntohs(dimensioneNET);
        riceviPacchetto(sd, (void*)utente, dimensione);
        // Verifico se ho già una connessione con l'utente
        dest = ottieniConnessioneDevice(listaConnessioni,utente);
        // Poiché il server mi invia tutti gli utenti online, ci sono alcuni che non sono validi
        // per l'aggiunta alla chat, e che quindi non considero: in particolare, l'utente con cui 
        // sto chattando, me stesso e tutti coloro che fanno parte del gruppo in cui già sono
        if( strcmp(utente, chatAttuale)==0 || 
            strcmp(utente, nomeUtenteLog)==0 || 
            (dest!=NULL && dest->gruppo == ottieniConnessioneDevice(listaConnessioni,chatAttuale)->gruppo && dest->gruppo!=0)){
                continue;
        }
        if(i==0){
            printf("Lista di utenti online:\n\n");
        }
        printf("- %s\n", utente);
        i=i+1;
    }
    // i si usa per contare il numero di righe ricevute, e stampare un messaggio opportuno se non c'è nessuno 
    // che può essere aggiunto alla chat. 
    if(i==0){
        alert("Non ci sono utenti online da poter aggiungere alla conversazione.\n");
        return 1;
    }
    return 0;
}

// Funzione per richiedere al server l'aggiunta di un utente ad un gruppo, 
// e gestire di conseguenza le connessioni con gli altri membri del gruppo
void richiediAggiuntaAGruppo(char* nomeUtente, time_t* idGruppo){
    uint16_t            dimensione, dimensioneNET, portaDestinatario, portaDestinatarioNET;
    uint8_t             comando;
    struct connessione  *server, *nuovaConnessione = NULL, *temp;
    int                 sd;
    
    // Il nome utente ottenuto tramite input termina con \n, quindi si sostituisce tale carattere con \0
    sostituisciNL(nomeUtente);
    // Recupero la connessione con il server
    server = ottieniConnessioneServer(listaConnessioni);
    if(server == NULL || strcmp(nomeUtente, chatAttuale)==0){
        alert("Impossibile aggingere l'utente al gruppo, il server potrebbe essersi disconnesso!");
        return;
    }
    // Si invia al server il comando per una nuova connnessione e l'utente che si vuole aggiungere 
    // al gruppo: il server risponde con la porta con cui contattare l'utente, 0 nel caso l'utente non esista
    // o non sia connesso
    comando = COMANDO_NUOVA_CONNESIONE; 
    dimensione = strlen(nomeUtente)+1;
    dimensioneNET = htons(dimensione);
    inviaPacchetto(server->socketDescriptor, (void*)&comando, sizeof(uint8_t));
    inviaPacchetto(server->socketDescriptor, (void*)&dimensioneNET, sizeof(uint16_t));
    inviaPacchetto(server->socketDescriptor, (void*)nomeUtente, dimensione);
    riceviPacchetto(server->socketDescriptor, (void*)&portaDestinatarioNET, sizeof(uint16_t));
    portaDestinatario = ntohs(portaDestinatarioNET);
    if(portaDestinatario==0){
        alert("Nome utente inserito non valido!\n");
        mostraChat();
        return;
    }

    // Provo a recuperare la connessione con l'utente da aggiungere al gruppo: se nuovaConnessione!=NULL allora ho già 
    // una connessione con l'utente, altrimenti la devo instaurare inviandogli il mio nome utente e la porta su cui ascolto
    nuovaConnessione = ottieniConnessioneDevice(listaConnessioni, nomeUtente);
    if(nuovaConnessione==NULL){
        // Apro la connessione TCP verificando eventuali errori
        sd = apriConnessioneTCP(portaDestinatario);
        errorHandler(sd, NULL);
        aggiungiARubrica(nomeUtente);
        // Aggiungo il socket a quelli da monitorare con la select
        FD_SET(sd, &master);
        if(sd>fdmax) fdmax=sd;
        // Gli invio il nome utente e la porta
        dimensione = strlen(nomeUtenteLog)+1;
        dimensioneNET = htons(dimensione);
        inviaPacchetto(sd, (void*)&dimensioneNET, sizeof(uint16_t));
        inviaPacchetto(sd, (void*)nomeUtenteLog, dimensione);
        inviaPacchetto(sd, (void*)&portaConnessione, sizeof(uint16_t));
        // Comunico, inviando 0, che la connessione che si sta per aprire non fa parte di nessun gruppo
        dimensione = 0;
        inviaPacchetto(sd, (void*)&dimensione, sizeof(uint16_t));
        mostraNotifica("[Aperta connessione con %s]\n", nomeUtente, 1);
    }
    else{
        sd = nuovaConnessione->socketDescriptor;
    }

    // Se non ho ancora un gruppo con l'utente con cui stavo chattando, allora creo un token per il gruppo
    // dato dal timestamp attuale (i gruppi saranno in tal modo univocamente determinati)
    if((*idGruppo)==0){
        (*idGruppo)=time(NULL);
    }

    // A cisacun membro del gruppo devo comunicare l'aggiunta del nuovo utente, inviandogli nome e porta di ascolto 
    // in modo che a loro volta possano costruirvi una connessione (specificando l'appartenenza ad un gruppo)
    temp = listaConnessioni;
    while(temp){
        // La connessione ha a che fare con il gruppo: si inviano le informazioni alle due parti
        if(temp->gruppo == (*idGruppo)){
            comando = AGGIUNGI_MEMBRO_GRUPPO;
            portaDestinatarioNET = htons(portaDestinatario);
            dimensione = strlen(nomeUtente)+1;
            dimensioneNET = htons(dimensione);

            inviaPacchetto(temp->socketDescriptor, (void*)&comando, sizeof(uint8_t));
            inviaPacchetto(temp->socketDescriptor, (void*)&portaDestinatarioNET, sizeof(uint16_t));
            inviaPacchetto(temp->socketDescriptor, (void*)&dimensioneNET, sizeof(uint16_t));
            inviaPacchetto(temp->socketDescriptor, (void*)nomeUtente, dimensione);

            mostraNotifica("[Inviata richiesta di aggiunta a gruppo a %s]\n", temp->nomeUtente, 1);
        }
        temp = temp->next;
    }
    // Si aggiunge l'eventuale nuova connessione alla lista e si modifica il suo gruppo
    if(nuovaConnessione == NULL){
        nuovaConnessione = aggiungiConnessione(&listaConnessioni, portaDestinatario,sd ,nomeUtente, 0);
    }
    nuovaConnessione->gruppo = (*idGruppo);
    mostraNotificaFullScreen("Aggiunta avvenuta correttamente!");
}

// Funzione invocata quando l'utente vuole inviare un messaggio (preme invio nella schermata di chat)
void invioMessaggioChat(){
    char                messaggio[MAX_LEN_MSG]; // Conterrà il messaggio
    char                buffer[LEN_BUFFER];     // Buffer di appoggio per prelevare da stdin
    int                 sd;             
    uint8_t             comando, risposta;
    uint16_t            dimensione, dimensioneNET, portaDestinatario;
    time_t              now;
    char                groupString[MAX_LEN_UTN+1];
    struct connessione  *conn, *temp;

    memset(buffer, 0, LEN_BUFFER); 
    memset(messaggio, 0, MAX_LEN_MSG);
    fgets(messaggio,MAX_LEN_MSG, stdin);

    // Recupero il timestamp attuale e lo porto nell'endianess della rete
    now = timestampAttuale();

    // Caso in cui è stato digitato \q: la chat si chiude
    if(strncmp(messaggio,"\\q",2)==0){
        posizioneTerminale = POSIZIONE_TERM_HOME;
        memset(chatAttuale,0,MAX_LEN_UTN);
        mostraHome();
        return;
    }
    // Caso in cui è stato digitato \u: si richiede al server la lista degli utenti online
    if(strncmp(messaggio, "\\u",2)==0){
        // Si può richiedere \u solo dopo che si è instaurata una chat con l'utente destinatario,
        // ovvero dopo aver inviato un primo messaggio
        conn = ottieniConnessioneDevice(listaConnessioni, chatAttuale);
        if(conn==NULL){
            system("clear");
            alert("Non è possibile creare una chat di gruppo prima di aver aperto\nuna connessione con il destinatario\n");
            mostraChat();
            return;
        }
        // Se qualcosa non è andato a buon fine (per esempio, non è stato possibilie raggiungere il server)
        // si mostra la chat
        if(richiediListaUtentiOnline()==1){
            mostraChat();
            return;
        } 
        // Altrimenti si mantiene per tre secondi la stampa della funzione (lista degli utenti online)
        // e si mostra nuovamente la chat aperta
        else {
            sleep(3);
            mostraChat();
            return;
        }
    }
    // Caso in cui si vuole aggiungere qualcuno al gruppo 
    if(strncmp(messaggio,"\\a",2)==0){
        // Si procede solo nel caso in cui si abbia una connessione con chatAttuale
        conn=ottieniConnessioneDevice(listaConnessioni, chatAttuale);
        if(conn==NULL){
            system("clear");
            alert("Non è possibile creare una chat di gruppo prima di aver aperto\nuna connessione con il destinatario\n");
            mostraChat();
            return;
        }
        // Si effettua la richiesta di aggiunta al gruppo
        richiediAggiuntaAGruppo((messaggio+3),&(conn->gruppo));
        sleep(1);
        mostraChat();
        return;
    }
    // In tutti gli altri casi, si prova ad inviare un messaggio:
    sostituisciNL(messaggio);
    // Verifico se è già presenta una connessione con chatAttuale: nel caso gli invio direttamente il 
    // messaggio 
    conn = ottieniConnessioneDevice(listaConnessioni, chatAttuale);
    if(conn != NULL){
        comando = COMANDO_MESSAGGIO;
        // Se l'utente con cui ho aprto una connessione fa parte di un gruppo, invio il messaggio a tutti
        // gli utenti del gruppo; lo salvo nella lista dei messaggi avente come destinatario il token del
        // gruppo
        if(conn->gruppo!=0){
            temp = listaConnessioni;
            sprintf(groupString, "%ld", conn->gruppo);
            while(temp){
                if(temp->gruppo==conn->gruppo){
                    inviaPacchetto(temp->socketDescriptor, (void*)&comando, sizeof(uint8_t));
                    inviaMessaggio(temp->socketDescriptor, nomeUtenteLog, chatAttuale, messaggio, now);
                }
                temp = temp->next;
            }
            aggiungiMessaggio(nomeUtenteLog,groupString,RISPOSTA_RECAPITO,now,messaggio, fileName, ORDINE_DEVICE);
            mostraChat();
            return;
        }
        // Altrimenti invio il messaggio direttamente al destinatario, di cui avevo precedentmente recuperato la
        // connessione
        sd = conn->socketDescriptor;
        inviaPacchetto(sd, (void*)&comando, sizeof(uint8_t));
        inviaMessaggio(sd, nomeUtenteLog, chatAttuale, messaggio, now);
        aggiungiMessaggio(nomeUtenteLog,chatAttuale,RISPOSTA_RECAPITO,now,messaggio, fileName, ORDINE_DEVICE);
        mostraChat();
        return;
    }
    // in caso contrario, devo chiedere al server di fare da tramite per l'apertura di una connessione
    conn = ottieniConnessioneServer(listaConnessioni);
    // Caso in cui il server non è connesso
    if(conn == NULL){
        alert("\n******************\nNon è possibile comunicare con il server!\nPotrebbe esssersi disconnesso. Riprovare più tardi.1\n******************\n");
        mostraChat();
        return;
    }
    // Si invia il messaggio al server, che si occupa di farci sapere se ha memorizzato il messaggio o se lo 
    // ha recapitato
    sd = conn->socketDescriptor;
    comando = COMANDO_MESSAGGIO;
    inviaPacchetto(sd, (void*)&comando, sizeof(uint8_t));
    inviaMessaggio(sd, nomeUtenteLog, chatAttuale, messaggio, now);
    riceviPacchetto(sd, (void*)&risposta, sizeof(uint8_t));

    // Se lo ha solo memorizzato, facciamo lo stesso a nostra volta, impostando lo stato del messagio ad 1
    // ad indicare che non è stato ancora ricevuto
    if(risposta == RISPOSTA_MEMORIZZAZIONE){
        aggiungiMessaggio(nomeUtenteLog,chatAttuale,1,now,messaggio, fileName, ORDINE_DEVICE);
    }
    // Se il messaggio è stato recaptitato, allora il server segue ad inviarci i dati necessari affinché si possa
    // instaurare una connessione con l'altro device (porta su cui ascolta connnessioni)
    else if(risposta == RISPOSTA_RECAPITO){
        aggiungiMessaggio(nomeUtenteLog,chatAttuale,2,now,messaggio, fileName, ORDINE_DEVICE);
        riceviPacchetto(sd, (void*)&portaDestinatario, sizeof(uint16_t));

        portaDestinatario = ntohs(portaDestinatario);
        // Apro la connessione TCP
        sd = apriConnessioneTCP(portaDestinatario);
        errorHandler(sd, NULL);

        dimensione = strlen(nomeUtenteLog)+1;
        dimensioneNET = htons(dimensione);

        // Invio il mio nome utente e la porta su cui ascolto
        inviaPacchetto(sd, (void*)&dimensioneNET, sizeof(uint16_t));
        inviaPacchetto(sd, (void*)nomeUtenteLog, dimensione);
        inviaPacchetto(sd, (void*)&portaConnessione, sizeof(uint16_t));
        // Comunico che non la connessione è 1-1, senza che ci sia un gruppo di mezzo
        dimensioneNET = 0;
        inviaPacchetto(sd, (void*)&dimensioneNET, sizeof(uint16_t));
        aggiungiConnessione(&listaConnessioni, portaDestinatario, sd, chatAttuale, 0);
        mostraNotifica("[Aperta connessione con %s]\n", chatAttuale, 1);
        sleep(1);

        // Aggiungo il socket tra quelli che la listen deve monitorare
        FD_SET(sd, &master);
        if(sd>fdmax){
            fdmax=sd;
        }
    }
    // Se il server ha riposto altro rispetto che RISPOSTA_RECAPITO e RISPOSTA_MEMORIZZAZIONE, l'utente
    // non esiste
    else{
        alert("\n******************\nL'utente inserito non esiste!\n******************\n");
        mostraChat();
        return;
    }
    mostraChat();
}

// Funzione per relaizzare il comando hanging
void hangingHandler(){
    uint16_t            numeroChat;
    uint8_t             comando, risposta;
    time_t              timestamp;
    char                mittente[MAX_LEN_UTN+1];
    struct connessione  *server;
    int                 sd, i = 0;

    memset(mittente, 0, MAX_LEN_UTN);
    system("clear");
    // Ottengo la connessione con il server
    server = ottieniConnessioneServer(listaConnessioni);

    if(server == NULL){
        alert("\nIl server si è disconnesso...\nNon è possibile eseguire hanging.\n");
        mostraHome();
        return;
    }
    // Invio il comando e proseguo a ricevere dati (e a stamparli) finché non ricevo
    // la risposta HANGING_FINE
    sd = server->socketDescriptor;
    comando = COMANDO_HANGING;
    inviaPacchetto(sd, (void*)&comando, sizeof(uint8_t));
    while(1){
        riceviPacchetto(sd, (void*)&risposta, sizeof(uint8_t));
        if(risposta == HANGING_FINE){
            break;
        }
        riceviRigaHanging(sd, mittente, &timestamp, &numeroChat);
        // Se ho ricevuto almeno una riga, stampo un header
        if(i==0){
            printf("Messaggi pendenti: \n\n");
        }
        printf("[%hd] %s - %ld \n", numeroChat, mittente, timestamp);
        i = i+1;
    }
    // Se non ho ricevuto nemmeno una riga, stampo un messaggio opportuno
    if(i==0){
        printf("Non ci sono messaggi pendenti!\n");
    }
    sleep(5);
    mostraHome();
}

// Funzione per ricevere gli ack di avvenuta lettura di un messaggio dato
// un certo socket descriptor (si usa quando il sever invia il comando 
// COMANDO_ACK)
void riceviACK(int sd){
    uint16_t            dimensione, dimensioneNET;
    char                destinatario[MAX_LEN_UTN+1];
    char                buffer[LEN_BUFFER]; 
    struct messaggio    *temp = listaMessaggi;

    memset(destinatario, 0, MAX_LEN_UTN+1);
    memset(buffer, 0, LEN_BUFFER);

    // Ricevo il nome utente del destinatario i cui messaggi sono stati letti
    riceviPacchetto(sd, (void*)&dimensioneNET, sizeof(uint16_t));
    dimensione = ntohs(dimensioneNET);
    riceviPacchetto(sd, (void*)destinatario, dimensione);
    // Aggiorno i messaggi del destinatario, impostando il loro stato a 2
    while(temp){
        if(strcmp(temp->destinatario,destinatario)==0){
            temp->stato = 2;
        }
        temp = temp->next;
    }
    // Aggiorno il file della lista dei messaggi
    aggiornaFileListaMessaggi(fileName,0);
    sprintf(buffer, "%s ha ricevuto i messaggi che avevi inoltrato!\n", destinatario);
    if(posizioneTerminale==POSIZIONE_TERM_CHAT && strcmp(destinatario, chatAttuale)==0){
        mostraChat();
    }
    else{
        mostraNotificaFullScreen(buffer);   
    }
}

// Funzione per gestire lo sblocco di uno dei socket costruiti per la connessione 
// con server e altri device. Per prima cosa i due si scambiano un comando, e sulla base 
// di quello e del tipo di connessione si effettuano operazioni differenti
void connectionHandler(int sd){
    struct connessione      *conn, *connDest;
    uint8_t                 comando, risposta;
    uint16_t                porta, dimensione;
    char                    messaggio[MAX_LEN_MSG];
    char                    buffer[LEN_BUFFER];
    char                    mittente[MAX_LEN_UTN];
    char                    destinatario[MAX_LEN_UTN];
    char                    stringGroup[MAX_LEN_UTN];
    time_t                  timestamp, groupID;
    int                     ret, i=0;

    memset(messaggio,0,MAX_LEN_MSG);
    memset(mittente,0,MAX_LEN_UTN);
    memset(destinatario,0,MAX_LEN_UTN);
    memset(buffer, 0,LEN_BUFFER);

    // Ottendo la connnessione riferita al socket
    conn = ottieniConnessioneDeviceSD(listaConnessioni, sd);
    if(conn == NULL){
        return;
    }
    // Ricevo il comando
    ret = riceviPacchetto(sd, (void*)&comando, sizeof(uint8_t));
    //Se ret==0, allora la connessione è stata chiusa, e mi comporto di conseguenza
    if(ret==0){
        // Ricavo il grupoID della connessione che si sta per chiudere: si cancella
        // il gruppo nel caso in cui il gruppo faccia ora parte di due soli membri
        mostraNotifica("[Chiusa connessione con un dispositivo]\n",NULL,1);
        groupID = conn->gruppo;
        chiudiConnessione(&listaConnessioni,sd);
        FD_CLR(sd, &master);
        if(groupID==0){
            return;
        }
        conn = listaConnessioni;
        // Se alla fine i==1, allora connDest conterrà l'unico altro membro del gruppo
        // rimasto: si pone a 0 il suo groupID
        while(conn){
            if(conn->gruppo==groupID){
                i = i+1;
                connDest = conn;
            }
            conn = conn->next;
        }
        if(i==1){
            connDest->gruppo=0;
            mostraHome();
            mostraNotifica("[Gruppo terminato]\n",NULL,1);
        }
        return;
    }
    // Casi in cui la connessione proviene dal server
    if(conn->isServer==1){
        // Realizza un test di connessione, alla stregua del comando ping
        if(comando == TEST_CONNECTION){
            risposta = SUCCESS_CODE;
            inviaPacchetto(sd, (void*)&risposta, sizeof(uint8_t));
            return;
        }
        // Abbiamo ricevuto un ACK
        if(comando == COMANDO_ACK){
            mostraNotifica("[Ricevuto ACK di lettura da parte del server]\n", NULL, 1);
            riceviACK(sd);
            return;
        }
    }
    // In questo casoo abbiamo ricevuto un messaggio (la connessione potrebbe comunque
    // provenire dal server, che ci ha inoltrato il messaggio)
    if(comando == COMANDO_MESSAGGIO){
        riceviMessaggio(sd, mittente, destinatario, messaggio, &timestamp);
        mostraNotifica("[Ricevuto un messaggio]\n", NULL, 1 );
        // Caso in cui il mittente non faceva parte di un gruppo (si salva normalmente
        // il messaggio)
        if(conn->gruppo==0){
            aggiungiMessaggio(mittente, destinatario, RISPOSTA_RECAPITO, timestamp, messaggio, fileName, ORDINE_DEVICE);
        }
        else{
            // Altrimenti, il destinatario del messaggio si pone uguale al token del
            // gruppo di cui fa parte il mittente 
            sprintf(stringGroup, "%ld", conn->gruppo);
            aggiungiMessaggio(mittente, stringGroup, RISPOSTA_RECAPITO, timestamp, messaggio, fileName, ORDINE_DEVICE);
        }
        // Si mostra la chat 
        sprintf(buffer, "Hai ricevuto un messaggio da: %s\n", mittente);
        // Si mostra la chat nel caso in cui si aveva già aperta
        if(posizioneTerminale==POSIZIONE_TERM_CHAT && strcmp(conn->nomeUtente, chatAttuale)==0 && conn->gruppo==0){
            mostraChat();
        }
        // Si mostra la chat se avevamo aperta una chat con un utente che fa parte dello
        // stesso gruppo del mittente del messaggio
        else if(ottieniConnessioneDevice(listaConnessioni, chatAttuale)!= NULL && ottieniConnessioneDevice(listaConnessioni, chatAttuale)->gruppo==conn->gruppo && conn->gruppo!=0){ 
            mostraChat();
        }
        // Negli altri casi, mostriamo una notifica
        else{
            mostraNotificaFullScreen(buffer);
        }
        return;
    }
    // Comando per la ricezione del file
    if(comando == COMANDO_INVIO_FILE){
        if(riceviFile(sd, conn->nomeUtente, nomeUtenteLog)){
            printf("File ricevuto con successo!\n");
        }
        else{
            printf("File rifiutato!\n");
        }
        sleep(3);
        mostraHome();
    }
    // Comando per l'aggiunta di un membro al gruppo
    if(comando == AGGIUNGI_MEMBRO_GRUPPO){
        // Ricevo porta e username dell'utente da aggiungere al gruppo
        riceviPacchetto(sd, (void*)&porta, sizeof(uint16_t));
        porta = ntohs(porta);
        riceviPacchetto(sd, (void*)&dimensione, sizeof(uint16_t));
        dimensione = ntohs(dimensione);
        riceviPacchetto(sd, (void*)destinatario, dimensione);
        
        // Eventualmente creo il token del gruppo
        if(conn->gruppo==0){
            conn->gruppo = time(NULL);
        }

        // Nel caso in cui non avessi già aperta la connessione con l'utente da aggiungere,
        // lo faccio.
        connDest = ottieniConnessioneDevice(listaConnessioni, destinatario);
        if(connDest == NULL){
            sd = apriConnessioneTCP(porta);
            errorHandler(sd, NULL);
            aggiungiARubrica(destinatario);
            FD_SET(sd, &master);
            if(sd>fdmax){
                fdmax=sd;
            }
            dimensione = strlen(nomeUtenteLog)+1;
            dimensione = htons(dimensione);
            
            // Per costruire la connessione, invio il mio nome utente, la mia porta
            // e il nome utente dell'utente che ha creato il gruppo, in modo che l'altro
            // possa fare l'associazione
            inviaPacchetto(sd, (void*)&dimensione, sizeof(uint16_t));
            inviaPacchetto(sd, (void*)nomeUtenteLog, ntohs(dimensione));
            inviaPacchetto(sd, (void*)&portaConnessione, sizeof(uint16_t));
            // Comunico chi ha fatto la richiesta per l'aggiunta ad un gruppoq
            dimensione = strlen(conn->nomeUtente)+1;
            dimensione = htons(dimensione);
            inviaPacchetto(sd, (void*)&dimensione, sizeof(uint16_t));
            inviaPacchetto(sd, (void*)conn->nomeUtente, ntohs(dimensione));

            connDest = aggiungiConnessione(&listaConnessioni, porta,sd ,destinatario, 0);
        }
        //Ho già una connessione con quell'utente, quindi mi basta comunicargli l'aggiunta al gruppo
        else{
            //Invio il comando di notifica di aggiunta a gruppo
            risposta = COMANDO_GRUPPO;
            inviaPacchetto(connDest->socketDescriptor, (void*)&risposta, sizeof(uint8_t));
            //Invio il nome di chi ha richeiesto l'aggiunta, con cui sicuramente ha una connessione
            dimensione = strlen(conn->nomeUtente)+1;
            dimensione = htons(dimensione);
            inviaPacchetto(connDest->socketDescriptor, (void*)&dimensione, sizeof(uint16_t));
            inviaPacchetto(connDest->socketDescriptor, (void*)conn->nomeUtente, ntohs(dimensione));
        }
        connDest->gruppo = conn->gruppo;
        sprintf(buffer, "E' stato aggiunto %s ad un gruppo!\n", connDest->nomeUtente);
        mostraNotificaFullScreen(buffer);
    }
    // Ricevo questo comando quando un utente con cui ho già una connessione mi notifica di essere 
    // stato aggiunto ad un gruppo
    if(comando==COMANDO_GRUPPO){
        riceviPacchetto(sd, (void*)&dimensione, sizeof(uint16_t));
        dimensione = ntohs(dimensione);
        riceviPacchetto(sd, (void*)destinatario, dimensione);
        connDest = ottieniConnessioneDevice(listaConnessioni, destinatario);

        if(connDest->gruppo==0){
            connDest->gruppo = time(NULL);
        }
        conn->gruppo = connDest->gruppo;
        
        mostraNotifica("[Sei stato aggiunto ad un gruppo da %s]\n", connDest->nomeUtente, 1);

        if(ottieniConnessioneDevice(listaConnessioni,chatAttuale)->gruppo==conn->gruppo){
            mostraChat();
        }
    }
}

// Funzione per la gestione del comando out
void outHandler(){
    struct connessione  *server;
    FILE*               file;
    char                filename[MAX_LEN_UTN+30];
    
    // Ottengo la connessione con il server
    server = ottieniConnessioneServer(listaConnessioni);
    // Se il server non è connesso, devo salvare nel file ts-nomeUtente.txt
    // il timestamp di logout, in modo che lo possa inviare alla successiva connessione
    // con il server
    if(!server){
        strcpy(filename, "./ListaMessaggi/ts-");
        strcat(filename, nomeUtenteLog);
        strcat(filename, ".txt\0");
        file = fopen(filename,"w");
        if(file == NULL){
            printf("Errore! Non è stato possibile aprire la lista dei messaggi!\n");
            exit(1);
        }
        fprintf(file, "%ld", time(NULL));
    }
    // Chiudi le strutture dati usate per la connessione
    chiudiListaMessaggi();
    terminaConnessioni(&listaConnessioni);
    close(listener);

    alert("Bye...\n");
    system("clear");
    exit(0);
}

// Funzione per la gestione del comando show, con cui si ricevono i messaggi 
// memorizzati nel server
void showHandler(char* mittente){
    struct connessione  *server;
    uint16_t            dimensione, dimensioneNET;
    uint8_t             comando, risposta;
    time_t              timestamp;
    char                mittenteMessagio[MAX_LEN_UTN+1];
    char                destinatarioMessaggio[MAX_LEN_UTN+1];
    char                messaggio[MAX_LEN_MSG+1];
    int                 sd, i = 0;
    
    system("clear");
    server = ottieniConnessioneServer(listaConnessioni);

    if(server == NULL){
        alert("\nIl server si è disconnesso...\nNon è possibile eseguire hanging.\n");
        mostraHome();
        return;
    }
    // Invio al server il comando opportuno
    sd = server->socketDescriptor;
    comando = COMANDO_SHOW;
    inviaPacchetto(sd, (void*)&comando, sizeof(uint8_t));
    // Invio al server il nome dell'utente di cui voglio ricevere i messaggi
    dimensione = strlen(mittente)+1;
    dimensioneNET = htons(dimensione);
    inviaPacchetto(sd, (void*)&dimensioneNET, sizeof(uint16_t));
    inviaPacchetto(sd, (void*)mittente, dimensione);

    // Se il server invia SHOW_FINE come risposta, allora non ci sono altri messaggi
    // da ricevere ed esco dal ciclo. Altrimenti ricevo un messaggio e lo memorizzo
    while(1){
        riceviPacchetto(sd, (void*)&risposta, sizeof(uint8_t));
        if(risposta == SHOW_FINE){
            break;
        }
        riceviMessaggio(sd, mittenteMessagio, destinatarioMessaggio, messaggio, &timestamp);
        aggiungiMessaggio(mittenteMessagio, destinatarioMessaggio, 2, timestamp, messaggio, fileName, ORDINE_DEVICE);
        i = i+1;
    }
    if(i==0){
        printf("\n\nNon ci sono messaggi pendenti da %s!\n", mittente);
        sleep(3);
    }
    else{
        if(i==1){
            printf("\n\nHai ricevuto un messaggio pendente da %s\n",mittente);
        }
        else{
            printf("\n\nHai ricevuto %d messaggi pendenti da %s\n",i,mittente);
        }
        sleep(3);
    }
    mostraHome();
}

// Funzione per richiedere gli ACK al server per una determinata chat
// Il device invia la richiesta al server, che a sua volta invia una richiesta
// al device che sarà gestita con connectionHanlder
void richiediACKchat(char* destinatario){
    struct connessione *server = ottieniConnessioneServer(listaConnessioni);
    if(server==NULL) return;
    uint8_t comando = COMANDO_ACK;
    uint16_t dimensione, dimensioneNET;

    inviaPacchetto(server->socketDescriptor, (void*)&comando, sizeof(uint8_t));
    dimensione = strlen(destinatario)+1;
    dimensioneNET = htons(dimensione);
    inviaPacchetto(server->socketDescriptor, (void*)&dimensioneNET, sizeof(uint16_t));
    inviaPacchetto(server->socketDescriptor, (void*)destinatario, dimensione);
}

// Funzione per la gestione del comando share
void shareHandler(char* nomeFile){
    FILE                *file;
    struct connessione  *temp = listaConnessioni;
    // Verifico l'esistenza del file da inviare nella directory corrente
    file = fopen(nomeFile, "r");
    if(file == NULL){
        alert("Il file inserito non esiste!\n");
        mostraHome();
        return;
    }
    fclose(file);
    system("clear");
    printf("Inizio invio File...\n");
    // A tutte le connessioni aperte che ho(eccetto quella con il server) provo ad
    // inviare il file. Mostro dei messaggi opportuni in caso di FAIL o SUCCESS
    while(temp){
        if(temp->isServer==0){
            printf("\nStai per inviare un file a: %s\n", temp->nomeUtente);
            if(inviaFile(temp->socketDescriptor, nomeFile)==0){
                printf("\tInvio andato a buon fine!\n");
            }
            else{
                printf("\tL'invio è stato rifiutato\n");
            }
        }
        temp = temp->next;
    }
    printf("\nFine invio file...\n");
    sleep(3);
    mostraHome();

}

// Funzione per la gestione del risveglio del socket stdin nella schermata home
void inputHandler(){
    char            input[MAX_UTN_INP];
    char            comando[MAX_UTN_INP];
    char            argomento[MAX_UTN_INP];

    memset(input, 0, MAX_UTN_INP);
    memset(comando, 0, MAX_UTN_INP);
    memset(argomento, 0, MAX_UTN_INP);

    // Leggo l'input
    fgets(input,MAX_UTN_INP,stdin);
    sscanf(input, "%s %s", comando, argomento);

    if(strcmp(comando, "chat") == 0){
        memset(chatAttuale,0,MAX_LEN_UTN);
        // Non posso aprire una chat con me stesso
        if(strcmp(argomento, nomeUtenteLog)==0){
            mostraHome();
            return;
        }
        // Verifico la presenza del nome utente in rubrica
        if(RicercaInRubrica(argomento)==0){
            mostraNotificaFullScreen("Utente non presente in rubrica!");
            return;
        }
        // Se il comando è chat, allora copio in chatAttuale il nome dell'utente
        // con cui si apre la chat
        strcpy(chatAttuale, argomento);
        posizioneTerminale = POSIZIONE_TERM_CHAT;
        // richiedo gli ACK per la conversazione con l'utente
        richiediACKchat(argomento);
        mostraChat();
    }
    else if(strcmp(comando,"hanging") == 0){
        hangingHandler();
    }
    else if(strcmp(comando,"show")    == 0){
        showHandler(argomento);
    }
    else if(strcmp(comando,"share")   == 0){
        shareHandler(argomento);
    }
    else if(strcmp(comando, "out")    == 0){
        outHandler();
    }
    else{
        alert("comando non valido!\n");
        mostraHome();
    }
}

void app(){
    struct sockaddr_in      deviceAddr; 
    struct sockaddr_in      cl_addr; 
    struct connessione      *daAggiungere, *partGruppo;
    uint16_t                dimensione, portaNuovaConnessione;
    fd_set                  read_fds;
    char                    partecipanteGruppo[MAX_LEN_UTN];
    char                    nomeUtente[MAX_LEN_UTN];
    int                     newfd, addrlen, i, ret;

    // Azzero i set
    FD_ZERO(&master); 
    FD_ZERO(&read_fds);

    // Struttura standard per mettere il dispositivo in ascolto sulla porta inserita
    listener = socket(AF_INET, SOCK_STREAM, 0);
    errorHandler(listener,NULL);

    memset(&deviceAddr, 0, sizeof(deviceAddr)); 
    deviceAddr.sin_family = AF_INET ;
    portaConnessione = htons(portaConnessione);
    deviceAddr.sin_port = portaConnessione;
    inet_pton(AF_INET, "127.0.0.1", &deviceAddr.sin_addr);

    ret = bind(listener, (struct sockaddr*)& deviceAddr, sizeof(deviceAddr));
    errorHandler(ret, NULL);
    ret = listen(listener, 10);
    errorHandler(ret, NULL);
     
    // Inizio ad ascoltare sul listener, sullo stdin e sul socket già creato per il server
    FD_SET(listener,     &master);
    FD_SET(listaConnessioni->socketDescriptor, &master);
    FD_SET(STDIN_FILENO, &master); //stdin
    fdmax = listener;
    
    // Inserisco in fileName il path per il file contenente la lista dei messaggi
    strcpy(fileName,"./ListaMessaggi/");
    strcat(fileName, nomeUtenteLog);
    strcat(fileName, ".txt\0");
    apriListaMessaggi(fileName, ORDINE_DEVICE);

    // Eventualmente invio al server l'ultimo timestamp salvato
    inviaUltimoTimestamp();

    mostraHome();
    posizioneTerminale = POSIZIONE_TERM_HOME;

    for(;;){
        read_fds = master; 
        ret = select(fdmax + 1, &read_fds, NULL, NULL, NULL);
        errorHandler(ret, NULL);
        for(i=0; i<=fdmax; i++) { 
            if(FD_ISSET(i, &read_fds)) {  
                // Ho ricevuto una richiesta di connessioe

                if(i == listener) { 
                    addrlen = sizeof(cl_addr); 
                    newfd = accept(listener, (struct sockaddr *)&cl_addr, (socklen_t*)&addrlen);

                    // Inserisco il socket nel measter
                    FD_SET(newfd, &master); 
                    memset(nomeUtente,0,MAX_LEN_UTN);

                    // Ricevo nome utente e porta su cui si connette tale utente
                    riceviPacchetto(newfd, (void*)&dimensione, sizeof(uint16_t));
                    riceviPacchetto(newfd, (void*)nomeUtente, ntohs(dimensione));
                    riceviPacchetto(newfd, (void*)&portaNuovaConnessione, sizeof(uint16_t));
                    portaNuovaConnessione = ntohs(portaNuovaConnessione);

                    // Aggiugo la connessione
                    daAggiungere = aggiungiConnessione(&listaConnessioni, portaNuovaConnessione ,newfd,nomeUtente,0);
                    aggiungiARubrica(nomeUtente);
                    mostraNotifica("[Aperta connessione con %s]\n", nomeUtente,1);

                    // Eventualmente la nuova connessione potrebbe far parte di un gruppo: in tal caso
                    // mi invia il nome utente dell'altro utente con cui ho già una connessione
                    riceviPacchetto(newfd, (void*)&dimensione, sizeof(uint16_t));
                    dimensione = ntohs(dimensione);
                    if(dimensione!=0){
                        riceviPacchetto(newfd, (void*)partecipanteGruppo, dimensione);

                        // ottengo l'altro partecipante al gruppo
                        partGruppo = ottieniConnessioneDevice(listaConnessioni, partecipanteGruppo);

                        // Eventuamente creo il token di connessione
                        if(partGruppo->gruppo==0){
                            partGruppo->gruppo = time(NULL);
                        }

                        // Modifico i token
                        daAggiungere->gruppo = partGruppo->gruppo;
                        mostraNotifica("[Sei stato aggiunto ad un gruppo da %s]\n", partGruppo->nomeUtente, 1);
                        if(ottieniConnessioneDevice(listaConnessioni,chatAttuale)->gruppo==partGruppo->gruppo){
                            mostraChat();
                        }
                    }
                    if(newfd > fdmax){
                        fdmax = newfd;
                    }
                }
                if(i==STDIN_FILENO){
                    if(posizioneTerminale==POSIZIONE_TERM_HOME){
                        inputHandler();
                    }
                    else if(posizioneTerminale==POSIZIONE_TERM_CHAT){
                        invioMessaggioChat();
                    }
                }
                else{
                    connectionHandler(i);
                }
            }
        }
    }
}


int main(int argc, char* argv[]){

    setvbuf (stdout, NULL, _IONBF, BUFSIZ);

    if(argc!=2){
        printf("Il device non è stato avviato in modo corretto!\n");
        printf("Inserire il numero di porta come parametro.\n");
        exit(1);
    }
    portaConnessione = atoi(argv[1]);

    pollingLog(portaConnessione);
    
    // Se arrivo qua, il login è avvenuto con successo
    app();
    
    return 0;
}
