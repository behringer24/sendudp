/*
 * yasul - yet another simple udp logger
 * https://github.com/behringer24/yasul
 * 	
 * Fork of:
 * udplogd - receive and log UDP datagrams
 * (c)1996,2000 Pitt Murmann
 * Copyright: GPL
 */

#define VERSION	"yasul version 0.1.0 (udplogd fork)\n"

#include <arpa/inet.h>
#ifdef SOLARIS
#include <sys/byteorder.h>
#endif
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <resolv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#define DGSIZE	65536

#define	STAMP_NONE	0
#define	STAMP_TIME	8
#define	STAMP_UNIX	10
#define STAMP_DAY	11
#define STAMP_MONTH	14
#define STAMP_YEAR	17
#define STAMP_FULL	19

#define FILEMODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#ifndef CFGFILE
#define CFGFILE	"/sbin/yasul.conf"
#endif

int debug = 0;
int ffd = STDOUT_FILENO;
int comm[2];
char *mp = NULL;
char *filename = NULL;
char *configfile = CFGFILE;
char *timebuffer = NULL;
char *suffix = NULL;
int fileindex = STAMP_NONE;
mode_t filemode = FILEMODE;
int fileflag = O_WRONLY|O_CREAT|O_APPEND;
char *address = "0.0.0.0";
int port = 0;
int tmode = STAMP_UNIX;
char addlf = 1;
char addsrcaddr = 0;
char retcode;
unsigned long int limit = 0;
unsigned long int count = 0;
time_t now;
struct tm *t;

/*
 *	Miscellaneous error functions.
 */

void error_memory()
{
	(void)fprintf(stderr, "Unable to allocate memory\n");
	exit(EXIT_FAILURE);
}

void error_signal()
{
	(void)fprintf(stderr, "Unable to set up signal handler\n");
	exit(EXIT_FAILURE);
}

void error_configfile()
{
	(void)fprintf(stderr, "Unable to open config file\n");
	exit(EXIT_FAILURE);
}

void error_syntax()
{
	(void)fprintf(stderr, "Syntax error in file %s, line %d\n", configfile, (int)count);
	exit(EXIT_FAILURE);
}

void error_badval()
{
	(void)fprintf(stderr, "Invalid or unknown type of argument in file %s, line %d\n", configfile, (int)count);
	exit(EXIT_FAILURE);
}

void error_trap()
{
	if(debug)
		(void)fprintf(stderr, "%s\n", mp);
	else {
		syslog(LOG_ERR, mp);
		syslog(LOG_ERR, "exiting");
		closelog();
	}
	exit(EXIT_FAILURE);
}

void error_write()
{
	(void)snprintf(mp, 1024, "Unable to write: %s", strerror(errno));
	error_trap();
}

/*
 *	Check whether string represents integer.
 */

int is_numeric(char *p)
{
	while(*p != '\0') {
		if(! isdigit((int)*p))
			return(0);
		p++;
	}
	return(1);
}

/*
 *	Check specified time stamp format.
 */

int check_stamp(char *stamp)
{
	if(strcasecmp("none", stamp) == 0)
		return(STAMP_NONE);
	else if(strcasecmp("unix", stamp) == 0)
		return(STAMP_UNIX);
	else if(strcasecmp("time", stamp) == 0)
		return(STAMP_TIME);
	else if(strcasecmp("day", stamp) == 0)
		return(STAMP_DAY);
	else if(strcasecmp("month", stamp) == 0)
		return(STAMP_MONTH);
	else if(strcasecmp("year", stamp) == 0)
		return(STAMP_YEAR);
	else if(strcasecmp("full", stamp) == 0)
		return(STAMP_FULL);
	else
		return(-1);
}

/*
 *	Open and parse config file.
 */

