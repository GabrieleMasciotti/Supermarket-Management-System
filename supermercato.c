#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include "utilities.h"		//inclusione del file di definizione delle funzioni ausiliarie

#ifndef DEBUG
#define DEBUG 0
#endif

typedef struct config{
	int K;		//numero massimo di casse aperte (cassieri)
	int C;		//numero massimo di clienti nel supermercato contemporaneamente
	int E;		//numero di clienti da far entrare ogni volta
	int T;		//numero massimo di millisecondi che un cliente trascorre nel supermercato
	int P;		//numero massimo di prodotti che un cliente può acquistare
	int S1; 	//soglia per la chiusura di una cassa (numero di casse con al più un cliente in coda)
	int S2; 	//soglia per l'apertura di una cassa (numero di clienti in coda in almeno una cassa)
	int I;		//numero delle casse aperte al momento dell'apertura del supermercato
	int tempProd;	//tempo impiegato dai cassieri per la gestione di un singolo prodotto
	int intCom;	//intervallo di tempo per comunicazione cassiere-direttore
	char* logFileName;
	} configValues;

typedef struct n{			//coda di clienti
	pthread_t cliente;
	int numprod;			//numero di prodotti che il cliente ha acquistato
	int servito;			//se il cliente è stato servito (1) oppure è ancora in attesa (0) (settato a -1 dal cassiere vuol dire che la cassa è stata chiusa dal direttore)
	pthread_cond_t condservito;	//il cliente attende di essere servito
	struct n* next;
	} filaCassa;

typedef struct node{				//struttura dati che implementa una cassa
	int numerodicassa;
	int open;				//cassa aperta: open=1, cassa chiusa: open=0
	int supermercato;			//supermercato=1 (aperto), supermercato=0 (chiuso)
	int prodottiVenduti;			//numero di prodotti venduti dalla cassa
	int clientiServiti;			//numero di clienti serviti dalla cassa
	int tempoApertura;			//tempo totale di apertura della cassa
	int tempoServizio;			//tempo totale impiegato per il servizio dei clienti
	int numChiusure;			//numero di volte che la cassa è stata chiusa dal direttore
	pthread_t cassiere;			//tid del cassiere che serve alla cassa
	filaCassa* coda;			//coda di clienti
	pthread_mutex_t mutexCassa;		//mutex di accesso in mutua esclusione alla cassa
	pthread_cond_t condcassa;		//condition variable per segnalare eventi al cassiere in attesa
	} strcassa;

typedef struct buf{
	int lencoda;
	pthread_mutex_t lenmux;		//mutex per gestione accesso al numero dei clienti in coda alla cassa
	} comunicazioneDirettoreCassieri;

typedef struct lp{				//struttura che implementa la lista di clienti in attesa del permesso del direttore per uscire senza acquisti
	int permesso;
	pthread_t tidcliente;
	pthread_cond_t permessocond;
	struct lp* next;
	} listaP;

typedef struct listcli{	//lista di tid dei clienti che escono dopo SIGQUIT senza attendere il servizio dei cassieri o il permesso del direttore (devono essere attesi dal thread main con join)
	pthread_t tidcli;
	struct listcli* next;
	} clientiQuit_t;


void configurazione_iniziale(configValues** valori){		//funzione di inizializzazione dei valori di configurazione
	*valori=Malloc(sizeof(configValues));
	int fd;						//file descriptor di config.txt
	if((fd = open("config.txt", O_RDONLY)) == -1){	//file config.txt nella stessa directory
		perror("Opening config.txt");
		exit(EXIT_FAILURE);
	}
	int byteLetti;
	struct stat info;
	if(fstat(fd,&info)==-1){	    //ricavo la lunghezza in byte del file di configurazione
		perror("fstat");
		exit(EXIT_FAILURE);}
	int lunconf=(int)info.st_size;
	char* buf=Malloc(lunconf*sizeof(char));
	if((byteLetti = read(fd,buf,lunconf))>0){	//lettura dei valori dei parametri
		char* p=strstr(buf,"K=");
		if(p!=NULL){
			p++; p++;
			(*valori)->K=strtol(p,NULL,10);
			}
		else{
			fprintf(stderr,"Configuration file not complete or ill formatted; unable to initialize program!\n");
			exit(EXIT_FAILURE);}
		p=strstr(buf,"C=");
		if(p!=NULL){
			p++; p++;
			(*valori)->C=strtol(p,NULL,10);}
		else{
			fprintf(stderr,"Configuration file not complete or ill formatted; unable to initialize program!\n");
			exit(EXIT_FAILURE);}
		p=strstr(buf,"E=");
		if(p!=NULL){
			p++; p++;
			(*valori)->E=strtol(p,NULL,10);}
		else{
			fprintf(stderr,"Configuration file not complete or ill formatted; unable to initialize program!\n");
			exit(EXIT_FAILURE);}
		p=strstr(buf,"T=");
		if(p!=NULL){
			p++; p++;
			(*valori)->T=strtol(p,NULL,10);}
		else{
			fprintf(stderr,"Configuration file not complete or ill formatted; unable to initialize program!\n");
			exit(EXIT_FAILURE);}
		p=strstr(buf,"P=");
		if(p!=NULL){
			p++; p++;
			(*valori)->P=strtol(p,NULL,10);}
		else{
			fprintf(stderr,"Configuration file not complete or ill formatted; unable to initialize program!\n");
			exit(EXIT_FAILURE);}
		p=strstr(buf,"S1=");
		if(p!=NULL){
			p++; p++; p++;
			(*valori)->S1=strtol(p,NULL,10);}
		else{
			fprintf(stderr,"Configuration file not complete or ill formatted; unable to initialize program!\n");
			exit(EXIT_FAILURE);}
		p=strstr(buf,"S2=");
		if(p!=NULL){
			p++; p++; p++;
			(*valori)->S2=strtol(p,NULL,10);}
		else{
			fprintf(stderr,"Configuration file not complete or ill formatted; unable to initialize program!\n");
			exit(EXIT_FAILURE);}
		p=strstr(buf,"I=");
		if(p!=NULL){
			p++; p++;
			(*valori)->I=strtol(p,NULL,10);}
		else{
			fprintf(stderr,"Configuration file not complete or ill formatted; unable to initialize program!\n");
			exit(EXIT_FAILURE);}
		p=strstr(buf,"tempProd=");
		if(p!=NULL){
			p=p+9;
			(*valori)->tempProd=strtol(p,NULL,10);}
		else{
			fprintf(stderr,"Configuration file not complete or ill formatted; unable to initialize program!\n");
			exit(EXIT_FAILURE);}
		p=strstr(buf,"intCom=");
		if(p!=NULL){
			p=p+7;
			(*valori)->intCom=strtol(p,NULL,10);}
		else{
			fprintf(stderr,"Configuration file not complete or ill formatted; unable to initialize program!\n");
			exit(EXIT_FAILURE);}
		p=strstr(buf,"logFileName=");
		if(p!=NULL){
			p=p+12;
			(*valori)->logFileName=Malloc(26*sizeof(char));
			int i=0;
			while(p[i]!=' ' && p[i]!='	'){
				(*valori)->logFileName[i]=p[i];
				i++;
			}
			(*valori)->logFileName[i]='\0';}
		else{
			fprintf(stderr,"Configuration file not complete or ill formatted; unable to initialize program!\n");
			exit(EXIT_FAILURE);}
	}
	if(byteLetti == -1){
		perror("Reading config.txt");
		exit(EXIT_FAILURE);
	}
	free(buf);
	if(close(fd) == -1){
		perror("Closing config.txt");
		exit(EXIT_FAILURE);
	}
}

