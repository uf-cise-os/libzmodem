/* lrz.c cosmetic modifications by Matt Porter
 * from rz.c By Chuck Forsberg
 * 
 *  A program for Linux to receive files and commands from computers running
 *  zmodem, ymodem, or xmodem protocols.
 *  lrz uses Unix buffered input to reduce wasted CPU time.
 *
 */

#include "config.h"

#if STDC_HEADERS
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#   include <strings.h>
# endif
# ifndef HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
char *strchr (), *strrchr ();
# ifndef HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif

#define SS_NORMAL 0
#define LOGFILE "/var/adm/rzlog"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#ifdef TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
#else
#  ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#  else
#    include <time.h>
#  endif
#endif
#include "timing.h"
extern int errno;
FILE *popen();

#define OK 0
#define FALSE 0
#define TRUE 1
#define ERROR (-1)

#define MAX_BLOCK 8192

/*
 * Max value for HOWMANY is 255 if NFGVMIN is not defined.
 *   A larger value reduces system overhead but may evoke kernel bugs.
 *   133 corresponds to an XMODEM/CRC sector
 */
#ifndef HOWMANY
#ifdef NFGVMIN
#define HOWMANY MAX_BLOCK
#else
#define HOWMANY 255
#endif
#endif

/* Ward Christensen / CP/M parameters - Don't change these! */
#define ENQ 005
#define CAN ('X'&037)
#define XOFF ('s'&037)
#define XON ('q'&037)
#define SOH 1
#define STX 2
#define EOT 4
#define ACK 6
#define NAK 025
#define CPMEOF 032
#define WANTCRC 0103	/* send C not NAK to get crc not checksum */
#define TIMEOUT (-2)
#define RCDO (-3)
#define ERRORMAX 5
#define RETRYMAX 5
#define WCEOT (-10)
#define PATHLEN 257	/* ready for 4.2 bsd ? */
#define UNIXFILE 0xF000	/* The S_IFMT file mask bit for stat */
int Zmodem=0;		/* ZMODEM protocol requested */
int Nozmodem = 0;	/* If invoked as "rb" */
unsigned Baudrate = 2400;

#include "rbsb.c"	/* most of the system dependent stuff here */

#include "crctab.c"

FILE *fout;

/*
 * Routine to calculate the free bytes on the current file system
 *  ~0 means many free bytes (unknown)
 */
long getfree()
{
	return(~0L);	/* many free bytes ... */
}

int Lastrx;
int Crcflg;
int Firstsec;
int Eofseen;		/* indicates cpm eof (^Z) has been received */
int errors;
int Restricted=1;	/* restricted; no /.. or ../ in filenames */
int Readnum = HOWMANY;	/* Number of bytes to ask for in read() from modem */

#define DEFBYTL 2000000000L	/* default rx file size */
long Bytesleft;		/* number of bytes of incoming file left */
long Modtime;		/* Unix style mod time for incoming file */
int Filemode;		/* Unix style mode for incoming file */
char Pathname[PATHLEN];
char *Progname;		/* the name by which we were called */

int Batch=0;
int Topipe=0;
int MakeLCPathname=TRUE;	/* make received pathname lower case */
int Verbose=0;
int Quiet=0;		/* overrides logic that would otherwise set verbose */
int Nflag = 0;		/* Don't really transfer files */
int Rxclob=FALSE;	/* Clobber existing file */
int Rxbinary=FALSE;	/* receive all files in bin mode */
int Rxascii=FALSE;	/* receive files in ascii (translate) mode */
int Thisbinary;		/* current file is to be received in bin mode */
int Blklen;		/* record length of received packets */
long rxbytes;
int try_resume=FALSE;
int allow_remote_commands=FALSE;
int no_timeout=FALSE;

#ifdef SEGMENTS
int chinseg = 0;	/* Number of characters received in this data seg */
char secbuf[1+(SEGMENTS+1)*MAX_BLOCK];
#else
char secbuf[MAX_BLOCK + 1];
#endif

#ifdef ENABLE_TIMESYNC
int timesync_flag=0;
int in_timesync=0;
#endif

char linbuf[HOWMANY];
int Lleft=0;		/* number of characters in linbuf */
time_t timep[2];
char Lzmanag;		/* Local file management request */
char zconv;		/* ZMODEM file conversion request */
char zmanag;		/* ZMODEM file management request */
char ztrans;		/* ZMODEM file transport request */
int Zctlesc;		/* Encode control characters */
int Zrwindow = 1400;	/* RX window size (controls garbage count) */

#define xsendline(c) sendline(c)

char *readline_ptr;	/* pointer for removing chars from linbuf */
#define READLINE_PF(timeout) \
	(--Lleft >= 0? (*readline_ptr++ & 0377) : readline(timeout))

#include "zm.c"

int tryzhdrtype=ZRINIT;	/* Header type to send corresponding to Last rx close */

RETSIGTYPE
alrm(int dummy)
{
	/* doesn't need to do anything */
}

/* called by signal interrupt or terminate to clean things up */
RETSIGTYPE
bibi(n)
{
	if (Zmodem)
		zmputs(Attn);
	canit(); mode(0);
	fprintf(stderr, "lrz: caught signal %d; exiting", n);
	cucheck();
	exit(128+n);
}

