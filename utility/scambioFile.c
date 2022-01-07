#include "scambioFile.h"
#include "gestoreConnessioni.h"
#include "gestoreMessaggi.h"

// Funzione per inviare un file. Si inviano N byte finché non siamo arrivati a EOF,
// dopo di che si invia 0. 
int inviaFile(int sd, char* nomeFile){
    FILE        *file;                      // Puntatore al file da inviare
    uint8_t     comando, risposta;          // Per l'interazione con l'altro 
    uint16_t    dimensione, dimensioneNET;  // Usata per memorizzare la dimensione del nome del file
    size_t      letti, lettiNET;            // Numero di byte letti e variabile per meorizzare quanto 
                                            // ricevuto dal server
    uint8_t     buffer[LEN_BUFFER];         // Buffer di applicazione epr lo scambio del file

    // Si invia la richiesta di inviare un file, e si aspetta una risposta, che può
    // essere sia positiva (risposta==ACETTA_FILE) sia negativa
    comando = COMANDO_INVIO_FILE; 
    inviaPacchetto(sd, (void*)&comando, sizeof(uint8_t));
    riceviPacchetto(sd, (void*)&risposta, sizeof(uint8_t));
    if(risposta!=ACCETTO_FILE) return 1;

    // Si misura la dimensione del nome del file e si invia come stringa (come sempre,
    // prima si invia la dimensione). 
    dimensione = strlen(nomeFile)+1;
    dimensioneNET = htons(dimensione);
    inviaPacchetto(sd, (void*)&dimensioneNET, sizeof(uint16_t));
    inviaPacchetto(sd, (void*)nomeFile, dimensione);
    
    // Invocando questa funzione, si è già aperto il file in lettura per verificarne l'esistenza
    file = fopen(nomeFile, "r");
    while(1){
        // Si leggono al più LEN_BUFFER byte dal file, rispetto all'ultimo punto in cui si era arrivati
        letti = fread(buffer, sizeof(uint8_t), LEN_BUFFER, file);
        // Se ne ho letti 0 o se sono arrivato alla fine del file, abbiamo terminato l'invio del file
        if(letti == 0 || letti == EOF){
            break;
        }
        // Altrimenti, prima si comunica quanti byte si sta per inviare, poi si inviano
        lettiNET = htonl(letti);
        inviaPacchetto(sd, (void*)&lettiNET, sizeof(size_t));
        inviaPacchetto(sd, (void*)buffer, letti);
        // Se nell'ultimo invio si sono letti meno di LEN_BUFFER byte, sicureamente abbiamo raggiunto EOF
        if(letti < LEN_BUFFER){
            break;
        }
    }
    // Inviare un valore di 0 indica la fine dell'invio del file 
    letti = 0;
    lettiNET = htons(letti);
    inviaPacchetto(sd, (void*)&lettiNET, sizeof(size_t));
    fclose(file);
    return 0;
}

// Funzioe per ricevere un file. Si ricevono byte e si scrivono nel file finché non si riceve
// una dimensione pari a 0- 
int riceviFile(int sd, char* mittente, char* destinatario){
    FILE        *file;
    uint8_t     risposta;
    uint16_t    dimensione, dimensioneNET;
    size_t      letti, lettiNET;
    uint8_t     buffer[LEN_BUFFER];
    char        nomeFile[LEN_BUFFER];
    char        nomeFileTemp[LEN_BUFFER];
    char        c;

    memset(buffer, 0, LEN_BUFFER);
    memset(nomeFile, 0, LEN_BUFFER);

    system("clear");
    printf("vuoi ricevere un file da %s?", mittente);
    printf("[Y/n]: ");
    c = getc(stdin);
    fflush(stdin);
    // Se la risposta è diversa da 'Y', si invia un rifiuto alla ricezione del file. 
    if(c!='Y'){
        risposta = RIFIUTO_FILE;
        inviaPacchetto(sd, (void*)&risposta, sizeof(uint8_t));
        return 0;
    }
    // Altrimenti, si accetta la ricezione del file. 
    risposta = ACCETTO_FILE;
    inviaPacchetto(sd, (void*)&risposta, sizeof(uint8_t));
    // Si riceve la dimensione del nome del file
    riceviPacchetto(sd, (void*)&dimensioneNET, sizeof(uint16_t));
    dimensione = htons(dimensioneNET);
    // Si riceve il nome del file
    riceviPacchetto(sd, (void*)nomeFileTemp, dimensione);
    // Al posto del nome effettivo, si salva come nomeUtente-nomefile, utile in fase
    // di test per verificare che gli utenti appartenenti ad un gruppo avessero ricevuto tutti
    // il file. 
    strcpy(nomeFile, destinatario);
    strcat(nomeFile, "-");
    strcat(nomeFile, nomeFileTemp);

    memset(buffer, 0, LEN_BUFFER);

    // Apro il file in scrittura
    file = fopen(nomeFile, "w");
    while(1){
        //Ricevo la dimensione dei byte che mi stanno per arrivare
        riceviPacchetto(sd, (void*)&lettiNET, sizeof(size_t));
        letti = ntohl(lettiNET);
        // Se il numero di byte è diverso da 0, allora è terminato lo scambio
        if(letti == 0) break;
        // Altrimenti, ricevo il numero di byte e li scrivo sul file. 
        riceviPacchetto(sd, (void*)buffer, letti);
        fwrite(buffer, sizeof(uint8_t), letti, file);
    }
    fclose(file);
    return 1;
}