// ----------------------- RISORSE CONDIVISE -----------------------

static configValues* valori=NULL;	//risorsa condivisa in lettura tra i thread, contenente i valori utili per il funzionamento (non protetta da mutex perché non modificabile)
static strcassa* listacasse;			//risorsa condivisa array delle casse nel supermercato (dalla cassa 0 alla K-1)
static comunicazioneDirettoreCassieri* communicationBuffer;  //buffer di comunicazione tra direttore e cassieri in cui i cassieri, accedendo con il proprio numero di cassa (i), inseriscono le lunghezze delle code di clienti alle casse dopo aver ottenuto la relativa lock
static int clientiPresenti=0;			//numero di clienti presenti all'interno del supermercato
pthread_mutex_t presenzemx=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t permessomx=PTHREAD_MUTEX_INITIALIZER;
static listaP* listaPermessi=NULL;			//coda di clienti con 0 prodotti che attendono il permesso del direttore per uscire dal supermercato
static int uscireSenzaPermesso=0;			//variabile globale utilizzata per comunicare ai clienti con 0 prodotti, non ancora in lista per attendere il permesso, di uscire immediatamente
pthread_cond_t supermercatoChiusoCond=PTHREAD_COND_INITIALIZER;
pthread_mutex_t openCloseMutex=PTHREAD_MUTEX_INITIALIZER;
static int openClose;		//variabile globale utilizzata per comunicare la chiusura immediata del supermercato ai clienti non ancora in coda (che stanno ancora facendo acquisti) e al direttore
static clientiQuit_t* clientiQuit=NULL;  //lista di clienti che sono usciti subito dal supermercato senza mettersi in coda alle casse o attendere il permesso del direttore (attesi con join dal main)
pthread_mutex_t clientiQuitmx=PTHREAD_MUTEX_INITIALIZER;
int logfd;			//file descriptor del file di log

// -----------------------------------------------------------------

void* communicationThread(void* args){				//sotto-thread dei cassieri addetto alla comunicazione cassiere-direttore
	int ncassa=*(int*)args;
	int tempcom=valori->intCom;
	struct timespec temp={(tempcom/1000),(tempcom%1000)*1000000};
	int out;
	int error;
	int lunghezzaCoda;
	filaCassa* count=NULL;
	while(1){
		out=nanosleep(&temp,NULL);							//attende l'intervallo di tempo della comunicazione
		if(out==-1) {perror("communication nanosleep");}
		error=pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);	//disabilita la cancellazione durante la fase di calcolo e comunicazione
		if(error!=0){
			errno=error;
			perror("while disabling communication thread cancellation");
			exit(EXIT_FAILURE);
		}
		Pthread_mutex_lock(&(listacasse[ncassa].mutexCassa));
		lunghezzaCoda=0;
		count=listacasse[ncassa].coda;
		while(count!=NULL){
			lunghezzaCoda++;						//calcolo della lunghezza della fila alla cassa "ncassa"
			count=count->next;
		}
		Pthread_mutex_unlock(&(listacasse[ncassa].mutexCassa));
		Pthread_mutex_lock(&(communicationBuffer[ncassa].lenmux));
		communicationBuffer[ncassa].lencoda=lunghezzaCoda;			//inserimento della lunghezza nel buffer di comunicazione
		
		if(DEBUG) fprintf(stdout,"Lunghezza coda %d: %d.\n",ncassa,communicationBuffer[ncassa].lencoda);
		
		Pthread_mutex_unlock(&(communicationBuffer[ncassa].lenmux));
		error=pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);		//riabilitazione della cancellazione del thread
		if(error!=0){
			errno=error;
			perror("while enabling communication thread cancellation");
			exit(EXIT_FAILURE);
		}
	}
}