main(argc, argv)
char *argv[];
{
	register char *cp;
	register npats;
	char *virgin, **patts;
	char *getenv();
	int exitcode=0;
	int under_rsh=FALSE;

	Rxtimeout = 100;
	setbuf(stderr, NULL);
	if ((cp=getenv("SHELL")) && (strstr(cp, "rsh") || strstr(cp, "rksh")
		|| strstr(cp,"rbash")))
		under_rsh=TRUE;
	if ((cp=getenv("ZMODEM_RESTRICTED"))!=NULL)
		Restricted=2;

	from_cu();
	chkinvok(virgin=argv[0]);	/* if called as [-]rzCOMMAND set flag */
	npats = 0;
	while (--argc) {
		cp = *++argv;
		if (*cp == '-') {
			while( *++cp) {
				switch(*cp) {
				case '\\':
					 cp[1] = toupper(cp[1]);  continue;
				case '+':
					Lzmanag = ZMAPND; break;
				case 'a':
					Rxascii=TRUE;  break;
				case 'b':
					Rxbinary=TRUE; break;
				case 'c':
					Crcflg=TRUE; break;
				case 'C':
					allow_remote_commands=TRUE; break;
				case 'D':
					Nflag = TRUE; break;
				case 'e':
					Zctlesc = 1; break;
				case 'h':
					usage(); break;
				case 'O': 
					no_timeout=TRUE; break;
				case 'p':
					Lzmanag = ZMPROT;  break;
				case 'q':
					Quiet=TRUE; Verbose=0; break;
				case 'r':
					try_resume=TRUE;  break;
				case 'R':
					Restricted++;  break;
				case 'S':
#ifdef ENABLE_TIMESYNC
					timesync_flag++;
					if (timesync_flag==2) {
#ifdef HAVE_SETTIMEOFDAY
	fputs("don't have settimeofday, will not set time\n", stderr);
#endif
						if (getuid()!=0)
	fputs("not running as root (this is good!), can not set time\n", stderr);
					}
#endif
					break;
				case 't':
					if (--argc < 1) {
						usage();
					}
					Rxtimeout = atoi(*++argv);
					if (Rxtimeout<10 || Rxtimeout>1000)
						usage();
					break;
				case 'w':
					if (--argc < 1) {
						usage();
					}
					Zrwindow = atoi(*++argv);
					break;
				case 'u':
					MakeLCPathname=FALSE; break;
				case 'U':
					if (!under_rsh)
						Restricted=0;
					else {
	fputs("security violation: running under restricted shell\n", stderr);
						exit(1);
					}
					break;
				case 'v':
					++Verbose; break;
				case 'y':
					Rxclob=TRUE; break;
				default:
					usage();
				}
			}
		}
		else if ( !npats && argc>0) {
			if (argv[0][0]) {
				npats=argc;
				patts=argv;
			}
		}
	}
	if (npats > 1)
		usage();
	if (Batch && npats)
		usage();
	if (Restricted && allow_remote_commands)
		allow_remote_commands=FALSE;
	if (Verbose) {
#if 0
		if (freopen(LOGFILE, "a", stderr)==NULL) {
			printf("Can't open log file %s\n",LOGFILE);
			exit(1);
		}
		fprintf(stderr, "argv[0]=%s Progname=%s\n", virgin, Progname);
#endif
		setbuf(stderr, NULL);
	}
	if (Fromcu && !Quiet) {
		if (Verbose == 0)
			Verbose = 2;
	}
	vfile("%s %s for %s\n", Progname, VERSION, OS);
	mode(1);
#ifndef linux
	if (signal(SIGINT, bibi) == SIG_IGN) {
		signal(SIGINT, SIG_IGN); signal(SIGKILL, SIG_IGN);
	}
	else {
		signal(SIGINT, bibi); signal(SIGKILL, bibi);
	}
	signal(SIGTERM, bibi);
#endif
	if (wcreceive(npats, patts)==ERROR) {
		exitcode=0200;
		canit();
	}
	mode(0);
	if (exitcode && !Zmodem)	/* bellow again with all thy might. */
		canit();
	if (exitcode)
		cucheck();
	exit(exitcode ? exitcode:SS_NORMAL);
}


usage()
{
	cucheck();
	fprintf(stderr,"Usage: lrz [-abeuvy] [-L FILE] (ZMODEM)\n");
	fprintf(stderr,"or     lrb [-abuvy] [-L FILE]      (YMODEM)\n");
	fprintf(stderr,"or     lrx [-abcv] [-L FILE] file  (XMODEM or XMODEM-1k)\n");
	fprintf(stderr,"	  -a ASCII transfer (strip CR)\n");
	fprintf(stderr,"	  -b Binary transfer for all files\n");
	fprintf(stderr,"	  -c Use 16 bit CRC	(XMODEM)\n");
	fprintf(stderr,"	  -e Escape control characters	(ZMODEM)\n");
	fprintf(stderr,"          -h Help, print this usage message\n");
	fprintf(stderr,"	  -R restricted, more secure mode\n");
	fprintf(stderr,"	  -v Verbose more v's give more info\n");
	fprintf(stderr,"	  -y Yes, clobber existing file if any\n");
	fprintf(stderr,"\t%s version %s for %s %s\n",
	  Progname, VERSION, CPU, OS);
	exit(SS_NORMAL);
}
/*
 *  Debugging information output interface routine
 */
