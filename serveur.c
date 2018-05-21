/* version 0 (PM, 16/4/17) :
	Le serveur de conversation
	- crée un tube (fifo) d'écoute (avec un nom fixe : ./ecoute)
	- gère un maximum de maxParticipants conversations : 
		* accepter les demandes de connexion tube d'écoute) de nouveau(x) participant(s)
			 si possible
			-> initialiser et ouvrir les tubes de service (entrée/sortie) fournis 
				dans la demande de connexion
		* messages des tubes (fifo) de service en entrée 
			-> diffuser sur les tubes de service en sortie
	- détecte les déconnexions lors du select
	- se termine à la connexion d'un client de pseudo "fin"
	Protocole
	- suppose que les clients ont créé les tube d'entrée/sortie avant
		la demande de connexion, nommés par le nom du client, suffixés par _C2S/_S2C.
	- les échanges par les tubes se font par blocs de taille fixe, dans l'idée d'éviter
	  le mode non bloquant.
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <stdbool.h>
#include <sys/stat.h>

#define NBPARTICIPANTS 5 	/* seuil au delà duquel la prise en compte de nouvelles
								 connexions sera différée */
#define TAILLE_MSG 128		/* nb caractères message complet (nom+texte) */
#define TAILLE_NOM 25		/* nombre de caractères d'un pseudo */
#define NBDESC FD_SETSIZE-1	/* pour un select éventuel
								 (macros non definies si >= FD_SETSIZE) */
#define TAILLE_TUBE 512		/*capacité d'un tube */

typedef struct ptp {
    bool actif;
    char nom [TAILLE_NOM];
    int in;		/* tube d'entrée */
    int out;	/* tube de sortie */
} participant;

typedef struct dde {
    char nom [TAILLE_NOM];
} demande;

static const int maxParticipants = NBPARTICIPANTS+1+TAILLE_TUBE/sizeof(demande);

participant participants [NBPARTICIPANTS+1+TAILLE_TUBE/sizeof(demande)]; //maxParticipants
char buf[TAILLE_TUBE];
int nbactifs = 0;

void effacer(int i) {
    participants[i].actif = false;
    bzero(participants[i].nom, TAILLE_NOM*sizeof(char));
    participants[i].in = -1;
    participants[i].out = -1;
}

void diffuser(char *dep) {
    int i, res, count;
    fd_set writefds;
    
    count = 0;
    FD_ZERO(&writefds);
    for (i=0; i<maxParticipants; i++) {
	if (participants[i].actif) {
	    FD_SET(participants[i].out,&writefds);
	    count++;
	}
    }
    if (count > 0) {
	res = select(NBDESC,NULL,&writefds,NULL,NULL);
	if (res == -1) {
	    perror("select");
	    exit(1);
	} else {
	    for (i=0; i<maxParticipants; i++) {
		if (participants[i].actif) {
		    if (FD_ISSET(participants[i].out, &writefds) != 0) {
			if (write(participants[i].out,dep,TAILLE_MSG) == -1) {
			    perror("write");
			    exit(1);
			}
		    }
		} 
	    }
	}
    }
}

void desactiver (int p) {
/* traitement d'un participant déconnecté */
    char message[TAILLE_MSG];

    if (participants[p].actif) {
	close(participants[p].in);
        close(participants[p].out);
	strcpy(message,"[service]");
	strcat(message,strcat(participants[p].nom, " quitte la conversation."));
	effacer(p);
	diffuser(message);
	nbactifs--;
    }
}

void ajouter(char *dep) {
/*  Pré : nbactifs < maxParticpants
	
	Ajoute un nouveau participant de pseudo dep.
	Si le participant est "fin", termine le serveur.
*/
    int i, descin, descout;
    char tubeC2S[TAILLE_NOM+5];
    char tubeS2C[TAILLE_NOM+5];
    char message[TAILLE_MSG];
    bool ajoute;
    ajoute = false;
    if (strcmp(dep,"fin") != 0) {
	i=0;
	while (i<=maxParticipants && !ajoute) {
	    if (!participants[i].actif) {
		participants[i].actif = true;
		strcpy(participants[i].nom,dep);

		strcpy(tubeC2S,dep);
		strcat(tubeC2S,"_C2S");
		if ((descin = open(tubeC2S,O_RDONLY)) == -1) {
		    perror("open");
		    exit(1);
		}
		
		strcpy(tubeS2C,dep);
		strcat(tubeS2C,"_S2C");
		if ((descout = open(tubeS2C,O_WRONLY)) == -1) {
		    perror("open");
		    exit(1);
		}

		
		participants[i].in = descin;
		participants[i].out = descout;
		strcpy(message,"[service]");
		strcat(message,strcat(dep, " rejoint la conversation."));
		diffuser(message);
		nbactifs++;
		ajoute = true;
	    }
	    i++;
	}
	
    } else {
	diffuser("[service]Deconnexion du serveur");
	sleep(3);
	for (i=0; i<maxParticipants; i++) {
	    desactiver(i);
	}
	printf("fin serveur\n");
	exit(0);
    }
}

int main (int argc, char *argv[]) {
    int i,nlus,res;
    int ecoute;		/* descripteur d'écoute */
    fd_set readfds;	/* ensemble de descripteurs écoutés par un select éventuel */
    char buf0[TAILLE_NOM];   /* pour parcourir le contenu d'un tube, si besoin */
    char message[TAILLE_MSG];

    /* création (puis ouverture) du tube d'écoute */
    mkfifo("./ecoute",S_IRUSR|S_IWUSR); // mmnémoniques sys/stat.h: S_IRUSR|S_IWUSR = 0600
    ecoute=open("./ecoute",O_RDONLY);

    for (i=0; i<maxParticipants; i++) {
        effacer(i);
    }
    FD_ZERO(&readfds);
    while (true) {
        printf("participants actifs : %d\n",nbactifs);
	/* boucle du serveur : traiter les requêtes en attente 
	    sur le tube d'écoute et les tubes d'entrée
	*/

	FD_ZERO(&readfds);
	if (nbactifs < maxParticipants) {
	    FD_SET(ecoute,&readfds);
	}
	for (i=0; i<maxParticipants; i++) {
	    if (participants[i].actif) {
		FD_SET(participants[i].in, &readfds);
	    }
	}
	
	res = select(NBDESC, &readfds, NULL, NULL, NULL);
	if (res == -1) {
	    perror("select");
	    exit(1);
	} else {
	    if (FD_ISSET(ecoute,&readfds) != 0) {
		nlus = read(ecoute,buf0,TAILLE_NOM);
		if (nlus == -1) {
		    perror("read");
		    exit(1);
		} else if (nlus > 0) {
		    ajouter(buf0);
		    bzero(buf0,TAILLE_NOM);
		}
	    }

	    for (i=0; i<maxParticipants; i++) {
		if (participants[i].actif) {
		    if (FD_ISSET(participants[i].in, &readfds) != 0) {
			bzero(buf,TAILLE_TUBE);
			nlus = read(participants[i].in, buf, TAILLE_MSG-(TAILLE_NOM+2));
			if (nlus == -1) {
			    perror("read");
			    exit(1);
			} else if (nlus > 0) {
			    strcpy(message,"[");
			    strcat(message,participants[i].nom);
			    strcat(message,"]");
			    strcat(message,buf);
			    diffuser(message);
			    if (strcmp(buf,"au revoir") == 0) {
				desactiver(i);
			    }			    
	    		}
		    }
		}
	    }
	}
    }
}
