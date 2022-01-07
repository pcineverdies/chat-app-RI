# 1. COMPILAZIONE
# Il comando 'make' necessita del makefile, che deve essere
# creato come descritto nella guida sulla pagina Elearn

# RUBRICA:
# gli username degli utenti devono essere 'user1' 'user2' e 'user3'.
# Per semplicità, non deve essere gestita la rubrica di ogni utente.
# Fare in modo che le rubriche degli utenti siano le seguenti:
#  'user1' ha in rubrica 'user2'
#  'user2' ha in rubrica 'user1' e 'user3'
#  'user3' ha in rubrica 'user2'.

  make

  read -p "Compilazione eseguita. Premi invio per eseguire..."

# 2. ESECUZIONE
# I file eseguibili di server e device devono
# chiamarsi 'serv' e 'dev', e devono essere nella current folder

# 2.1 esecuzioe del server sulla porta 4242
  gnome-terminal -x sh -c "./serv 4242; exec bash"

# 2.2 esecuzione di 3 device sulle porte {5001,...,5003}
  for port in {5001..5003}
  do
     gnome-terminal -x sh -c "./dev $port; exec bash"
  done
  