/* VARARGS1 */
vfile(f, a, b, c)
register char *f;
{
	if (Verbose > 2) {
		fprintf(stderr, f, a, b, c);
		fprintf(stderr, "\n");
	}
}

/*
 * Let's receive something already.
 */

char *rbmsg =
"%s waiting to receive.";

wcreceive(argc, argp)
char **argp;
{
	register c;

	if (Batch || argc==0) {
		Crcflg=1;
		if ( !Quiet)
			fprintf(stderr, rbmsg, Progname, Nozmodem?"sb":"sz");
		if (c=tryz()) {
			if (c == ZCOMPL)
				return OK;
			if (c == ERROR)
				goto fubar;
			c = rzfiles();
			if (c)
				goto fubar;
		} else {
			for (;;) {
				if (wcrxpn(secbuf)== ERROR)
					goto fubar;
				if (secbuf[0]==0)
					return OK;
				if (procheader(secbuf) == ERROR)
					goto fubar;
				if (wcrx()==ERROR)
					goto fubar;
			}
		}
	} else {
		Bytesleft = DEFBYTL; Filemode = 0; Modtime = 0L;

		procheader(""); strcpy(Pathname, *argp); checkpath(Pathname);
		fprintf(stderr, "\n%s: ready to receive %s\r\n", Progname, Pathname);
		if ((fout=fopen(Pathname, "w")) == NULL)
			return ERROR;
		if (wcrx()==ERROR)
			goto fubar;
                timing(1);
	}
	return OK;
fubar:
	canit();
	if (Topipe && fout) {
		pclose(fout);  return ERROR;
	}
	if (fout)
		fclose(fout);
	if (Restricted) {
		unlink(Pathname);
		fprintf(stderr, "\r\n%s: %s removed.\r\n", Progname, Pathname);
	}
	return ERROR;
}


/*
 * Fetch a pathname from the other end as a C ctyle ASCIZ string.
 * Length is indeterminate as long as less than Blklen
 * A null string represents no more files (YMODEM)
 */
wcrxpn(rpn)
char *rpn;	/* receive a pathname */
{
	register c;

#ifdef NFGVMIN
	readline(1);
#else
	purgeline();
#endif

et_tu:
	Firstsec=TRUE;  Eofseen=FALSE;
	sendline(Crcflg?WANTCRC:NAK);
	Lleft=0;	/* Do read next time ... */
	while ((c = wcgetsec(rpn, 100)) != 0) {
		if (c == WCEOT) {
			zperr( "Pathname fetch returned %d", c);
			sendline(ACK);
			Lleft=0;	/* Do read next time ... */
			readline(1);
			goto et_tu;
		}
		return ERROR;
	}
	sendline(ACK);
	return OK;
}

/*
 * Adapted from CMODEM13.C, written by
 * Jack M. Wierda and Roderick W. Hart
 */

wcrx()
{
	register int sectnum, sectcurr;
	register char sendchar;
	register char *p;
	int cblklen;			/* bytes to dump this block */

	Firstsec=TRUE;sectnum=0; Eofseen=FALSE;
	sendchar=Crcflg?WANTCRC:NAK;

	for (;;) {
		sendline(sendchar);	/* send it now, we're ready! */
		Lleft=0;	/* Do read next time ... */
		sectcurr=wcgetsec(secbuf, (sectnum&0177)?50:130);
		report(sectcurr);
		if (sectcurr==(sectnum+1 &0377)) {
			sectnum++;
			cblklen = Bytesleft>Blklen ? Blklen:Bytesleft;
			if (putsec(secbuf, cblklen)==ERROR)
				return ERROR;
			if ((Bytesleft-=cblklen) < 0)
				Bytesleft = 0;
			sendchar=ACK;
		}
		else if (sectcurr==(sectnum&0377)) {
			zperr( "Received dup Sector");
			sendchar=ACK;
		}
		else if (sectcurr==WCEOT) {
			if (closeit())
				return ERROR;
			sendline(ACK);
			Lleft=0;	/* Do read next time ... */
			return OK;
		}
		else if (sectcurr==ERROR)
			return ERROR;
		else {
			zperr( "Sync Error");
			return ERROR;
		}
	}
}

/*
 * Wcgetsec fetches a Ward Christensen type sector.
 * Returns sector number encountered or ERROR if valid sector not received,
 * or CAN CAN received
 * or WCEOT if eot sector
 * time is timeout for first char, set to 4 seconds thereafter
 ***************** NO ACK IS SENT IF SECTOR IS RECEIVED OK **************
 *    (Caller must do that when he is good and ready to get next sector)
 */

