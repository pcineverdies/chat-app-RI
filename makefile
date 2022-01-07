all: device server

device: device.o gestoreConnessioni.o gestoreMessaggi.o scambioFile.o
	gcc -Wall -std=c89 device.o gestoreConnessioni.o gestoreMessaggi.o scambioFile.o -o dev

server: server.o gestoreConnessioni.o gestoreMessaggi.o
	gcc -Wall -std=c89 server.o gestoreConnessioni.o gestoreMessaggi.o -o serv

device.o: device.c ./utility/all.h ./utility/gestoreConnessioni.h ./utility/gestoreMessaggi.h ./utility/scambioFile.h
	gcc -c -Wall -std=c89 device.c -o device.o

server.o: server.c ./utility/all.h ./utility/gestoreConnessioni.h ./utility/gestoreMessaggi.h 
	gcc -c -Wall -std=c89 server.c -o server.o

gestoreConnessioni.o: ./utility/gestoreConnessioni.c ./utility/gestoreConnessioni.h
	gcc -c -Wall -std=c89 ./utility/gestoreConnessioni.c -o gestoreConnessioni.o

gestoreMessaggi.o: ./utility/gestoreMessaggi.c ./utility/gestoreMessaggi.h
	gcc -c -Wall -std=c89 ./utility/gestoreMessaggi.c -o gestoreMessaggi.o

scambioFile.o: ./utility/scambioFile.c ./utility/scambioFile.h
	gcc -c -Wall -std=c89 ./utility/scambioFile.c -o scambioFile.o

clean: 
	rm *.o dev serv