int parse_config()
{
	FILE *f;
	char *p;
	char *cmd;
	char *arg;
	struct servent *service;

	if(configfile == NULL) {
		if(debug)
			return(0);
		else
			error_configfile();
	}
	if((f = fopen(configfile, "r")) == NULL)
		error_configfile();
	count = 0;
	while(fgets(mp, 1024, f) != NULL) {
		count++;
		p = mp;
		if((*p == '\n') || (*p == '#'))
			continue;
		while((*p == ' ') || (*p == '\t'))
			p++;
		cmd = p;
		while(isalnum((int)*p))
			p++;
		if(! isspace((int)*p))
			error_syntax();
		*p++ = '\0';
		if(*p == '\0')
			arg = NULL;
		else {
			while(((*p == ' ') || (*p == '\t')) && (*p != '\0'))
				p++;
			if(*p == '"') {
				p++;
				arg = p;
				while((*p != '"') && (*p != '\n') && (*p != '\0'))
					p++;
				if(*p != '"')
					error_syntax();
			}
			else {
				arg = p;
				while(! isspace((int)*p))
					p++;
			}
			*p = '\0';
		}

		if(strcasecmp("LocalAddress", cmd) == 0) {
			if((address = (char *)malloc(strlen(arg) + 1)) == NULL)
				error_memory();
			(void)strcpy(address, arg);
		}
		else if(strcasecmp("LocalPort", cmd) == 0) {
			if(is_numeric(arg))
				port = atoi(arg);
			else {
				if((service = getservbyname(arg, "udp")) == NULL)
					error_badval();
				port = ntohs((uint16_t)service->s_port);
			}
		}
		else if(strcasecmp("TimeStamp", cmd) == 0) {
			if((tmode = check_stamp(arg)) == -1)
				error_badval();
		}
		else if(strcasecmp("FileName", cmd) == 0) {
			if((filename = (char *)malloc(strlen(arg) + 30)) == NULL)
				error_memory();
			(void)strcpy(filename, arg);
			suffix = filename + strlen(filename);
		}
		else if(strcasecmp("FileMode", cmd) == 0) {
			if(! is_numeric(arg))
				error_badval();
			if(sscanf(arg, "%o", (unsigned int *)&filemode) != 1)
				error_badval();
		}
		else if(strcasecmp("FileFlag", cmd) == 0) {
			if(strcasecmp("append", arg) == 0)
				fileflag |= O_APPEND;
			else if(strcasecmp("truncate", arg) == 0)
				fileflag |= O_TRUNC;
			else if(strcasecmp("exclusive", arg) == 0)
				fileflag |= O_EXCL;
			else
				error_badval();
		}
		else if(strcasecmp("FileIndex", cmd) == 0) {
			if((fileindex = check_stamp(arg)) == -1)
				error_badval();
		}
		else if(strcasecmp("SizeLimit", cmd) == 0) {
			p = arg;
			while(*p != '\0')
				p++;
			p--;
			if((*p == 'k') || (*p == 'K')) {
				limit = 1024;
				*p = '\0';
			}
			else if((*p == 'm') || (*p == 'M')) {
				limit = 1048576;
				*p = '\0';
			}
			else
				limit = 1;
			if(! is_numeric(arg))
				error_badval();
			limit *= (unsigned long)atol(arg);
		}
		else if(strcasecmp("SourceAddress", cmd) == 0) {
			if(strcasecmp("true", arg) == 0)
				addsrcaddr = 1;
			else if(strcasecmp("false", arg) == 0)
				addsrcaddr = 0;
			else
				error_badval();
		}
		else if(strcasecmp("LineFeed", cmd) == 0) {
			if(strcasecmp("true", arg) == 0)
				addlf = 1;
			else if(strcasecmp("false", arg) == 0)
				addlf = 0;
			else
				error_badval();
		}
		else {
			(void)fprintf(stderr, "Unknown command in file %s, line %d: %s\n", configfile, (int)count, cmd);
			exit(EXIT_FAILURE);
		}
	}
	if(! feof(f)) {
		(void)perror("Reading");
		exit(EXIT_FAILURE);
	}
	(void)fclose(f);
	return(count);
}

void bye(int code)
{
	retcode = (char)code;
	(void)write(comm[1], &retcode, 1);
	exit(code);
}

/*
 *	Close existing file.
 */

void close_file()
{
	if(ffd > STDERR_FILENO) {
		syslog(LOG_INFO, "closing file %s (%ld bytes)", filename, count);
		close(ffd);
		ffd = 0;
	}
}

/*
 *	Open new output file.
 */