wcgetsec(rxbuf, maxtime)
char *rxbuf;
int maxtime;
{
	register checksum, wcj, firstch;
	register unsigned short oldcrc;
	register char *p;
	int sectcurr;

	for (Lastrx=errors=0; errors<RETRYMAX; errors++) {

		if ((firstch=readline(maxtime))==STX) {
			Blklen=1024; goto get2;
		}
		if (firstch==SOH) {
			Blklen=128;
get2:
			sectcurr=readline(1);
			if ((sectcurr+(oldcrc=readline(1)))==0377) {
				oldcrc=checksum=0;
				for (p=rxbuf,wcj=Blklen; --wcj>=0; ) {
					if ((firstch=readline(1)) < 0)
						goto bilge;
					oldcrc=updcrc(firstch, oldcrc);
					checksum += (*p++ = firstch);
				}
				if ((firstch=readline(1)) < 0)
					goto bilge;
				if (Crcflg) {
					oldcrc=updcrc(firstch, oldcrc);
					if ((firstch=readline(1)) < 0)
						goto bilge;
					oldcrc=updcrc(firstch, oldcrc);
					if (oldcrc & 0xFFFF)
						zperr( "CRC");
					else {
						Firstsec=FALSE;
						return sectcurr;
					}
				}
				else if (((checksum-firstch)&0377)==0) {
					Firstsec=FALSE;
					return sectcurr;
				}
				else
					zperr( "Checksum");
			}
			else
				zperr("Sector number garbled");
		}
		/* make sure eot really is eot and not just mixmash */
#ifdef NFGVMIN
		else if (firstch==EOT && readline(1)==TIMEOUT)
			return WCEOT;
#else
		else if (firstch==EOT && Lleft==0)
			return WCEOT;
#endif
		else if (firstch==CAN) {
			if (Lastrx==CAN) {
				zperr( "Sender CANcelled");
				return ERROR;
			} else {
				Lastrx=CAN;
				continue;
			}
		}
		else if (firstch==TIMEOUT) {
			if (Firstsec)
				goto humbug;
bilge:
			zperr( "TIMEOUT");
		}
		else
			zperr( "Got 0%o sector header", firstch);

humbug:
		Lastrx=0;
		while(readline(1)!=TIMEOUT)
			;
		if (Firstsec) {
			sendline(Crcflg?WANTCRC:NAK);
			Lleft=0;	/* Do read next time ... */
		} else {
			maxtime=40; sendline(NAK);
			Lleft=0;	/* Do read next time ... */
		}
	}
	/* try to stop the bubble machine. */
	canit();
	return ERROR;
}

/*
 * This version of readline is reasoably well suited for
 * reading many characters.
 *  (except, currently, for the Regulus version!)
 *
 * timeout is in tenths of seconds
 */
readline(timeout)
int timeout;
{
	register n;
#ifndef READLINE_PF
	static char *readline_ptr;	/* pointer for removing chars from linbuf */
#endif

	if (--Lleft >= 0) {
		if (Verbose > 8) {
			fprintf(stderr, "%02x ", *readline_ptr&0377);
		}
		return (*readline_ptr++ & 0377);
	}

	if (!no_timeout)
	{
		n = timeout/10;
		if (n < 2)
			n = 3;
		if (Verbose > 5)
			fprintf(stderr, "Calling read: alarm=%d  Readnum=%d ",
			  n, Readnum);
		signal(SIGALRM, alrm); alarm(n);
	}
	else if (Verbose > 5)
		fprintf(stderr, "Calling read: Readnum=%d ",
		  Readnum);

	Lleft=read(iofd, readline_ptr=linbuf, Readnum);
	if (!no_timeout)
		alarm(0);
	if (Verbose > 5) {
		fprintf(stderr, "Read returned %d bytes\n", Lleft);
	}
	if (Lleft < 1)
		return TIMEOUT;
	--Lleft;
	if (Verbose > 8) {
		fprintf(stderr, "%02x ", *readline_ptr&0377);
	}
	return (*readline_ptr++ & 0377);
}



/*
 * Purge the modem input queue of all characters
 */
purgeline()
{
	Lleft = 0;
#ifdef TCFLSH
	ioctl(iofd, TCFLSH, 0);
#else
	lseek(iofd, 0L, 2);
#endif
}

/*
 * Process incoming file information header
 */
