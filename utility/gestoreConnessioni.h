#include "all.h"

struct connessione{
    uint16_t porta;
    int socketDescriptor;
    char nomeUtente[MAX_LEN_UTN];
    int isServer;
    time_t gruppo; 
    struct connessione* next;
};

void errorHandler(int , const char * );

struct connessione* aggiungiConnessione(struct connessione**, uint16_t , int , char* , int );

void chiudiConnessione(struct connessione**, int );

void terminaConnessioni(struct connessione** );

struct connessione* ottieniConnessioneServer(struct connessione*);

struct connessione* ottieniConnessioneDevice(struct connessione*, char* );

struct connessione* ottieniConnessioneDeviceSD(struct connessione*, int);

int apriConnessioneTCP(uint16_t );

int inviaPacchetto(int, void*, int);

int riceviPacchetto(int, void*, int);