void open_file()
{
	off_t offset;
	int whence = SEEK_END;

	if(debug)
		return;
	close_file();
	if(limit) {
		now = time(NULL);
		switch(fileindex) {
		case STAMP_UNIX:
			(void)snprintf(suffix, STAMP_UNIX + 1, "%lu", (unsigned long int)now);
			break;
		case STAMP_TIME:
			t = localtime(&now);
			strftime(suffix, STAMP_TIME + 1, "%H-%M-%S", t);
			break;
		case STAMP_DAY:
			t = localtime(&now);
			strftime(suffix, STAMP_DAY + 1, "%d_%H-%M-%S", t);
			break;
		case STAMP_MONTH:
			t = localtime(&now);
			strftime(suffix, STAMP_MONTH + 1, "%m-%d_%H-%M-%S", t);
			break;
		case STAMP_YEAR:
			t = localtime(&now);
			strftime(suffix, STAMP_YEAR + 1, "%y-%m-%d_%H-%M-%S", t);
			break;
		case STAMP_FULL:
			t = localtime(&now);
			strftime(suffix, STAMP_FULL + 1, "%Y-%m-%d_%H-%M-%S", t);
			break;
		default:
			break;
		}
	}
	if((ffd = open(filename, fileflag, filemode)) == -1) {
		(void)snprintf(mp, 1024, "Unable to open \"%s\": %s", filename, strerror(errno));
		error_trap();
	}
	if(fileflag & O_TRUNC)
		whence = SEEK_SET;
	if((offset = (unsigned long)lseek(ffd, 0, whence)) == -1) {
		(void)snprintf(mp, 1024, "Unable to determine file size: %s", strerror(errno));
		error_trap();
	}
	count = (unsigned long)offset;
	syslog(LOG_INFO, "opening file %s", filename);
}

static void sig_hup(int signo)
{
	syslog(LOG_INFO, "caught SIGHUP");
	open_file();
}

static void sig_int(int signo)
{
	syslog(LOG_INFO, "caught SIGINT");
	close_file();
	syslog(LOG_INFO, "exiting");
	closelog();
	exit(EXIT_SUCCESS);
}

static void sig_term(int signo)
{
	syslog(LOG_INFO, "caught SIGTERM");
	close_file();
	syslog(LOG_INFO, "exiting");
	closelog();
	exit(EXIT_SUCCESS);
}