procheader(name)
char *name;
{
	register char *openmode, *p, **pp;
	int tabs, tab_num;

	/* set default parameters and overrides */
	openmode = "w";
	Thisbinary = (!Rxascii) || Rxbinary;
	if (Lzmanag)
		zmanag = Lzmanag;

	/*
	 *  Process ZMODEM remote file management requests
	 */
	if (!Rxbinary && zconv == ZCNL)	/* Remote ASCII override */
		Thisbinary = 0;
	if (zconv == ZCBIN)	/* Remote Binary override */
		Thisbinary = TRUE;
	else if (zmanag == ZMAPND)
		openmode = "a";
	if (Thisbinary && zconv == ZCBIN && try_resume)
		zconv=ZCRESUM;

#ifdef ENABLE_TIMESYNC
	in_timesync=0;
	if (timesync_flag && 0==strcmp(name,"$time$.t"))
		in_timesync=1;
#endif
	/* Check for existing file */
	if (zconv != ZCRESUM && !Rxclob && (zmanag&ZMMASK) != ZMCLOB && (fout=fopen(name, "r"))) {
#ifdef ENABLE_TIMESYNC
	    if (!in_timesync)
#endif
		fclose(fout);  return ERROR;
	}

	Bytesleft = DEFBYTL; Filemode = 0; Modtime = 0L;

	p = name + 1 + strlen(name);
	if (*p) {	/* file coming from Unix or DOS system */
		sscanf(p, "%ld%lo%o", &Bytesleft, &Modtime, &Filemode);
		if (Filemode & UNIXFILE)
			++Thisbinary;
	} else {		/* File coming from CP/M system */
		for (p=name; *p; ++p)		/* change / to _ */
			if ( *p == '/')
				*p = '_';

		if ( *--p == '.')		/* zap trailing period */
			*p = 0;
	}

#ifdef ENABLE_TIMESYNC
	if (in_timesync)
	{
		long t=time(0);
		long d=t-Modtime;
		if (d<0)
			d=0;
		if ((Verbose && d>60) || Verbose > 1)
			fprintf(stderr,  
	"TIMESYNC: here %ld, remote %ld, diff %d seconds\n",
			(long) t, (long) Modtime, (long) d);
#ifdef HAVE_SETTIMEOFDAY
		if (timesync_flag > 1 && d > 10)
		{
			struct timeval tv;
			tv.tv_sec=Modtime;
			tv.tv_usec=0;
			if (settimeofday(&tv,NULL))
				fprintf(stderr, "TIMESYNC: cannot set time: %s\n",
				strerror(errno));
		}
#endif
		return ERROR; /* skips file */
	}
#endif /* ENABLE_TIMESYNC */

	if (!Zmodem && MakeLCPathname && !IsAnyLower(name)
	  && !(Filemode&UNIXFILE))
		uncaps(name);
	if (Topipe > 0) {
		sprintf(Pathname, "%s %s", Progname+2, name);
		if (Verbose)
			fprintf(stderr,  "Topipe: %s %s\n",
			  Pathname, Thisbinary?"BIN":"ASCII");
		if ((fout=popen(Pathname, "w")) == NULL)
			return ERROR;
	} else {
		strcpy(Pathname, name);
		if (Verbose)
			fprintf(stderr, "\nReceiving: %s\n", name);
		timing(1);
		checkpath(name);
		if (Nflag)
			name = "/dev/null";
#ifdef OMEN
		if (name[0] == '!' || name[0] == '|') {
			if ( !(fout = popen(name+1, "w"))) {
				return ERROR;
			}
			Topipe = -1;  return(OK);
		}
#endif
		if (Thisbinary && zconv==ZCRESUM) {
			struct stat st;
			fout = fopen(name, "r+");
			if (fout && 0==fstat(fileno(fout),&st))
			{
				/* retransfer whole blocks */
				rxbytes = st.st_size & ~(1024);
				/* Bytesleft == filelength on remote */
				if (rxbytes < Bytesleft) {
					if (fseek(fout, rxbytes, 0)) {
						fclose(fout);
						return ZFERR;
					}
				}
				goto buffer_it;
			}
			rxbytes=0;
			if (fout)
				fclose(fout);
		}
#ifdef ENABLE_MKDIR
		fout = fopen(name, openmode);
		if ( !fout && Restricted < 2)
			if (make_dirs(name))
				fout = fopen(name, openmode);
#else
		fout = fopen(name, openmode);
#endif
		if ( !fout)
		{
			int e=errno;
			fprintf(stderr, "lrz: cannot open %s: %s\n", name,
				strerror(e));
			return ERROR;
		}
	}
buffer_it:
	if (Topipe == 0) {
		static char *s=NULL;
		if (!s) {
			s=malloc(16384);
			if (!s) {
				fprintf(stderr,"lrz: out of memory\r\n");
				exit(1);
			}
#ifdef SETVBUF_REVERSED
			setvbuf(fout,_IOFBF,s,16384);
#else
			setvbuf(fout,s,_IOFBF,16384);
#endif
		}
	}

	return OK;
}

#ifdef ENABLE_MKDIR
/*
 *  Directory-creating routines from Public Domain TAR by John Gilmore
 */

/*
 * After a file/link/symlink/dir creation has failed, see if
 * it's because some required directory was not present, and if
 * so, create all required dirs.
 */
make_dirs(pathname)
register char *pathname;
{
	register char *p;		/* Points into path */
	int madeone = 0;		/* Did we do anything yet? */
	int save_errno = errno;		/* Remember caller's errno */
	char *strchr();

	if (errno != ENOENT)
		return 0;		/* Not our problem */

	for (p = strchr(pathname, '/'); p != NULL; p = strchr(p+1, '/')) {
		/* Avoid mkdir of empty string, if leading or double '/' */
		if (p == pathname || p[-1] == '/')
			continue;
		/* Avoid mkdir where last part of path is '.' */
		if (p[-1] == '.' && (p == pathname+1 || p[-2] == '/'))
			continue;
		*p = 0;				/* Truncate the path there */
		if ( !mkdir(pathname, 0777)) {	/* Try to create it as a dir */
			vfile("Made directory %s\n", pathname);
			madeone++;		/* Remember if we made one */
			*p = '/';
			continue;
		}
		*p = '/';
		if (errno == EEXIST)		/* Directory already exists */
			continue;
		/*
		 * Some other error in the mkdir.  We return to the caller.
		 */
		break;
	}
	errno = save_errno;		/* Restore caller's errno */
	return madeone;			/* Tell them to retry if we made one */
}

#endif /* ENABLE_MKDIR */

/*
 * Putsec writes the n characters of buf to receive file fout.
 *  If not in binary mode, carriage returns, and all characters
 *  starting with CPMEOF are discarded.
 */
putsec(buf, n)
     char *buf;
     register n;
{
	register char *p;

	if (n == 0)
		return OK;
	if (Thisbinary) {
		if (fwrite(buf,n,1,fout)!=1)
			return ERROR;
	}
	else {
		if (Eofseen)
			return OK;
		for (p=buf; --n>=0; ++p ) {
			if ( *p == '\r')
				continue;
			if (*p == CPMEOF) {
				Eofseen=TRUE; return OK;
			}
			putc(*p ,fout);
		}
	}
	return OK;
}