void* cassiere(void* arg){
	sigset_t closureSigMask;			//maschera di segnali di chiusura del supermercato (ereditata anche dai sotto-thread di comunicazione)
	int si;
	si=sigemptyset(&closureSigMask);
	if(si==-1){
		perror("cashier_sigemptyset");
	}
	si=sigaddset(&closureSigMask,SIGQUIT);
	if(si==-1){
		perror("cashier_sigaddset");
	}
	si=sigaddset(&closureSigMask,SIGHUP);
	if(si==-1){
		perror("cashier_sigaddset");
	}
	si=pthread_sigmask(SIG_SETMASK,&closureSigMask,NULL);				//il thread cassiere maschera i segnali di chiusura per indirizzarli al thread main
	if(si!=0){
		fprintf(stderr,"pthread_sigmask returned with error: %d.\n",si);
		exit(EXIT_FAILURE);
	}
	int gestprod=valori->tempProd;
	int ncassa=*(int*) arg;
	unsigned int s=time(NULL)+ncassa;
	unsigned int* seed=&s;
	int tempofisso=(rand_r(seed)%60)+20;		//generazione casuale tempo fisso di gestione dei clienti da parte di ogni cassiere (tra 20 e 80 millisecondi)
	pthread_t tidd;
	int err=pthread_create(&tidd,NULL,&communicationThread,arg);			//genero il sotto-thread addetto alla comunicazione cassiere-direttore
	if(err!=0){
		fprintf(stdout, "SC pthread_create returned with error: %d while creating communication thread\n", err);
		exit(EXIT_FAILURE);
		}
	filaCassa* corr=NULL;
	int cancelreq;
	void* exitstatus;
	int numprodotti;
	int tempototale;
	struct timespec* temp=NULL;
	int out;
	pthread_t tidcliente;
	struct timespec tempoA;
	struct timespec tempoS;
	int openTime=0;
	int closeTime=0;
	int inizioServizio=0;
	int fineServizio=0;
	clock_gettime(CLOCK_REALTIME, &tempoA);
	openTime=tempoA.tv_sec*1000+tempoA.tv_nsec/1000000;
	while(1){
		Pthread_mutex_lock(&(listacasse[ncassa].mutexCassa));
		//il cassiere controlla, prima di iniziare a servire un nuovo cliente, che il direttore non abbia chiuso la cassa o che il main non abbia chiuso il supermercato
		if(listacasse[ncassa].open==0 || listacasse[ncassa].supermercato==0){
			if(listacasse[ncassa].coda!=NULL){
				corr=listacasse[ncassa].coda;
				while(corr!=NULL){
					corr->servito=-1;				//se ci sono clienti in coda gli segnala che devono cambiare cassa
					Pthread_cond_signal(&(corr->condservito));
					corr=corr->next;
				}
			}
			listacasse[ncassa].cassiere=0;
			Pthread_mutex_unlock(&(listacasse[ncassa].mutexCassa));
			cancelreq=pthread_cancel(tidd);				//il cassiere richiede la cancellazione del suo sotto-thread addetto alla comunicazione periodica col direttore
			if(cancelreq!=0){
				errno=cancelreq;
				perror("pthread_cancel");
				exit(EXIT_FAILURE);
			}
			cancelreq=pthread_join(tidd,&exitstatus);			//attende la terminazione del thread di comunicazione
			if(cancelreq!=0){
				errno=cancelreq;
				perror("CCth_pthread_join");
				exit(EXIT_FAILURE);
			}
			if(exitstatus!=PTHREAD_CANCELED){				//controllo dell'avvenuta cancellazione del thread di comunicazione
				fprintf(stdout, "Communication thread not canceled (shouldn't happen).\n");
			}
			Pthread_mutex_lock(&(communicationBuffer[ncassa].lenmux));
			communicationBuffer[ncassa].lencoda=-1;
			Pthread_mutex_unlock(&(communicationBuffer[ncassa].lenmux));
			clock_gettime(CLOCK_REALTIME, &tempoA);
			closeTime=tempoA.tv_sec*1000+tempoA.tv_nsec/1000000;
			listacasse[ncassa].tempoApertura+=(closeTime-openTime);
			listacasse[ncassa].numChiusure++;
			
			if(DEBUG) fprintf(stdout,"Cassiere terminato!\n");
			
			return(NULL);						//terminazione thread cassiere a seguito della chiusura della cassa
		}
		while(listacasse[ncassa].coda==NULL && listacasse[ncassa].open==1 && listacasse[ncassa].supermercato==1){
			
			if(DEBUG) fprintf(stdout,"                             Cassiere %d in attesa!\n",ncassa);
			
			Pthread_cond_wait(&(listacasse[ncassa].condcassa),&(listacasse[ncassa].mutexCassa));
			
		}
		if(listacasse[ncassa].coda!=NULL){
			clock_gettime(CLOCK_REALTIME, &tempoS);
			inizioServizio=tempoS.tv_sec*1000+tempoS.tv_nsec/1000000;
			numprodotti=listacasse[ncassa].coda->numprod;
			listacasse[ncassa].prodottiVenduti+=numprodotti;
			tidcliente=listacasse[ncassa].coda->cliente;
			Pthread_mutex_unlock(&(listacasse[ncassa].mutexCassa));
			tempototale=tempofisso+(numprodotti*gestprod);
			temp=Malloc(sizeof(struct timespec));
			temp->tv_sec=(tempototale/1000);
			temp->tv_nsec=(tempototale%1000)*1000000;
			out=nanosleep(temp,temp);					//attende "tempototale" millisecondi (tempototale/1000 secondi) e "(tempototale%1000)*1000000" nanosecondi
			while(out==-1 && (temp->tv_sec!=0 || temp->tv_nsec!=0)){			//se la nanosleep dovesse essere interrotta, riprenderebbe da dove era stata interrotta
				out=nanosleep(temp,temp);
			}
			if(out==-1) {perror("cashier nanosleep");}
			free(temp);
			Pthread_mutex_lock(&(listacasse[ncassa].mutexCassa));
			listacasse[ncassa].coda->servito=1;
			Pthread_cond_signal(&(listacasse[ncassa].coda->condservito));		//il cassiere segnala al cliente in attesa il termine del servizio
			listacasse[ncassa].clientiServiti+=1;
			Pthread_mutex_unlock(&(listacasse[ncassa].mutexCassa));
			clock_gettime(CLOCK_REALTIME, &tempoS);
			fineServizio=tempoS.tv_sec*1000+tempoS.tv_nsec/1000000;
			listacasse[ncassa].tempoServizio+=(fineServizio-inizioServizio);
			
			if(DEBUG) fprintf(stdout,"Cassiere %d attende il cliente %ld!\n",ncassa,tidcliente);
			
			out=pthread_join(tidcliente,NULL);					//attende l'uscita del cliente
				if(out!=0){
					errno=out;
					perror("CCli_pthread_join");
					exit(EXIT_FAILURE);
				}
		}
		else{
			Pthread_mutex_unlock(&(listacasse[ncassa].mutexCassa));
		}
	}
}


void addClienteQuit(clientiQuit_t** lista, pthread_t tidCliente, pthread_mutex_t mutex){		//funzione che aggiunge un tid cliente alla lista di clienti da attendere nel main (dopo SIGQUIT)
	Pthread_mutex_lock(&(mutex));
	if((*lista)==NULL){
		(*lista)=Malloc(sizeof(clientiQuit_t));
		(*lista)->tidcli=tidCliente;
		(*lista)->next=NULL;
	}
	else{
		clientiQuit_t* corr=(*lista);
		while(corr->next!=NULL){
			corr=corr->next;
		}
		corr->next=Malloc(sizeof(clientiQuit_t));
		corr=corr->next;
		corr->tidcli=tidCliente;
		corr->next=NULL;
	}
	Pthread_mutex_unlock(&(mutex));
}

