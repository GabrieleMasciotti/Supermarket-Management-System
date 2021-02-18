CFLAGS = -Wall
LIBS = -lpthread
CC = gcc
D = -DDEBUG

.PHONY: all clean test conf

all: supermercato

supermercato: supermercato.c
	$(CC) $(CFLAGS) supermercato.c $(LIBS) -o $@

test:
	@ clear
	@ echo "USE <parameter=value> FOR EACH CONFIGURATION PARAMETER.\n\nK=6			//total number of cash desks\nC=50			//maximum number of customers in the supermarket at the same time (reasonable >=10)\nE=3			//customers to let enter in the supermarket at each time\nT=200			//max number of milliseconds a customer can spend in the supermarket\nP=100			//max number of products a customer can buy\nS1=2			//value to consider to close a cash desk (number of cash desks with no more of one customer in queue)\nS2=15			//value to consider to open a cash desk (number of customers in queue in at least one cash desk)\nI=2			//opened cash desks at start\ntempProd=8		//milliseconds used by a cashier to process each product\nintCom=500		//time interval for communication with director (milliseconds)\nlogFileName=logFile	//name of log file produced after the simulation is completed (MAX 20 characters; name terminated by space or tab)\n" >config.txt
	@ echo "Avviata simulazione predefinita di 25 secondi..."
	@ sleep 25s && pkill -SIGHUP supermercato &
	@ ./supermercato
	@ chmod +x analisi.sh
	@ ./analisi.sh

clean:
	@ -rm supermercato;
	@ -rm *.log;
	@ -rm config.txt;
	@ echo "Rimosso eseguibile supermercato!";
	@ echo "Rimossi file di log!";
	@ echo "Rimosso file di configurazione!";

conf:
	@ echo "USE <parameter=value> FOR EACH CONFIGURATION PARAMETER.\n\nK=			//total number of cash desks\nC=			//maximum number of customers in the supermarket at the same time (reasonable >=10)\nE=			//customers to let enter in the supermarket at each time\nT=			//max number of milliseconds a customer can spend in the supermarket\nP=			//max number of products a customer can buy\nS1=			//value to consider to close a cash desk (number of cash desks with no more of one customer in queue)\nS2=			//value to consider to open a cash desk (number of customers in queue in at least one cash desk)\nI=			//opened cash desks at start\ntempProd=		//milliseconds used by a cashier to process each product\nintCom=		//time interval for communication with director (milliseconds)\nlogFileName=		//name of log file produced after the simulation is completed (MAX 20 characters; name terminated by space or tab)\n" >config.txt