/*
 *  Send a character to modem.  Small is beautiful.
 */
sendline(c)
{
	char d;

	d = c;
	if (Verbose>6)
		fprintf(stderr, "Sendline: %x\n", c);
	write(1, &d, 1);
}

flushmo() {}

/* make string s lower case */
uncaps(s)
register char *s;
{
	for ( ; *s; ++s)
		if (isupper(*s))
			*s = tolower(*s);
}
/*
 * IsAnyLower returns TRUE if string s has lower case letters.
 */
IsAnyLower(s)
register char *s;
{
	for ( ; *s; ++s)
		if (islower(*s))
			return TRUE;
	return FALSE;
}

/*
 * Log an error
 */
/*VARARGS1*/
zperr(s,p,u)
char *s, *p, *u;
{
	if (Verbose <= 0)
		return;
	fprintf(stderr, "Retry %d: ", errors);
	fprintf(stderr, s, p, u);
	fprintf(stderr, "\n");
}

/* send cancel string to get the other end to shut up */
canit()
{
	static char canistr[] = {
	 24,24,24,24,24,24,24,24,24,24,8,8,8,8,8,8,8,8,8,8,0
	};

	printf(canistr);
	Lleft=0;	/* Do read next time ... */
	fflush(stdout);
}


report(sct)
int sct;
{
	if (Verbose>1)
		fprintf(stderr,"Blocks received: %d\r",sct);
}

/*
 * If called as [-][dir/../]vrzCOMMAND set Verbose to 1
 * If called as [-][dir/../]rzCOMMAND set the pipe flag
 * If called as rb use YMODEM protocol
 */
chkinvok(s)
char *s;
{
	register char *p;

	p = s;
	while (*p == '-')
		s = ++p;
	while (*p)
		if (*p++ == '/')
			s = p;
	if (*s == 'v') {
		Verbose=1; ++s;
	}
	Progname = s;
	if (*s == 'l') {
		/* lrz -> rz */
		++s;
	}
	if (s[0]=='r' && s[1]=='z')
		Batch = TRUE;
	if (s[0]=='r' && s[1]=='b')
		Batch = Nozmodem = TRUE;
	if (s[2] && s[0]=='r' && s[1]=='b')
		Topipe = 1;
	if (s[2] && s[0]=='r' && s[1]=='z')
		Topipe = 1;
}

/*
 * Totalitarian Communist pathname processing
 */
checkpath(name)
char *name;
{
	if (Restricted) {
		if (fopen(name, "r") != NULL) {
			canit();
			fprintf(stderr, "\r\nlrz: %s exists\n", name);
			bibi(-1);
		}
		/* restrict pathnames to current tree or uucppublic */
		if ( strstr(name, "../")
#ifdef PUBDIR
		 || (name[0]== '/' && strncmp(name, PUBDIR, 
		 	strlen(PUBDIR)))
#endif
		) {
			canit();
			fprintf(stderr,"\r\nlrz:\tSecurity Violation\r\n");
			bibi(-1);
		}
		if (Restricted > 1) {
			if (name[0]=='.' || strstr(name,"/.")) {
				canit();
				fprintf(stderr,"\r\nlrz:\tSecurity Violation\r\n");
				bibi(-1);
			}
		}
	}
}

/*
 * Initialize for Zmodem receive attempt, try to activate Zmodem sender
 *  Handles ZSINIT frame
 *  Return ZFILE if Zmodem filename received, -1 on error,
 *   ZCOMPL if transaction finished,  else 0
 */