void* cliente(void* argNotUsed){
	sigset_t closureSigMask;
	int si;
	si=sigemptyset(&closureSigMask);
	if(si==-1){
		perror("client_sigemptyset");
	}
	si=sigaddset(&closureSigMask,SIGQUIT);
	if(si==-1){
		perror("client_sigaddset");
	}
	si=sigaddset(&closureSigMask,SIGHUP);
	if(si==-1){
		perror("client_sigaddset");
	}
	si=pthread_sigmask(SIG_SETMASK,&closureSigMask,NULL);				//il thread cliente maschera i segnali di chiusura per indirizzarli al thread main
	if(si!=0){
		fprintf(stderr,"Client pthread_sigmask returned with error: %d.\n",si);
		exit(EXIT_FAILURE);
	}
	int tempomassimo=valori->T;
	int numMaxProd=valori->P;
	long int id=(long int) pthread_self();
	pthread_t mytid=pthread_self();
	unsigned int s=time(NULL)+id;
	unsigned int* seed=&s;
	int tempoacquisti=(rand_r(seed)%(tempomassimo-10))+10;	//generazione casuale tempo acquisti tra 10 e T millisecondi
	int prodotti=(rand_r(seed)%numMaxProd);			//generazione casuale prodotti acquistati
	int tempoEntrata=0;
	int tempoUscita=0;
	int tempoPresenza=0;
	int inizioFila=0;
	int fineFila=0;
	int tempoInFila=0;
	int numCode=0;
	struct timespec timeS;
	struct timespec timeC;
	struct timespec timer;
	clock_gettime(CLOCK_REALTIME, &timeS);
	tempoEntrata=timeS.tv_sec*1000+timeS.tv_nsec/1000000;	//ricavo il tempo di entrata nel supermercato in millisecondi
	clock_gettime(CLOCK_REALTIME, &timer);
	timer.tv_sec+=tempoacquisti/1000;
	if((timer.tv_nsec+=(tempoacquisti%1000)*1000000)>=1000000000){	//aggiustamento dei tempi di attesa della pthread_cond_timedwait (i millisecondi possono essere al massimo 999999999)
		timer.tv_sec+=1;
		timer.tv_nsec-=1000000000;
	}
	Pthread_mutex_lock(&(openCloseMutex));
	int timeWaitErr=0;
	while(timeWaitErr==0 && openClose==1){	//il cliente fa i suoi acquisti in "tempoacquisti" millisecondi (tempoacquisti/1000 secondi) e "(tempoacquisti%1000)*1000000" nanosecondi
		timeWaitErr=pthread_cond_timedwait(&(supermercatoChiusoCond),&(openCloseMutex),&(timer));
	}
	if(timeWaitErr!=0 && timeWaitErr!=ETIMEDOUT){
		fprintf(stderr,"Timed waiting failed! Program termination.\n");
		exit(EXIT_FAILURE);
	}
	if(openClose==0){					//se mentre fa gli acquisti il supermercato viene chiuso con SIGQUIT, allora esce immediatamente
		Pthread_mutex_unlock(&(openCloseMutex));
		addClienteQuit(&clientiQuit,mytid,clientiQuitmx);
		Pthread_mutex_lock(&(presenzemx));
		clientiPresenti--;
		Pthread_mutex_unlock(&(presenzemx));
		clock_gettime(CLOCK_REALTIME, &timeS);
		tempoUscita=timeS.tv_sec*1000+timeS.tv_nsec/1000000;	//tempo all'uscita in millisecondi
		tempoPresenza=tempoUscita-tempoEntrata;
		dprintf(logfd,"Cliente %ld: |prodotti acquistati: 0|tempo tot. nel supermercato: %.3f|tempo in fila: %.3f|numero di code visitate: %d.\n",id,(float) tempoPresenza/1000,(float) tempoInFila/1000,numCode);
		return(NULL);
	}
	Pthread_mutex_unlock(&(openCloseMutex));
	if(prodotti==0){
		
		if(DEBUG) fprintf(stdout,"Cliente con 0 prodotti!\n");
		
		listaP* aux=NULL;
		Pthread_mutex_lock(&(permessomx));
		if(uscireSenzaPermesso==1){
			Pthread_mutex_unlock(&(permessomx));
			addClienteQuit(&clientiQuit,mytid,clientiQuitmx);
			Pthread_mutex_lock(&(presenzemx));
			clientiPresenti--;
			Pthread_mutex_unlock(&(presenzemx));
			clock_gettime(CLOCK_REALTIME, &timeS);
			tempoUscita=timeS.tv_sec*1000+timeS.tv_nsec/1000000;	//tempo all'uscita in millisecondi
			tempoPresenza=tempoUscita-tempoEntrata;
			dprintf(logfd,"Cliente %ld: |prodotti acquistati: %d|tempo tot. nel supermercato: %.3f|tempo in fila: %.3f|numero di code visitate: %d.\n",id,prodotti,(float) tempoPresenza/1000,(float) tempoInFila/1000,numCode);
			return(NULL);				//il cliente con 0 prodotti esce immediatamente (senza attendere il permesso) dal supermercato perché chiuso con SIGQUIT
		}
		if(listaPermessi==NULL){
			listaPermessi=Malloc(sizeof(listaP));
			listaPermessi->permesso=0;
			listaPermessi->tidcliente=mytid;
			int errr=pthread_cond_init(&(listaPermessi->permessocond),NULL);
			if(errr!=0){
				fprintf(stderr,"pthread_cond_init failed");
				exit(EXIT_FAILURE);
			}
			listaPermessi->next=NULL;
			aux=listaPermessi;
		}							//il cliente si mette in fila per attendere il permesso del direttore per uscire
		else{
			listaP* corr=listaPermessi;
			while(corr->next!=NULL){
				corr=corr->next;
			}
			corr->next=Malloc(sizeof(listaP));
			corr=corr->next;
			corr->permesso=0;
			corr->tidcliente=mytid;
			int errr=pthread_cond_init(&(corr->permessocond),NULL);
			if(errr!=0){
				fprintf(stderr,"pthread_cond_init failed");
				exit(EXIT_FAILURE);
			}
			corr->next=NULL;
			aux=corr;
		}
		while(aux->permesso==0){
			Pthread_cond_wait(&(aux->permessocond),&(permessomx));	//se il cliente non ha acquistato prodotti attende il permesso del direttore per uscire dal supermercato
		}
		if(aux==listaPermessi){
			listaPermessi=listaPermessi->next;
			free(aux);
		}
		else{
			listaP* prec=listaPermessi;
			while(prec->next!=aux){
				prec=prec->next;
			}
			prec->next=aux->next;
			free(aux);
		}
		Pthread_mutex_unlock(&(permessomx));
		
		if(DEBUG) fprintf(stdout,"Cliente esce dal supermercato senza acquisti!\n");
		
		Pthread_mutex_lock(&(presenzemx));
		clientiPresenti--;
		Pthread_mutex_unlock(&(presenzemx));
		clock_gettime(CLOCK_REALTIME, &timeS);
		tempoUscita=timeS.tv_sec*1000+timeS.tv_nsec/1000000;	//tempo all'uscita in millisecondi
		tempoPresenza=tempoUscita-tempoEntrata;
		dprintf(logfd,"Cliente %ld: |prodotti acquistati: %d|tempo tot. nel supermercato: %.3f|tempo in fila: %.3f|numero di code visitate: %d.\n",id,prodotti,(float) tempoPresenza/1000,(float) tempoInFila/1000,numCode);
		
		return(NULL);			//il cliente esce dal supermercato senza acquisti dopo aver ricevuto il permesso del direttore
	}
	int k=valori->K;
	int cassa;
	int ok;
	filaCassa* pointer;
	int erc;
	filaCassa* corr=NULL;
	filaCassa* prec=NULL;
	int openSuper;
	while(1){
		ok=0;
		while(ok==0){
			cassa=(rand_r(seed)%k);					//il cliente sceglie una cassa random
			Pthread_mutex_lock(&(listacasse[cassa].mutexCassa));
			if(listacasse[cassa].supermercato==0){
				Pthread_mutex_unlock(&(listacasse[cassa].mutexCassa));
				addClienteQuit(&clientiQuit,mytid,clientiQuitmx);
				Pthread_mutex_lock(&(presenzemx));
				clientiPresenti--;
				Pthread_mutex_unlock(&(presenzemx));
				clock_gettime(CLOCK_REALTIME, &timeS);
				tempoUscita=timeS.tv_sec*1000+timeS.tv_nsec/1000000;	//tempo all'uscita in millisecondi
				tempoPresenza=tempoUscita-tempoEntrata;
				dprintf(logfd,"Cliente %ld: |prodotti acquistati: 0|tempo tot. nel supermercato: %.3f|tempo in fila: %.3f|numero di code visitate: %d.\n",id,(float) tempoPresenza/1000,(float) tempoInFila/1000,numCode);
				return(NULL);					//il cliente esce immediatamente dal supermercato perché chiuso con SIGQUIT
			}
			if(listacasse[cassa].open==1) {ok=1;}
			else{Pthread_mutex_unlock(&(listacasse[cassa].mutexCassa));}
		}
		pointer=NULL;
		if(listacasse[cassa].coda==NULL){				//se il cliente è il primo a mettersi in fila in quella cassa
			clock_gettime(CLOCK_REALTIME, &timeC);
			inizioFila=timeC.tv_sec*1000+timeC.tv_nsec/1000000;
			listacasse[cassa].coda=Malloc(sizeof(filaCassa));
			listacasse[cassa].coda->cliente=mytid;
			listacasse[cassa].coda->numprod=prodotti;
			listacasse[cassa].coda->servito=0;
			erc=pthread_cond_init(&(listacasse[cassa].coda->condservito),NULL);
			if(erc!=0){
				fprintf(stderr,"pthread_cond_init failed");
				exit(EXIT_FAILURE);
			}
			listacasse[cassa].coda->next=NULL;
			pointer=listacasse[cassa].coda;
			numCode++;
			
			if(DEBUG) fprintf(stdout,"Cliente %ld in coda alla cassa %d!\n",listacasse[cassa].coda->cliente,cassa);
			
			Pthread_cond_signal(&(listacasse[cassa].condcassa));
		}
		else{
			clock_gettime(CLOCK_REALTIME, &timeC);
			inizioFila=timeC.tv_sec*1000+timeC.tv_nsec/1000000;
			corr=listacasse[cassa].coda;
			while(corr->next!=NULL){
				corr=corr->next;
			}
			corr->next=Malloc(sizeof(filaCassa));		//se non è il primo a mettersi in coda alla cassa selezionata
			corr=corr->next;
			corr->cliente=mytid;
			corr->numprod=prodotti;
			corr->servito=0;
			int erc=pthread_cond_init(&(corr->condservito),NULL);
			if(erc!=0){
				fprintf(stderr,"pthread_cond_init failed");
				exit(EXIT_FAILURE);
			}
			corr->next=NULL;
			pointer=corr;
			numCode++;
			
			if(DEBUG) fprintf(stdout,"Cliente %ld in coda alla cassa %d!\n",corr->cliente,cassa);
			
		}
		while(pointer->servito==0){
			Pthread_cond_wait(&(pointer->condservito),&(listacasse[cassa].mutexCassa));		//il cliente attende di essere servito dal cassiere
		}
		if(pointer->servito==1){							//se il cliente è stato servito dal cassiere, esce dal supermercato
			if(pointer==listacasse[cassa].coda){
				listacasse[cassa].coda=listacasse[cassa].coda->next;
				free(pointer);
				Pthread_mutex_unlock(&(listacasse[cassa].mutexCassa));
				clock_gettime(CLOCK_REALTIME, &timeC);
				fineFila=timeC.tv_sec*1000+timeC.tv_nsec/1000000;
				tempoInFila+=(fineFila-inizioFila);
				Pthread_mutex_lock(&(presenzemx));
				clientiPresenti--;
				Pthread_mutex_unlock(&(presenzemx));
				clock_gettime(CLOCK_REALTIME, &timeS);
				tempoUscita=timeS.tv_sec*1000+timeS.tv_nsec/1000000;	//tempo all'uscita in millisecondi
				tempoPresenza=tempoUscita-tempoEntrata;
				dprintf(logfd,"Cliente %ld: |prodotti acquistati: %d|tempo tot. nel supermercato: %.3f|tempo in fila: %.3f|numero di code visitate: %d.\n",id,prodotti,(float) tempoPresenza/1000,(float) tempoInFila/1000,numCode);
				
				if(DEBUG) fprintf(stdout,"Cliente %ld servito dal cassiere %d!\n",mytid,cassa);
				
				return(NULL);
			}
			prec=listacasse[cassa].coda;
			while(prec->next!=pointer){
				prec=prec->next;
			}
			prec->next=pointer->next;
			free(pointer);
			Pthread_mutex_unlock(&(listacasse[cassa].mutexCassa));
			clock_gettime(CLOCK_REALTIME, &timeC);
			fineFila=timeC.tv_sec*1000+timeC.tv_nsec/1000000;
			tempoInFila+=(fineFila-inizioFila);
			Pthread_mutex_lock(&(presenzemx));
			clientiPresenti--;
			Pthread_mutex_unlock(&(presenzemx));
			clock_gettime(CLOCK_REALTIME, &timeS);
			tempoUscita=timeS.tv_sec*1000+timeS.tv_nsec/1000000;	//tempo all'uscita in millisecondi
			tempoPresenza=tempoUscita-tempoEntrata;
			dprintf(logfd,"Cliente %ld: |prodotti acquistati: %d|tempo tot. nel supermercato: %.3f|tempo in fila: %.3f|numero di code visitate: %d.\n",id,prodotti,(float) tempoPresenza/1000,(float) tempoInFila/1000,numCode);
			if(DEBUG) fprintf(stdout,"Cliente %ld servito dal cassiere %d!\n",mytid,cassa);
			
			return(NULL);							//il cliente esce dopo essere stato servito
		}
		else{							//se servito==-1 (cassa o supermercato chiusi)
			if(pointer==listacasse[cassa].coda){
				openSuper=listacasse[cassa].supermercato;
				listacasse[cassa].coda=listacasse[cassa].coda->next;
				free(pointer);
				Pthread_mutex_unlock(&(listacasse[cassa].mutexCassa));
				clock_gettime(CLOCK_REALTIME, &timeC);
				fineFila=timeC.tv_sec*1000+timeC.tv_nsec/1000000;
				tempoInFila+=(fineFila-inizioFila);
			}
			else{
				prec=listacasse[cassa].coda;
				openSuper=listacasse[cassa].supermercato;
				while(prec->next!=pointer){
					prec=prec->next;
				}
				prec->next=pointer->next;
				free(pointer);
				Pthread_mutex_unlock(&(listacasse[cassa].mutexCassa));
				clock_gettime(CLOCK_REALTIME, &timeC);
				fineFila=timeC.tv_sec*1000+timeC.tv_nsec/1000000;
				tempoInFila+=(fineFila-inizioFila);
			}
		}
		if(openSuper==0){
			addClienteQuit(&clientiQuit,mytid,clientiQuitmx);
			Pthread_mutex_lock(&(presenzemx));
			clientiPresenti--;
			Pthread_mutex_unlock(&(presenzemx));
			clock_gettime(CLOCK_REALTIME, &timeS);
			tempoUscita=timeS.tv_sec*1000+timeS.tv_nsec/1000000;	//tempo all'uscita in millisecondi
			tempoPresenza=tempoUscita-tempoEntrata;
			dprintf(logfd,"Cliente %ld: |prodotti acquistati: 0|tempo tot. nel supermercato: %.3f|tempo in fila: %.3f|numero di code visitate: %d.\n",id,(float) tempoPresenza/1000,(float) tempoInFila/1000,numCode);
			
			if(DEBUG) fprintf(stdout,"\nCliente esce dopo sigquit!!!\n");
			
			return(NULL);				//il cliente esce immediatamente dal supermercato perché chiuso con SIGQUIT
		}
		if(DEBUG) fprintf(stdout,"\nCliente cambia coda!!!\n");
	}
}