void usage()
{
	(void)fprintf(stderr, "usage: yasul [<options>]\nwhere options are: [defaults in brackets]\n-a <addr>  bind to specific address [any]\n-c <file>  use specified configuration file [%s]\n-d         debugging mode (do not fork, write to stdout only)\n-h         this text\n-p <port>  specify port\n-v         print version information\n", configfile);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	int opt;
	int n;
	int sfd;
	int clport = 0;
	char *cp;
	char *bod;
	char *claddress = NULL;
	pid_t pid;
#ifdef SOLARIS
	char *ntoa;
	extern char *inet_ntoa(struct in_addr);
#endif
	struct sockaddr_in server;
	struct sockaddr_in client;
	struct servent *service;
	socklen_t l;

	while((opt = getopt(argc, (char ** const)argv, "a:c:dhp:v")) != EOF) {
		switch(opt) {
		case 'a':
			claddress = optarg;
			break;
		case 'c':
			configfile = optarg;
			break;
		case 'd':
			debug = 1;
			tmode = 0;
			addsrcaddr = 0;
			break;
		case 'h':
			usage();
			break;
		case 'p':
			if(is_numeric(optarg))
				clport = atoi(optarg);
			else {
				if((service = getservbyname(optarg, "udp")) == NULL) {
					(void)fprintf(stderr, "Unknown service: %s\n", optarg);
					exit(EXIT_FAILURE);
				}
				clport = ntohs((uint16_t)service->s_port);
			}
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

	if((mp = (char *)malloc(DGSIZE + 256)) == NULL)
		error_memory();
	bod = mp + 192;

	if(! debug) {
		parse_config();
		if(filename == NULL) {
			(void)fprintf(stderr, "No output file specified\n");
			exit(EXIT_FAILURE);
		}
	}
	if(claddress != NULL)
		address = claddress;
	if(clport != 0)
		port = clport;
	if(port == 0) {
		(void)fprintf(stderr, "No port specified\n");
		exit(EXIT_FAILURE);

	}
	if((limit && (! fileindex)) || ((! limit) && fileindex)) {
		(void)fprintf(stderr, "Options SizeLimit and FileIndex depend on each other\n");
		exit(EXIT_FAILURE);
	}

	if(! debug) {
		if(pipe(comm) == -1) {
			(void)perror("Unable to create pipe");
			exit(EXIT_FAILURE);
		}
		if((pid = fork()) == -1) {
			(void)perror("Unable to fork");
			exit(EXIT_FAILURE);
		}
		else if(pid > 0) {	/* parent */
			close(comm[1]);
			if(read(comm[0], &retcode, 1) != 1)
				exit(EXIT_FAILURE);
			close(comm[0]);
			exit((int)retcode);
		}
		close(comm[0]);	/* child */
	}

	if(limit)
		suffix = filename + strlen(filename);

	/*
	 *	network
	 */
	if((sfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		(void)perror("Unable to create socket");
		bye(EXIT_FAILURE);
	}
	memset(&server, 0, sizeof(server));
	if(! inet_pton(AF_INET, address, &server.sin_addr)) {
		(void)fprintf(stderr, "Invalid address: %s\n", address);
		bye(EXIT_FAILURE);
	}
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
#ifdef SOLARIS
	if(bind(sfd, (const struct sockaddr *)&server, sizeof(server)) == -1) {
#else
	if(bind(sfd, (struct sockaddr *)&server, sizeof(server)) == -1) {
#endif
		(void)fprintf(stderr, "Unable to bind to %s:%d\n", address, port);
		bye(EXIT_FAILURE);
	}

	if(! debug)
		open_file();

	/*
	 *	Prepare messaging and interrupt handlers.
	 */
	if(debug)
		(void)fprintf(stderr, "yasul started in debug mode\nWaiting for input on %s:%d\n", address, port);
	else {
		if(signal(SIGHUP, sig_hup) == SIG_ERR)
			error_signal();
		if(signal(SIGINT, sig_int) == SIG_ERR)
			error_signal();
		if(signal(SIGTERM, sig_term) == SIG_ERR)
			error_signal();
		openlog("yasul", 0, LOG_DAEMON|LOG_PID);
		syslog(LOG_INFO, "address %s, port %d", address, port);
		/* say good-bye to parent */
		*mp = (char)EXIT_SUCCESS;
		(void)write(comm[1], mp, 1);
		(void)close(comm[1]);
	}

	/*
	 *	Deal with incoming data in an eternal loop.
	 */
	count = 0;
	while(1) {
#ifdef SOLARIS
		if((n = recvfrom(sfd, bod, DGSIZE, 0,
			(struct sockaddr *)&client, &l)) == -1) {
#else
		if((n = recvfrom(sfd, bod, DGSIZE, 0, (struct sockaddr *)&client, &l)) == -1) {
#endif
			(void)snprintf(mp, 1024, "Unable to read: %s", strerror(errno));
			error_trap();
		}
		if(addlf) {
			*(bod + n) = '\n';
			n++;
		}
		cp = mp;
		if(tmode) {
			if(! addsrcaddr)
				cp = bod - tmode - 1;
			now = time(NULL);
			switch(tmode) {
			case STAMP_UNIX:
				(void)snprintf(cp, STAMP_UNIX + 1, "%lu", (unsigned long int)now);
				break;
			case STAMP_TIME:
				t = localtime(&now);
				strftime(cp, STAMP_TIME + 1, "%T", t);
				break;
			case STAMP_DAY:
				t = localtime(&now);
				strftime(cp, STAMP_DAY + 1, "%d %T", t);
				break;
			case STAMP_MONTH:
				t = localtime(&now);
				strftime(cp, STAMP_MONTH + 1, "%m-%d %T", t);
				break;
			case STAMP_YEAR:
				t = localtime(&now);
				strftime(cp, STAMP_YEAR + 1, "%y-%m-%d %T", t);
				break;
			case STAMP_FULL:
				t = localtime(&now);
				strftime(cp, STAMP_FULL + 1, "%Y-%m-%d %T", t);
				break;
			default:
				break;
			}
			*(cp + tmode) = ' ';
			cp += (tmode + 1);
		}
		if(addsrcaddr) {
#ifdef SOLARIS
			if((ntoa = inet_ntoa(client.sin_addr)) == NULL)
				strcpy(cp, "[unknown]");
			else
				strcpy(cp, ntoa);
#else
			if(inet_ntop(AF_INET, &client.sin_addr, cp, 96) == NULL)
				strcpy(cp, "[unknown]");
#endif
			while(*cp != '\0')
				cp++;
			*cp++ = ' ';
			*cp = '\0';
			opt = (int)(cp - mp);
			if(write(ffd, mp, opt) != opt)
				error_write();
			count += (unsigned long)opt;
			if(write(ffd, bod, n) != n)
				error_write();
			count += (unsigned long)n;
		}
		else {
			if(tmode) {
				cp = bod - tmode - 1;
				opt = n + tmode + 1;
			}
			else {
				cp = bod;
				opt = n;
			}
			if(write(ffd, cp, opt) != opt)
				error_write();
			count += (unsigned long)opt;
		}
		if(limit && (count >= limit))
			open_file();
	}
}