tryz()
{
	register c, n;
	register cmdzack1flg;

	if (Nozmodem)		/* Check for "rb" program name */
		return 0;


	for (n=Zmodem?15:5; --n>=0; ) {
		/* Set buffer length (0) and capability flags */
#ifdef SEGMENTS
		stohdr(SEGMENTS*MAX_BLOCK);
#else
		stohdr(0L);
#endif
#ifdef CANBREAK
		Txhdr[ZF0] = CANFC32|CANFDX|CANOVIO|CANBRK;
#else
		Txhdr[ZF0] = CANFC32|CANFDX|CANOVIO;
#endif
#ifdef ENABLE_TIMESYNC
		if (timesync_flag)
			Txhdr[ZF1] |= ZF1_TIMESYNC;
#endif
		if (Zctlesc)
			Txhdr[ZF0] |= TESCCTL; /* TESCCTL == ESCCTL */
		zshhdr(tryzhdrtype, Txhdr);
		if (tryzhdrtype == ZSKIP)	/* Don't skip too far */
			tryzhdrtype = ZRINIT;	/* CAF 8-21-87 */
again:
		switch (zgethdr(Rxhdr, 0)) {
		case ZRQINIT:
			continue;
		case ZEOF:
			continue;
		case TIMEOUT:
			continue;
		case ZFILE:
			zconv = Rxhdr[ZF0];
			zmanag = Rxhdr[ZF1];
			ztrans = Rxhdr[ZF2];
			tryzhdrtype = ZRINIT;
			c = zrdata(secbuf, MAX_BLOCK);
			mode(3);
			if (c == GOTCRCW)
				return ZFILE;
			zshhdr(ZNAK, Txhdr);
			goto again;
		case ZSINIT:
			Zctlesc = TESCCTL & Rxhdr[ZF0];
			if (zrdata(Attn, ZATTNLEN) == GOTCRCW) {
				stohdr(1L);
				zshhdr(ZACK, Txhdr);
				goto again;
			}
			zshhdr(ZNAK, Txhdr);
			goto again;
		case ZFREECNT:
			stohdr(getfree());
			zshhdr(ZACK, Txhdr);
			goto again;
		case ZCOMMAND:
			cmdzack1flg = Rxhdr[ZF0];
			if (zrdata(secbuf, MAX_BLOCK) == GOTCRCW) {
				if (Verbose)
				{
					fprintf(stderr,"lrz: remote requested command\n");
					fprintf(stderr,"lrz: %s\n",secbuf);
				}
				if (!allow_remote_commands) 
				{
					if (Verbose)
						fprintf(stderr,"lrz: not executed\n");
					zshhdr(ZCOMPL, Txhdr);
					return ZCOMPL;
				}
				if (cmdzack1flg & ZCACK1)
					stohdr(0L);
				else
					stohdr((long)sys2(secbuf));
				purgeline();	/* dump impatient questions */
				do {
					zshhdr(ZCOMPL, Txhdr);
				}
				while (++errors<20 && zgethdr(Rxhdr,1) != ZFIN);
				ackbibi();
				if (cmdzack1flg & ZCACK1)
					exec2(secbuf);
				return ZCOMPL;
			}
			zshhdr(ZNAK, Txhdr); goto again;
		case ZCOMPL:
			goto again;
		default:
			continue;
		case ZFIN:
			ackbibi(); return ZCOMPL;
		case ZCAN:
			return ERROR;
		}
	}
	return 0;
}

/*
 * Receive 1 or more files with ZMODEM protocol
 */
rzfiles()
{
	register c;

	for (;;) {
		switch (c = rzfile()) {
		case ZEOF:
		case ZSKIP:
			switch (tryz()) {
			case ZCOMPL:
				return OK;
			default:
				return ERROR;
			case ZFILE:
				break;
			}
			continue;
		default:
			return c;
		case ERROR:
			return ERROR;
		}
	}
}

/*
 * Receive a file with ZMODEM protocol
 *  Assumes file name frame is in secbuf
 */