void* direttore(void* argNotUsed){
	sigset_t closureSigMask;
	int si;
	si=sigemptyset(&closureSigMask);
	if(si==-1){
		perror("director_sigemptyset");
	}
	si=sigaddset(&closureSigMask,SIGQUIT);
	if(si==-1){
		perror("director_sigaddset");
	}
	si=sigaddset(&closureSigMask,SIGHUP);
	if(si==-1){
		perror("director_sigaddset");
	}
	si=pthread_sigmask(SIG_SETMASK,&closureSigMask,NULL);				//il thread direttore maschera i segnali di chiusura per indirizzarli al thread main
	if(si!=0){
		fprintf(stderr,"Director pthread_sigmask returned with error: %d.\n",si);
		exit(EXIT_FAILURE);
	}
	int closeValue=valori->S1;
	int openValue=valori->S2;
	int cassetot=valori->K;
	int inter=(valori->intCom)+5;
	struct timespec temp={(inter/1000),(inter%1000)*1000000};
	int out;
	int casseaperte=valori->I;						//parametri di decisione per l'apertura o la chiusura delle casse
	int casseconmolticlienti;
	int casseconpochiclienti;
	int cassadachiudere;
	int i;
	pthread_t casstid;
	int erj;
	int j;
	int fatto;
	listaP* cl;
	pthread_t tid;
	while(1){
		Pthread_mutex_lock(&(openCloseMutex));
		if(openClose==0){						//il direttore controlla periodicamente che il supermercato non debba chiudere
			break;
			Pthread_mutex_unlock(&(openCloseMutex));
		}
		Pthread_mutex_unlock(&(openCloseMutex));
		out=nanosleep(&temp,NULL);				//attende l'intervallo di tempo della comunicazione con i cassieri (+ 5 millisecondi)
		if(out==-1) {perror("director nanosleep");}
		casseaperte=0;
		casseconmolticlienti=0;
		casseconpochiclienti=0;
		cassadachiudere=-1;
		Pthread_mutex_lock(&(openCloseMutex));
		if(openClose==0){
			break;
			Pthread_mutex_unlock(&(openCloseMutex));
		}
		Pthread_mutex_unlock(&(openCloseMutex));
		for(i=0;i<cassetot;i++){					//lettura dei dati comunicati dai cassieri
			Pthread_mutex_lock(&(communicationBuffer[i].lenmux));
			if(communicationBuffer[i].lencoda<=1 && communicationBuffer[i].lencoda!=-1){		//se ci sono casse con 0 o 1 cliente in coda
				casseconpochiclienti++;
				casseaperte++;
				cassadachiudere=i;
			}
			else{
				if(communicationBuffer[i].lencoda>=openValue){
					casseaperte++;
					casseconmolticlienti++;
				}
				else{
					if(communicationBuffer[i].lencoda!=-1){
						casseaperte++;
					}
				}
			}
			Pthread_mutex_unlock(&(communicationBuffer[i].lenmux));
		}
		Pthread_mutex_lock(&(openCloseMutex));
		if(openClose==0){
			break;
			Pthread_mutex_unlock(&(openCloseMutex));
		}
		Pthread_mutex_unlock(&(openCloseMutex));
		if(casseaperte==0){
			break;
		}
		if(cassadachiudere!=-1){
			Pthread_mutex_lock(&(communicationBuffer[cassadachiudere].lenmux));
			Pthread_mutex_lock(&(presenzemx));
			if(casseaperte==1 && communicationBuffer[cassadachiudere].lencoda==0 && clientiPresenti==0){
				Pthread_mutex_unlock(&(presenzemx));
				Pthread_mutex_unlock(&(communicationBuffer[cassadachiudere].lenmux));
				Pthread_mutex_lock(&(listacasse[cassadachiudere].mutexCassa));
				listacasse[cassadachiudere].open=0;					//se è rimasta aperta una sola cassa con 0 clienti in coda e non ci sono più clienti (SIGHUP)
				casstid=listacasse[cassadachiudere].cassiere;
				Pthread_cond_signal(&(listacasse[cassadachiudere].condcassa));
				Pthread_mutex_unlock(&(listacasse[cassadachiudere].mutexCassa));
				erj=pthread_join(casstid,NULL);							//attende la terminazione del cassiere
				if(erj!=0){
					errno=erj;
					perror("DCas_pthread_join");
					exit(EXIT_FAILURE);
				}
				break;
			}
			Pthread_mutex_unlock(&(presenzemx));
			Pthread_mutex_unlock(&(communicationBuffer[cassadachiudere].lenmux));
		}
		Pthread_mutex_lock(&(openCloseMutex));
		if(openClose==0){
			break;
			Pthread_mutex_unlock(&(openCloseMutex));
		}
		Pthread_mutex_unlock(&(openCloseMutex));
		if(casseconmolticlienti>0 && casseaperte<cassetot){
			j=0;
			fatto=0;
			while(fatto==0 && j<cassetot){
				Pthread_mutex_lock(&(listacasse[j].mutexCassa));					
				if(listacasse[j].open==0){
					listacasse[j].open=1;											//il direttore apre una cassa
					erj=pthread_create(&(listacasse[j].cassiere),NULL,&cassiere,&(listacasse[j].numerodicassa));
					if(erj!=0){
						fprintf(stdout, "SC pthread_create returned with error: %d while creating cashier\n", erj);
						exit(EXIT_FAILURE);
					}
					
					if(DEBUG) fprintf(stdout,"Cassiere creato!\n");
					
					fatto=1;
					casseaperte++;
				}
				Pthread_mutex_unlock(&(listacasse[j].mutexCassa));
				j++;
			}
		}
		else{
			if(casseconpochiclienti>=closeValue && casseaperte>1 && cassadachiudere!=-1){
			Pthread_mutex_lock(&(listacasse[cassadachiudere].mutexCassa));
			listacasse[cassadachiudere].open=0;							//il direttore chiude la cassa con uno o zero clienti in coda
			casstid=listacasse[cassadachiudere].cassiere;
			Pthread_cond_signal(&(listacasse[cassadachiudere].condcassa));
			Pthread_mutex_unlock(&(listacasse[cassadachiudere].mutexCassa));
			erj=pthread_join(casstid,NULL);							//attende la terminazione del cassiere
			if(erj!=0){
				errno=erj;
				perror("DCas_pthread_join");
				exit(EXIT_FAILURE);
				}
				casseaperte--;
				
				if(DEBUG) fprintf(stdout,"Cassa %d chiusa!\n",cassadachiudere);
				
			}
		}
		
		Pthread_mutex_lock(&(permessomx));
		while(listaPermessi!=NULL){					//se ci sono clienti in attesa del permesso per uscire
			cl=listaPermessi;
			while(cl!=NULL){
				cl->permesso=1;
				Pthread_cond_signal(&(cl->permessocond));	//il direttore concede il permesso di uscire ai clienti che non hanno effettuato acquisti
				tid=cl->tidcliente;
				cl=cl->next;
				Pthread_mutex_unlock(&(permessomx));
				erj=pthread_join(tid,NULL);					//attende l'uscita del cliente
					if(erj!=0){
					errno=erj;
					perror("DCli_pthread_join");
					exit(EXIT_FAILURE);
					}
				Pthread_mutex_lock(&(permessomx));
			}
		}
		Pthread_mutex_unlock(&(permessomx));
	}
	Pthread_mutex_lock(&(permessomx));
	while(listaPermessi!=NULL){					//se ci sono ancora clienti in attesa del permesso per uscire
		cl=listaPermessi;
		while(cl!=NULL){
			cl->permesso=1;
			Pthread_cond_signal(&(cl->permessocond));	//il direttore concede il permesso di uscire ai clienti che non hanno effettuato acquisti, prima di terminare
			tid=cl->tidcliente;
			cl=cl->next;
			Pthread_mutex_unlock(&(permessomx));
			erj=pthread_join(tid,NULL);					//attende l'uscita del cliente
				if(erj!=0){
				errno=erj;
				perror("DCli_pthread_join");
				exit(EXIT_FAILURE);
				}

			if(DEBUG) fprintf(stdout,"CLIENTE USCITO PRIMA DELLA TERMINAZIONE DEL DIRETTORE!\n");

			Pthread_mutex_lock(&(permessomx));
		}
	}
	Pthread_mutex_unlock(&(permessomx));
	
	if(DEBUG) fprintf(stdout,"Direttore terminato!\n");
	
	return(NULL);
	
}


