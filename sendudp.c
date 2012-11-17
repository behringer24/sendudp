/*
 *	sendudp - send input strings via UDP datagrams
 *	(c)1998,2000 Pitt Murmann
 */

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VERSION "version 0.2\n"
#define DGSIZE	65536

void usage()
{
	(void)fprintf(stderr, "usage: sendudp [-i <string>] <address>:<port> [<file>*]\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	int i, l;
	int fd = -1;
	int presize = 0;
	int port = 0;
	int cutlf = 1;
	char *cp, *mp, *bod;
	char *prepend = NULL;
	struct sockaddr_in server;
	struct in_addr **list;
	struct hostent *host;
	FILE *f;

	while((i = getopt(argc, (char ** const)argv, "hni:v")) != EOF) {
		switch(i) {
		case 'h':
			usage();
			break;
		case 'i':
			prepend = optarg;
			presize = strlen(prepend) + 1;
			break;
		case 'n':
			cutlf = 0;
			break;
		case 'v':
			(void)fprintf(stderr, VERSION);
			exit(EXIT_SUCCESS);
			break;
		default:
			usage();
			break;
		}
	}

	if(argv[optind] == NULL)
		usage();
	cp = argv[optind];
	while(*cp != '\0') {
		if(*cp == ':')
			break;
		cp++;
	}
	if(*cp == ':') {
		*cp = '\0';
		cp++;
		mp = cp;
		while(*cp != '\0') {
			if(! isdigit((int)*cp))
				usage();
			cp++;
		}
		port = atoi((const char *)mp);
		if(port < 1 || port > 65535) {
			(void)fprintf(stderr, "Invalid port\n");
			exit(EXIT_FAILURE);
		}
	}
	else
		usage();

	/* network */
	if((host = gethostbyname((const char*)argv[optind])) == NULL) {
		(void)fprintf(stderr, "Unknown host: %s\n", argv[optind]);
		exit(EXIT_FAILURE);
	}
	list = (struct in_addr **)host->h_addr_list;
	while(*list != NULL) {
		if((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
			(void)perror("Unable to create socket");
			exit(EXIT_FAILURE);
		}
		memset(&server, 0, sizeof(server));
		server.sin_family = AF_INET;
		server.sin_port = htons(port);
		memcpy(&server.sin_addr, *list, sizeof(struct in_addr));
		if(connect(fd, (const struct sockaddr *)&server, sizeof(server)) == 0)
			break;
		close(fd);
		list++;
	}
	if(list == NULL) {
		(void)fprintf(stderr, "Unable to connect\n");
		exit(EXIT_FAILURE);
	}

	/* input */
	if((mp = (char *)malloc(DGSIZE)) == NULL) {
		(void)fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}
	if(prepend == NULL) {
		bod = mp;
	}
	else {
		strcpy(mp, prepend);
		*(mp + presize - 1) = ' ';
		bod = mp + presize;
	}

	optind++;
	if(argv[optind] == NULL) {
		i = DGSIZE - presize;
		while(fgets(bod, i, stdin) != NULL) {
			l = strlen(bod);
			if(cutlf) {
				if(*(bod + l - 1) == '\n')
					l--;
			}
			if(l < 1)
				continue;
			*(bod + l) = '\0';
			l += presize;
			if(write(fd, mp, l) != l) {
				(void)perror("Unable to transmit data");
				exit(EXIT_FAILURE);
			}
		}
	}
	else {
		while(argv[optind] != NULL) {
			if((f = fopen(argv[optind], "r")) == NULL) {
				(void)perror(argv[optind]);
				exit(EXIT_FAILURE);
			}
			i = DGSIZE - presize;
			while(fgets(bod, i, f) != NULL) {
				l = strlen(bod);
				if(cutlf) {
					if(*(bod + l - 1) == '\n')
						l--;
				}
				if(l < 1)
					continue;
				*(bod + l) = '\0';
				l += presize;
				if(write(fd, mp, l) != l) {
					(void)perror("Unable to transmit data");
					exit(EXIT_FAILURE);
				}
			}
			if(! feof(f))
				(void)perror(argv[optind]);
			(void)fclose(f);
			optind++;
		}
	}
	free(mp);
	exit(EXIT_SUCCESS);
}