rzfile()
{
	register c, n;
	long last_rxbytes=0;
	long last_bps=0;
	long not_printed=0;

	Eofseen=FALSE;

	n = 20; rxbytes = 0l;

	if (procheader(secbuf) == ERROR) {
		return (tryzhdrtype = ZSKIP);
	}


	for (;;) {
#ifdef SEGMENTS
		chinseg = 0;
#endif
		stohdr(rxbytes);
		zshhdr(ZRPOS, Txhdr);
nxthdr:
		switch (c = zgethdr(Rxhdr, 0)) {
		default:
			vfile("lrzfile: zgethdr returned %d", c);
			return ERROR;
		case ZNAK:
		case TIMEOUT:
#ifdef SEGMENTS
			putsec(secbuf, chinseg);
			chinseg = 0;
#endif
			if ( --n < 0) {
				vfile("lrzfile: zgethdr returned %d", c);
				return ERROR;
			}
		case ZFILE:
			zrdata(secbuf, MAX_BLOCK);
			continue;
		case ZEOF:
#ifdef SEGMENTS
			putsec(secbuf, chinseg);
			chinseg = 0;
#endif
			if (rclhdr(Rxhdr) != rxbytes) {
				/*
				 * Ignore eof if it's at wrong place - force
				 *  a timeout because the eof might have gone
				 *  out before we sent our zrpos.
				 */
				errors = 0;  goto nxthdr;
			}
			if (Verbose>1) {
				int minleft =  0;
				int secleft =  0;
				last_bps=(rxbytes/timing(0));
				if (last_bps > 0) {
					minleft =  (Bytesleft-rxbytes)/last_bps/60;
					secleft =  ((Bytesleft-rxbytes)/last_bps)%60;
				}
				fprintf(stderr, "\rBytes Received: %7ld/%7ld   BPS:%-6ld                   \r\n",
					rxbytes, Bytesleft, last_bps, minleft, secleft);
			}
			if (closeit()) {
				tryzhdrtype = ZFERR;
				vfile("lrzfile: closeit returned <> 0");
				return ERROR;
			}
			vfile("lrzfile: normal EOF");
			return c;
		case ERROR:	/* Too much garbage in header search error */
#ifdef SEGMENTS
			putsec(secbuf, chinseg);
			chinseg = 0;
#endif
			if ( --n < 0) {
				vfile("lrzfile: zgethdr returned %d", c);
				return ERROR;
			}
			zmputs(Attn);
			continue;
		case ZSKIP:
#ifdef SEGMENTS
			putsec(secbuf, chinseg);
			chinseg = 0;
#endif
			closeit();
			vfile("lrzfile: Sender SKIPPED file");
			return c;
		case ZDATA:
			if (rclhdr(Rxhdr) != rxbytes) {
				if ( --n < 0) {
					return ERROR;
				}
#ifdef SEGMENTS
				putsec(secbuf, chinseg);
				chinseg = 0;
#endif
				zmputs(Attn);  continue;
			}
moredata:
			if (Verbose>1 
				&& (not_printed > 7 || rxbytes > last_bps / 2 + last_rxbytes)) {
				int minleft =  0;
				int secleft =  0;
				last_bps=(rxbytes/timing(0));
				if (last_bps > 0) {
					minleft =  (Bytesleft-rxbytes)/last_bps/60;
					secleft =  ((Bytesleft-rxbytes)/last_bps)%60;
				}
				fprintf(stderr, "\rBytes Received: %7ld/%7ld   BPS:%-6ld ETA %02d:%02d  ",
				 rxbytes, Bytesleft, last_bps, minleft, secleft);
				last_rxbytes=rxbytes;
				not_printed=0;
			} else if (Verbose)
				not_printed++;
#ifdef SEGMENTS
			if (chinseg >= (MAX_BLOCK * SEGMENTS)) {
				putsec(secbuf, chinseg);
				chinseg = 0;
			}
			switch (c = zrdata(secbuf+chinseg, MAX_BLOCK))
#else
			switch (c = zrdata(secbuf, MAX_BLOCK))
#endif
			{
			case ZCAN:
#ifdef SEGMENTS
				putsec(secbuf, chinseg);
				chinseg = 0;
#endif
				vfile("lrzfile: zgethdr returned %d", c);
				return ERROR;
			case ERROR:	/* CRC error */
#ifdef SEGMENTS
				putsec(secbuf, chinseg);
				chinseg = 0;
#endif
				if ( --n < 0) {
					vfile("lrzfile: zgethdr returned %d", c);
					return ERROR;
				}
				zmputs(Attn);
				continue;
			case TIMEOUT:
#ifdef SEGMENTS
				putsec(secbuf, chinseg);
				chinseg = 0;
#endif
				if ( --n < 0) {
					vfile("lrzfile: zgethdr returned %d", c);
					return ERROR;
				}
				continue;
			case GOTCRCW:
				n = 20;
#ifdef SEGMENTS
				chinseg += Rxcount;
				putsec(secbuf, chinseg);
				chinseg = 0;
#else
				putsec(secbuf, Rxcount);
#endif
				rxbytes += Rxcount;
				stohdr(rxbytes);
				zshhdr(ZACK, Txhdr);
				sendline(XON);
				goto nxthdr;
			case GOTCRCQ:
				n = 20;
#ifdef SEGMENTS
				chinseg += Rxcount;
#else
				putsec(secbuf, Rxcount);
#endif
				rxbytes += Rxcount;
				stohdr(rxbytes);
				zshhdr(ZACK, Txhdr);
				goto moredata;
			case GOTCRCG:
				n = 20;
#ifdef SEGMENTS
				chinseg += Rxcount;
#else
				putsec(secbuf, Rxcount);
#endif
				rxbytes += Rxcount;
				goto moredata;
			case GOTCRCE:
				n = 20;
#ifdef SEGMENTS
				chinseg += Rxcount;
#else
				putsec(secbuf, Rxcount);
#endif
				rxbytes += Rxcount;
				goto nxthdr;
			}
		}
	}
}

/*
 * Send a string to the modem, processing for \336 (sleep 1 sec)
 *   and \335 (break signal)
 */
zmputs(s)
char *s;
{
	char *p;

	while (s && *s)
	{
		p=strpbrk(s,"\335\336");
		if (!p)
		{
			write(1,s,strlen(s));
			return;
		}
		if (p!=s)
		{
			write(1,s,p-s);
			s=p;
		}
		if (*p=='\336')
			sleep(1);
		else
			sendbrk();
		p++;
	}
}

/*
 * Close the receive dataset, return OK or ERROR
 */
closeit()
{
	time_t time();

	if (Topipe) {
		if (pclose(fout)) {
			return ERROR;
		}
		return OK;
	}
	if (fclose(fout)) {
		fprintf(stderr, "file close error: %s\n",strerror(errno));
		/* this may be any sort of error, including random data corruption */
		unlink(Pathname);
		return ERROR;
	}
	if (Modtime) {
		timep[0] = time(NULL);
		timep[1] = Modtime;
		utime(Pathname, timep);
	}
	if ((Filemode&S_IFMT) == S_IFREG)
		chmod(Pathname, (07777 & Filemode));
	return OK;
}

/*
 * Ack a ZFIN packet, let byegones be byegones
 */
ackbibi()
{
	register n;

	vfile("ackbibi:");
	Readnum = 1;
	stohdr(0L);
	for (n=3; --n>=0; ) {
		purgeline();
		zshhdr(ZFIN, Txhdr);
		switch (readline(100)) {
		case 'O':
			readline(1);	/* Discard 2nd 'O' */
			vfile("ackbibi complete");
			return;
		case RCDO:
			return;
		case TIMEOUT:
		default:
			break;
		}
	}
}



/*
 * Local console output simulation
 */
bttyout(c)
{
	if (Verbose || Fromcu)
		putc(c, stderr);
}

/*
 * Strip leading ! if present, do shell escape. 
 */
sys2(s)
register char *s;
{
	if (*s == '!')
		++s;
	return system(s);
}
/*
 * Strip leading ! if present, do exec.
 */
exec2(s)
register char *s;
{
	if (*s == '!')
		++s;
	mode(0);
	execl("/bin/sh", "sh", "-c", s);
}
/* End of lrz.c */
















