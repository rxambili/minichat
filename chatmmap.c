/* version 0.1 (PM, 23/4/17) :
	La discussion est un tableau de messages, couplé en mémoire partagée.
	Un message comporte un auteur, un texte et un numéro d'ordre (croissant).
	Le numéro d'ordre permet à chaque participant de détecter si la discussion a évolué
	depuis la dernière fois qu'il l'a affichée.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h> /* définit mmap  */
#include <signal.h>

#define TAILLE_AUTEUR 25
#define TAILLE_TEXTE 128
#define NB_LIGNES 20

/* message : numéro d'ordre, auteur (25 caractères max), texte (128 caractères max) */
struct message {
  int numero;
  char auteur [TAILLE_AUTEUR];
  char texte [TAILLE_TEXTE];
};

/* discussion (20 derniers messages) */
struct message * discussion;

/* dernier message reçu */
int dernier0 = 0 ;

/* afficher la discussion */
void afficher() {
	int i;
	for (i=0; i<NB_LIGNES; i++) {
		printf("[%s] : %s\n", discussion[i].auteur, discussion[i].texte);
	}
	printf("=========================================================================\n");
}

/* traitant : rafraichir la discussion, s'il y a lieu */
void traitant (int sig) {
	/* à faire */
	if (discussion[NB_LIGNES-1].numero > dernier0) {
		afficher();
		dernier0 = discussion[NB_LIGNES-1].numero;
	}
}

/* envoi un message */
void envoyer(struct message m) {
	int i;
	m.numero = dernier0+1;
	for (i=0; i<NB_LIGNES-1; i++) {
		discussion[i] = discussion[i+1];
	}
	discussion[NB_LIGNES-1] = m;
	kill(0,SIGUSR1);	
}
	

int main (int argc, char *argv[]) { 
	struct message m;
	struct message *tmp;
	int i,taille,fdisc, nlus;
 	char qq [1];
 	FILE * fdf;
	char *positionEntree;
	positionEntree = NULL;

	signal(SIGUSR1,traitant);

	
	if (argc != 3) {
		printf("usage: %s <discussion> <participant>\n", argv[0]);
		exit(1);
	}

	 /* ouvrir et coupler discussion */
	if ((fdisc = open (argv[1], O_RDWR | O_CREAT, 0666)) == -1) {
		printf("erreur ouverture discussion\n");
		exit(2);
	}
	
	/*	mmap ne spécifie pas quel est le resultat d'une ecriture *apres* la fin d'un 
		fichier couple (SIGBUS est une possibilite, frequente). Il faut donc fixer la 
		taille du fichier destination à la taille du fichier source *avant* le couplage. 
		On utilise ici lseek (a la taille du fichier source) + write d'un octet, 
		qui sont deja connus.
	*/
	qq[0]='x';
	taille = sizeof(struct message)*NB_LIGNES;
 	lseek (fdisc, taille, SEEK_SET);
 	write (fdisc, qq, 1);
 	
 	/* à compléter : saisie des messages, gestion de la discussion*/
	 if ((discussion = mmap (NULL, taille, PROT_READ | PROT_WRITE , MAP_SHARED, fdisc, 0)) == (caddr_t) -1) {
 	/* MAP_SHARED est necessaire pour que les ecritures soient visibles dans le fichier */
   		printf("erreur couplage destination\n");
   		exit(6);
	}
	
	if (discussion[NB_LIGNES - 1].numero != NULL) {
		dernier0 = discussion[NB_LIGNES - 1].numero;
	}
	
	/* envoi du message service de connexion*/
	strcpy(m.auteur,"service");
	strcpy(m.texte,argv[2]);
	strcat(m.texte," rejoint la conversation");
	envoyer(m);
			
 	strcpy(m.auteur,argv[2]);	
	strcpy(m.texte,"");
	fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
	
	while (strcmp(m.texte,"au revoir") != 0) {
		bzero(m.texte,TAILLE_TEXTE);
		nlus = read(0,m.texte,TAILLE_TEXTE);
		if (nlus > 0) {
			/* on enlève le retour à la ligne.*/
			positionEntree = strchr(m.texte, '\n');
			if (positionEntree != NULL) {
				*positionEntree = '\0';
			}
				
			envoyer(m);
		}
		kill(0,SIGUSR1);
		
	}
	/* envoi du message service de deconnexion*/
	strcpy(m.auteur,"service");
	strcpy(m.texte,argv[2]);
	strcat(m.texte," quitte la conversation");
	envoyer(m);

		
	close(fdisc);
 	exit(0);
}
