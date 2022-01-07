#include "all.h"

struct messaggio{
    char mittente[MAX_LEN_UTN+1];       // Nome utente mittente del messaggio
    char destinatario[MAX_LEN_UTN+1];   // Nome utente destinatario del messaggio
    int stato;                          // Stato del messaggio: nel caso del device, può assumere valori 
                                        // 1 o 2, a seconda che il messaggio sia arrivato o meno a destinazione;
                                        // nel caso del server, assume valori 0 o 2, nel caso in cui sia stato inoltrato
                                        // a destinazione. 
    time_t timestamp;
    char testo[MAX_LEN_MSG+1];
    struct messaggio* next;
};

struct ack{
    char mittente[MAX_LEN_UTN+1];       // Mittente del messaggio di cui conosciamo l'avvenuta lettura
    char destinatario[MAX_LEN_UTN+1];   // Destinatario del messaggio, ovvero colui che ha ricevuto il messagio
    int inviato;
    struct ack* next;
};

struct messaggio* listaMessaggi;        // Puntatori globali per la gestione delle liste dei messaggi e degli ack.
struct ack* listaACK;                   // La scelta di queste due variabili globali non è delle più rosee, ma facilita
                                        // la chiamata alle funzioni che li gestiscono, evitando ogni volta di passare
                                        // il riferimento alla lista su cui lavoare (che comunque è sempre la stessa).

void apriListaACK(char* );

void aggiungiACK(char*, char*);

void salvaListaACK(char* );

struct ack* trovaACK(char* , char* );

void chiudiListaMessaggi();

void chiudiListaACK();

void inserimentoOrdinatoListaMessaggi(struct messaggio*, int);

FILE* apriFileListaMessaggi(char* , char* );

void apriListaMessaggi(char*, int);

void aggiornaFileListaMessaggi(char* , int);

void aggiungiMessaggio(char* , char* , int , time_t , char*, char* , int);

void inviaMessaggio(int, char*, char*, char*, time_t);

void riceviMessaggio(int, char*, char*, char*, time_t* );

void riceviRigaHanging(int, char* , time_t* , uint16_t* );

void invioRigaHanging(int, char* , time_t , uint16_t );