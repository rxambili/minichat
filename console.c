/* version 0 (PM, 16/4/17) :
	Le client de conversation
	- crée deux tubes (fifo) d'E/S, nommés par le nom du client, suffixés par _C2S/_S2C
	- demande sa connexion via le tube d'écoute du serveur (nom supposé connu),
		 en fournissant le pseudo choisi (max TAILLE_NOM caractères)
	- attend la réponse du serveur sur son tube _C2S
	- effectue une boucle : lecture sur clavier/S2C.
	- sort de la boucle si la saisie au clavier est "au revoir"
	Protocole
	- les échanges par les tubes se font par blocs de taille fixe TAILLE_MSG,
	- le texte émis via C2S est préfixé par "[pseudo] ", et tronqué à TAILLE_MSG caractères
Notes :
	-le  client de pseudo "fin" n'entre pas dans la boucle : il permet juste d'arrêter 
		proprement la conversation.
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>

#define TAILLE_MSG 128		/* nb caractères message complet (nom+texte) */
#define TAILLE_NOM 25		/* nombre de caractères d'un pseudo */
#define NBDESC FD_SETSIZE-1  /* pour le select (macros non definies si >= FD_SETSIZE) */
#define TAILLE_TUBE 512		/*capacité d'un tube */
#define NB_LIGNES 20
#define TAILLE_SAISIE 1024

char pseudo [TAILLE_NOM];
char buf [TAILLE_TUBE];
char discussion [NB_LIGNES] [TAILLE_MSG]; /* derniers messages reçus */

void afficher(int depart) {
    int i;
    for (i=1; i<=NB_LIGNES; i++) {
        printf("%s\n", discussion[(depart+i)%NB_LIGNES]);
    }
    printf("=========================================================================\n");
}

int main (int argc, char *argv[]) {
    int nlus,necrits,res;
    
    int ecoute, S2C, C2S;			/* descripteurs tubes */
    int curseur = 0;				/* position dernière ligne reçue */

    fd_set readfds; 				/* ensemble de descripteurs écoutés par le select */

    char tubeC2S [TAILLE_NOM+5];	/* pour le nom du tube C2S */
    char tubeS2C [TAILLE_NOM+5];	/* pour le nom du tube S2C */
    char pseudo [TAILLE_NOM];
    char message [TAILLE_MSG];
    char saisie [TAILLE_SAISIE];
    char *positionEntree;
    positionEntree = NULL;

    if (!(argc == 2) && (strlen(argv[1]) < TAILLE_NOM*sizeof(char))) {
        printf("utilisation : %s <pseudo>\n", argv[0]);
        printf("Le pseudo ne doit pas dépasser 25 caractères\n");
        exit(1);
    }
    strcpy(pseudo,argv[1]); 

    /* ouverture du tube d'écoute */
    ecoute = open("./ecoute",O_WRONLY);
    if (ecoute==-1) {
        printf("Le serveur doit être lance, et depuis le meme repertoire que le client\n");
        exit(2);
    }
    /* création des tubes de service */
    strcpy(tubeC2S,pseudo);
    strcat(tubeC2S,"_C2S");

    strcpy(tubeS2C,pseudo);
    strcat(tubeS2C,"_S2C");

    mkfifo(tubeC2S,S_IRUSR|S_IWUSR);
    mkfifo(tubeS2C,S_IRUSR|S_IWUSR);


    /* connexion */

    necrits = write(ecoute,pseudo, TAILLE_NOM);
    if (necrits == -1) {
	perror("connexion");
	exit(1);
    }

        
    if (strcmp(pseudo,"fin")!=0) {
    	/* client " normal " */
	/* initialisations */
	
	C2S=open(tubeC2S,O_WRONLY);
	S2C=open(tubeS2C,O_RDONLY);
	
	strcpy(saisie,"");
        while (strcmp(saisie,"au revoir")!=0) {
	    /* boucle principale */

	    FD_ZERO(&readfds);
	    FD_SET(S2C,&readfds);
	    FD_SET(0,&readfds);

	    res=select(NBDESC,&readfds,NULL,NULL,NULL);
	    if (res == -1) {
		perror("select");
		exit(1);
	    } else {
		if (FD_ISSET(0,&readfds) != 0) {
		    bzero(saisie, TAILLE_SAISIE);
		    nlus = read(0,saisie,TAILLE_SAISIE);
		    if (nlus == -1) {
			perror("saisie");
			exit(1);
		    }

		    /* on enlève le retour à la ligne.*/
		    positionEntree = strchr(saisie, '\n');
		    if (positionEntree != NULL) {
			*positionEntree = '\0';
		    }
		    necrits = write(C2S,saisie, TAILLE_MSG-(TAILLE_NOM+2));
		    if (necrits == -1) {
			perror("envoi");
			exit(1);
		    }
		    
		}
		if (FD_ISSET(S2C,&readfds) != 0) {
		    bzero(message, TAILLE_MSG);
		    nlus = read(S2C,message,TAILLE_MSG);
		    if (nlus == -1) {
			perror("reception");
			exit(1);
		    }
		    strcpy(discussion[curseur],message);
		    afficher(curseur);
		    curseur = (curseur+1)%NB_LIGNES;
		    if (strcmp(message,"[service]Deconnexion du serveur") == 0) {
			break;
		    }
		}

	    }
	    	
        }
	sleep(3);
	close(C2S);
	close(S2C);
    }
    /* nettoyage */
    close(ecoute);
    bzero(message, TAILLE_MSG);
    bzero(saisie, TAILLE_SAISIE);

    printf("fin client\n");
    exit (0);
}