volatile sig_atomic_t sighup=0;		//variabili globali settate dall'handler dei segnali di chiusura, al momento della ricezione
volatile sig_atomic_t sigquit=0;

static void signalHandler(int signum){	//gestore segnali di chiusura del supermercato
	if(signum==SIGHUP) {sighup=1;}
	if(signum==SIGQUIT) {sigquit=1;}
}

int main(int argc, char** argv){

// ----------------------- INIZIALIZZAZIONE SIMULAZIONE -----------------------

	configurazione_iniziale(&valori);

	int i;
	listacasse=Malloc(valori->K*sizeof(strcassa));
	for(i=0;i<(valori->K);i++){					//inizializzazione valori array delle casse
		listacasse[i].numerodicassa=i;
		listacasse[i].open=0;
		listacasse[i].supermercato=1;
		listacasse[i].clientiServiti=0;
		listacasse[i].prodottiVenduti=0;
		listacasse[i].tempoApertura=0;
		listacasse[i].tempoServizio=0;
		listacasse[i].numChiusure=0;
		listacasse[i].cassiere=0;
		listacasse[i].coda=NULL;
		pthread_mutex_init(&(listacasse[i].mutexCassa),NULL);
		if(pthread_cond_init(&(listacasse[i].condcassa),NULL)!=0){
			fprintf(stderr,"pthread_cond_init failed");
			exit(EXIT_FAILURE);
		}
	}
	
	communicationBuffer=Malloc(valori->K*sizeof(comunicazioneDirettoreCassieri));
	for(i=0;i<(valori->K);i++){
		communicationBuffer[i].lencoda=-1;				//inizializzazione buffer di comunicazione cassieri-direttore
		pthread_mutex_init(&(communicationBuffer[i].lenmux),NULL);
	}

	struct sigaction newSignalsAct;					//inizializzazione gestione segnali di chiusura
	memset(&newSignalsAct, 0, sizeof(newSignalsAct));
	sigset_t sigmask;
	i=sigemptyset(&sigmask);
	if(i==-1){
		perror("main_sigemptyset");
	}
	i=sigaddset(&sigmask,SIGQUIT);
	if(i==-1){
		perror("main_sigaddset");
	}
	i=sigaddset(&sigmask,SIGHUP);
	if(i==-1){
		perror("main_sigaddset");
	}
	newSignalsAct.sa_mask=sigmask;		//maschera i due segnali durante l'esecuzione dell'handler
	newSignalsAct.sa_handler=signalHandler;
	i=sigaction(SIGHUP,&newSignalsAct,NULL);
	if(i==-1){
		perror("sigaction");
	}
	i=sigaction(SIGQUIT,&newSignalsAct,NULL);
	if(i==-1){
		perror("sigaction");
	}
	
	pthread_t *casseAperteAllaChiusura=Malloc(valori->K*sizeof(pthread_t));		//array che servirà alla fine della simulazione per la chiusura delle casse
	for(i=0;i<(valori->K);i++){
		casseAperteAllaChiusura[i]=0;
	}
	
	char* logname=strcat(valori->logFileName,".log");
	if(DEBUG) {fprintf(stdout,"Nome file di log completo: %s\n",logname);}
	if((logfd = open(logname, O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1){		//apro il file di log in scrittura
		perror("Log file, in apertura");
		exit(EXIT_FAILURE);
	}

// ----------------------- APERTURA SUPERMERCATO -----------------------

	openClose=1;

	i=0;
	while(i<valori->I){			//apertura delle prime I casse
		listacasse[i].open=1;
		int err=pthread_create(&(listacasse[i].cassiere),NULL,&cassiere,&(listacasse[i].numerodicassa));	//passo alla funzione cassiere il numero di cassa (i) che deve gestire
		if(err!=0){
			fprintf(stdout, "SC pthread_create returned with error: %d while creating cashiers\n", err);
			exit(EXIT_FAILURE);
			}
		i++;
		}

	int clients=0;
	pthread_t tidaux;
	while(clients<valori->C){							//generazione dei primi C clienti che entrano contemporaneamente nel supermercato
		int err=pthread_create(&(tidaux),NULL,&cliente,NULL);
		if(err!=0){
			fprintf(stdout, "SC pthread_create returned with error: %d while creating clients\n", err);
			exit(EXIT_FAILURE);
			}
		Pthread_mutex_lock(&(presenzemx));
		clientiPresenti++;
		Pthread_mutex_unlock(&(presenzemx));
		clients++;
		}

	pthread_t tid_dir;
	int erd=pthread_create(&(tid_dir),NULL,&direttore,NULL);			//generazione thread direttore del supermercato
	if(erd!=0){
		fprintf(stdout, "SC pthread_create returned with error: %d while creating director\n", erd);
		exit(EXIT_FAILURE);
	}

// ----------------------- SIMULAZIONE IN CORSO -----------------------

	int clientimax=valori->C;
	int clientientranti=valori->E;
	int intervallosleep=300;
	struct timespec temp={(intervallosleep/1000),(intervallosleep%1000)*1000000};
	int out;
	int err;
	while(sighup==0 && sigquit==0){							//generazione clienti fintanto che il supermercato è aperto
		out=nanosleep(&temp,NULL);
		if(out==-1){
			if(errno!=EINTR){
				perror("Simulation main nanosleep");
				exit(EXIT_FAILURE);
				}
			break;
		}
		Pthread_mutex_lock(&(presenzemx));
		if(DEBUG) fprintf(stdout,"\nCLIENTI PRESENTI 1: %d.\n\n",clientiPresenti);
		if(clientiPresenti<=(clientimax-clientientranti)){
			while(clientiPresenti<clientimax){
				clientiPresenti++;
				err=pthread_create(&(tidaux),NULL,&cliente,NULL);
				if(err!=0){
					fprintf(stdout, "SC pthread_create returned with error: %d while creating clients\n", err);
					exit(EXIT_FAILURE);
				}
			}
		}
		Pthread_mutex_unlock(&(presenzemx));
	}

// ----------------------- CHIUSURA SUPERMERCATO -----------------------

	if(sigquit){
		fprintf(stdout,"Ricevuto segnale SIGQUIT\n");
		fprintf(stdout,"Attendere la chiusura del supermercato.\n");
		Pthread_mutex_lock(&(openCloseMutex));
		openClose=0;
		int z;
		z=pthread_cond_broadcast(&(supermercatoChiusoCond));		//il main fa uscire immediatamente tutti i clienti che stanno ancora facendo acquisti
		if(z!=0){
			fprintf(stderr,"pthread_cond_broadcast failed! Program termination.\n");
			exit(EXIT_FAILURE);
		}
		Pthread_mutex_unlock(&(openCloseMutex));
		Pthread_mutex_lock(&(permessomx));
		uscireSenzaPermesso=1;					//il main dice ai clienti senza acquisti di uscire immediatamente senza attendere il permesso del direttore
		Pthread_mutex_unlock(&(permessomx));
		for(z=0;z<valori->K;z++){
			Pthread_mutex_lock(&(listacasse[z].mutexCassa));
			listacasse[z].supermercato=0;
			if(listacasse[z].open==1){
				casseAperteAllaChiusura[z]=listacasse[z].cassiere;
				Pthread_cond_signal(&(listacasse[z].condcassa));
			}
			Pthread_mutex_unlock(&(listacasse[z].mutexCassa));
		}
		int o;
		for(z=0;z<valori->K;z++){
			if(casseAperteAllaChiusura[z]!=0){
				o=pthread_join(casseAperteAllaChiusura[z],NULL);		//il thread main attende la terminazione dei cassieri attivi al momento della chiusura
				if(o!=0){
					errno=o;
					perror("MQCas_pthread_join");
					exit(EXIT_FAILURE);
				}
			}
		}
		o=pthread_join(tid_dir,NULL);					//il thread main attende la terminazione del direttore
		if(o!=0){
			errno=o;
			perror("MQDir_pthread_join");
			exit(EXIT_FAILURE);
		}
		Pthread_mutex_lock(&(presenzemx));
		while(clientiPresenti>0){			//se tutti i clienti non sono ancora usciti, il thread main si sospende per un poco per permettere a tutti di uscire (poco probabile)
			Pthread_mutex_unlock(&(presenzemx));
			out=nanosleep(&temp,NULL);
			if(out==-1) {perror("main nanosleep");}
			
			if(DEBUG) fprintf(stdout,"\n\n\nMain ha dormito per attendere i clienti!!!\n\n\n");
			
			Pthread_mutex_lock(&(presenzemx));
		}
		Pthread_mutex_unlock(&(presenzemx));
		Pthread_mutex_lock(&(clientiQuitmx));
		clientiQuit_t* aux=NULL;
		while(clientiQuit!=NULL){
			
			if(DEBUG) fprintf(stdout,"Atteso dal main il cliente %ld.\n",clientiQuit->tidcli);
			
			o=pthread_join(clientiQuit->tidcli,NULL);		//il thread main fa la join su tutti i clienti terminati senza essersi messi in nessuna coda (e senza permesso)
			if(o!=0){
				errno=o;
				perror("MQCli_pthread_join");
				exit(EXIT_FAILURE);
			}
			aux=clientiQuit;
			clientiQuit=clientiQuit->next;
			free(aux);
		}
		Pthread_mutex_unlock(&(clientiQuitmx));
		int clientiTotali=0;
		int prodottiTotali=0;
		for(z=0;z<(valori->K);z++){
			clientiTotali+=listacasse[z].clientiServiti;
			prodottiTotali+=listacasse[z].prodottiVenduti;
			if(listacasse[z].clientiServiti==0){
				dprintf(logfd,"Cassa %d: |n. prodotti elaborati: %d|n. clienti serviti: %d|tempo tot. di apertura: %.3f|tempo medio di servizio: ND|numero chiusure: %d.\n",z,listacasse[z].prodottiVenduti,listacasse[z].clientiServiti,(float) listacasse[z].tempoApertura/1000,listacasse[z].numChiusure);
			}
			else{
				dprintf(logfd,"Cassa %d: |n. prodotti elaborati: %d|n. clienti serviti: %d|tempo tot. di apertura: %.3f|tempo medio di servizio: %.3f|numero chiusure: %d.\n",z,listacasse[z].prodottiVenduti,listacasse[z].clientiServiti,(float) listacasse[z].tempoApertura/1000,((float) listacasse[z].tempoServizio/1000)/listacasse[z].clientiServiti,listacasse[z].numChiusure);
			}
		}
		dprintf(logfd,"Numero totale di clienti serviti: %d.\n",clientiTotali);
		dprintf(logfd,"Numero totale di prodotti venduti: %d.\n",prodottiTotali);
		if(close(logfd) == -1){
			perror("Closing log file");
			exit(EXIT_FAILURE);
		}
		free(valori->logFileName);
		free(valori);
		free(listacasse);
		free(communicationBuffer);
		free(casseAperteAllaChiusura);
		fprintf(stdout,"Simulazione terminata.\n");
		return 0;
	}

	if(sighup){
		fprintf(stdout,"Ricevuto segnale SIGHUP\n");
		fprintf(stdout,"Attendere la chiusura del supermercato.\n");
		int waitDir;
		int z;
		int clientiTotali=0;
		int prodottiTotali=0;
		waitDir=pthread_join(tid_dir,NULL);
		if(waitDir!=0){
			errno=waitDir;
			perror("MHDir_pthread_join");
			exit(EXIT_FAILURE);
		}
		for(z=0;z<(valori->K);z++){
			clientiTotali+=listacasse[z].clientiServiti;
			prodottiTotali+=listacasse[z].prodottiVenduti;
			if(listacasse[z].clientiServiti==0){
				dprintf(logfd,"Cassa %d: |n. prodotti elaborati: %d|n. clienti serviti: %d|tempo tot. di apertura: %.3f|tempo medio di servizio: ND|numero chiusure: %d.\n",z,listacasse[z].prodottiVenduti,listacasse[z].clientiServiti,(float) listacasse[z].tempoApertura/1000,listacasse[z].numChiusure);
			}
			else{
				dprintf(logfd,"Cassa %d: |n. prodotti elaborati: %d|n. clienti serviti: %d|tempo tot. di apertura: %.3f|tempo medio di servizio: %.3f|numero chiusure: %d.\n",z,listacasse[z].prodottiVenduti,listacasse[z].clientiServiti,(float) listacasse[z].tempoApertura/1000,((float) listacasse[z].tempoServizio/1000)/listacasse[z].clientiServiti,listacasse[z].numChiusure);
			}
		}
		dprintf(logfd,"Numero totale di clienti serviti: %d.\n",clientiTotali);
		dprintf(logfd,"Numero totale di prodotti venduti: %d.\n",prodottiTotali);
		if(close(logfd) == -1){
			perror("Closing log file");
			exit(EXIT_FAILURE);
		}
		free(valori->logFileName);
		free(valori);
		free(listacasse);
		free(communicationBuffer);
		free(casseAperteAllaChiusura);
		fprintf(stdout,"Simulazione terminata.\n");
		return 0;
	}
	return 0;
}

