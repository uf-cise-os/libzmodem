/*  lsz.c cosmetic modifications by Matt Porter *  from the Public Domain version of sz.c by Chuck Forsberg,
 *  Omen Technology INC
 *
 *  A program for Linux to send files and commands to computers running
 *  zmodem, ymodem, or xmodem protocols.
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

#define NEW_ERROR

char *getenv();

#define SS_NORMAL 0
#define LOGFILE "/var/log/szlog"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
#include <ctype.h>
#include <errno.h>
#ifdef HAVE_LIMITS_H
#  include <limits.h>
#else 
#  define PATH_MAX 1024
#endif
#if defined(ENABLE_TIMESYNC)
#  ifdef TM_IN_SYS_TIME
#    ifdef TIME_WITH_SYS_TIME
#      include <sys/time.h>
#      include <time.h>
#    else
#      ifdef HAVE_SYS_TIME_H
#        include <sys/time.h>
#      else
#        include <time.h>
#      endif
#    endif
#  else
#    include <time.h>
#  endif
#endif

#if defined(HAVE_SYS_MMAN_H) && defined(HAVE_MMAP)
#  include <sys/mman.h>
size_t mm_size;
void *mm_addr=NULL;
#else
#  undef HAVE_MMAP
#endif
#include "timing.h"
extern int errno;
#define sendline(c) putchar(c & 0377)
#define xsendline(c) putchar(c)


#define PATHLEN 256
#define OK 0
#define FALSE 0
#define TRUE 1
#define ERROR (-1)
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
#define WANTG 0107	/* Send G not NAK to get nonstop batch xmsn */
#define TIMEOUT (-2)
#define RCDO (-3)
#define RETRYMAX 10


#define HOWMANY 2
int Zmodem=0;		/* ZMODEM protocol requested by receiver */
unsigned Baudrate=2400;	/* Default, should be set by first mode() call */
unsigned Txwindow;	/* Control the size of the transmitted window */
unsigned Txwspac;	/* Spacing between zcrcq requests */
unsigned Txwcnt;	/* Counter used to space ack requests */
long Lrxpos;		/* Receiver's last reported offset */
int errors;

int Canseek=1; /* 1: can; 0: only rewind, -1: neither */

#include "rbsb.c"	/* most of the system dependent stuff here */

#include "crctab.c"

static int wctx(off_t flen);

int Filesleft;
long Totalleft,Filesize;

/*
 * Attention string to be executed by receiver to interrupt streaming data
 *  when an error is detected.  A pause (0336) may be needed before the
 *  ^C (03) or after it.
 */
#ifdef READCHECK
char Myattn[] = { 0 };
#else
char Myattn[] = { 03, 0336, 0 };
#endif

FILE *in;

#define MAX_BLOCK 8192
char txbuf[MAX_BLOCK];

long vpos = 0;			/* Number of bytes read from file */

char Lastrx;
char Crcflg;
int Verbose=0;
int Modem2=0;		/* XMODEM Protocol - don't send pathnames */
int Restricted=0;	/* restricted; no /.. or ../ in filenames */
int Quiet=0;		/* overrides logic that would otherwise set verbose */
int Ascii=0;		/* Add CR's for brain damaged programs */
int Fullname=0;		/* transmit full pathname */
int Unlinkafter=0;	/* Unlink file after it is sent */
int Dottoslash=0;	/* Change foo.bar.baz to foo/bar/baz */
int firstsec;
int errcnt=0;		/* number of files unreadable */
int blklen=128;		/* length of transmitted records */
int Optiong;		/* Let it rip no wait for sector ACK's */
int Eofseen;		/* EOF seen on input set by zfilbuf */
int Totsecs;		/* total number of sectors this file */
int Filcnt=0;		/* count of number of files opened */
int Lfseen=0;
unsigned Rxbuflen = 16384;	/* Receiver's max buffer length */
int Tframlen = 0;	/* Override for tx frame length */
int blkopt=0;		/* Override value for zmodem blklen */
int Rxflags = 0;
int Rxflags2 = 0;
long bytcnt;
int Wantfcs32 = TRUE;	/* want to send 32 bit FCS */
char Lzconv;	/* Local ZMODEM file conversion request */
char Lzmanag;	/* Local ZMODEM file management request */
int Lskipnocor;
char Lztrans;
char zconv;		/* ZMODEM file conversion request */
char zmanag;		/* ZMODEM file management request */
char ztrans;		/* ZMODEM file transport request */
int Command;		/* Send a command, then exit. */
char *Cmdstr;		/* Pointer to the command string */
int Cmdtries = 11;
int Cmdack1;		/* Rx ACKs command, then do it */
int Exitcode;
int enable_timesync=0;
int Test;		/* 1= Force receiver to send Attn, etc with qbf. */
			/* 2= Character transparency test */
char *qbf="The quick brown fox jumped over the lazy dog's back 1234567890\r\n";
long Lastsync;		/* Last offset to which we got a ZRPOS */
int Beenhereb4;		/* How many times we've been ZRPOS'd same place */

int no_timeout=FALSE;
int max_blklen=1024;
int start_blklen=1024;
#ifdef NEW_ERROR
int error_count;
#define OVERHEAD 18
#define OVER_ERR 20
#endif
#define MK_STRING(x) MK_STRING2(x)
#define MK_STRING2(x) #x


jmp_buf intrjmp;	/* For the interrupt on RX CAN */

/* called by signal interrupt or terminate to clean things up */
RETSIGTYPE
bibi(int n)
{
	canit(); fflush(stdout); mode(0);
	fprintf(stderr, "sz: caught signal %d; exiting\n", n);
	if (n == SIGQUIT)
		abort();
	if (n == 99)
		fprintf(stderr, "mode(2) in rbsb.c not implemented!!\n");
	cucheck();
	exit(128+n);
}
/* Called when ZMODEM gets an interrupt (^X) */
RETSIGTYPE
onintr()
{
	signal(SIGINT, SIG_IGN);
	longjmp(intrjmp, -1);
}

int Zctlesc;	/* Encode control characters */
int Nozmodem = 0;	/* If invoked as "sb" */
char *Progname = "sz";
int Zrwindow = 1400;	/* RX window size (controls garbage count) */

#define READLINE_PF(x) readline(x)
#include "zm.c"


int 
main(int argc, char **argv)
{
	register char *cp;
	register npats;
	int dm;
	int under_rsh=FALSE;
	char **patts;

	if ((cp = getenv("ZNULLS")) && *cp)
		Znulls = atoi(cp);
	if ((cp=getenv("SHELL")) && (strstr(cp, "rsh") || strstr(cp, "rksh")
		|| strstr(cp, "rbash")))
	{
		under_rsh=TRUE;
		Restricted=1;
	}
	if ((cp=getenv("ZMODEM_RESTRICTED"))!=NULL)
		Restricted=1;
	from_cu();
	chkinvok(argv[0]);

	Rxtimeout = 600;
	npats=0;
	if (argc<2)
		usage();
	while (--argc) {
		cp = *++argv;
		if (*cp++ == '-' && *cp) {
			while ( *cp) {
				switch(*cp++) {
				case '\\':
					 *cp = toupper(*cp);  continue;
				case '+':
					Lzmanag = ZMAPND; break;
#ifdef CSTOPB
				case '2':
					Twostop = TRUE; break;
#endif
				case '8':
					if (max_blklen==8192)
						start_blklen=8192;
					else
						max_blklen=8192;
					break;
				case 'a':
					Lzconv = ZCNL;
					Ascii = TRUE; break;
				case 'b':
					Lzconv = ZCBIN; break;
				case 'C':
					if (--argc < 1) {
						usage();
					}
					Cmdtries = atoi(*++argv);
					break;
				case 'i':
					Cmdack1 = ZCACK1;
					/* **** FALL THROUGH TO **** */
				case 'c':
					if (--argc != 1) {
						usage();
					}
					Command = TRUE;
					Cmdstr = *++argv;
					break;
				case 'd':
					++Dottoslash;
					/* **** FALL THROUGH TO **** */
				case 'f':
					Fullname=TRUE; break;
				case 'e':
					Zctlesc = 1; break;
				case 'h':
					usage(); break;
				case 'k':
					start_blklen=1024; break;
				case 'L':
					if (--argc < 1) {
						usage();
					}
					blkopt = atoi(*++argv);
					if (blkopt<24 || blkopt>MAX_BLOCK)
						usage();
					break;
				case 'l':
					if (--argc < 1) {
						usage();
					}
					Tframlen = atoi(*++argv);
					if (Tframlen<32 || Tframlen>MAX_BLOCK)
						usage();
					break;
				case 'N':
					Lzmanag = ZMNEWL;  break;
				case 'n':
					Lzmanag = ZMNEW;  break;
				case 'o':
					Wantfcs32 = FALSE; break;
				case 'O':
					no_timeout = TRUE; break;
				case 'p':
					Lzmanag = ZMPROT;  break;
				case 'r':
					Lzconv = ZCRESUM; break;
				case 'R':
					Restricted = TRUE; break;
				case 'q':
					Quiet=TRUE; Verbose=0; break;
				case 'S':
				    enable_timesync=1;
				    break;
				case 't':
					if (--argc < 1) {
						usage();
					}
					Rxtimeout = atoi(*++argv);
					if (Rxtimeout<10 || Rxtimeout>1000)
						usage();
					break;
				case 'T':
					if (++Test > 1) {
						chartest(1); chartest(2);
						mode(0);  exit(0);
					}
					break;
				case 'u':
					++Unlinkafter; break;
				case 'U':
					if (!under_rsh)
						Restricted=0;
					else {
fprintf(stderr,"security violation: running under restricted shell\n",stderr);
						exit(1);
					}
					break;
				case 'v':
					++Verbose; break;
				case 'w':
					if (--argc < 1) {
						usage();
					}
					Txwindow = atoi(*++argv);
					if (Txwindow < 256)
						Txwindow = 256;
					Txwindow = (Txwindow/64) * 64;
					Txwspac = Txwindow/4;
					if (blkopt > Txwspac
					 || (!blkopt && Txwspac < MAX_BLOCK))
						blkopt = Txwspac;
					break;
				case 'X':
					++Modem2; break;
				case 'Y':
					Lskipnocor = TRUE;
					/* **** FALLL THROUGH TO **** */
				case 'y':
					Lzmanag = ZMCLOB; break;
				default:
					usage();
				}
			}
		}
		else if ( !npats && argc>0) {
			if (argv[0][0]) {
				npats=argc;
				patts=argv;
				if ( !strcmp(*patts, "-"))
					iofd = 1;
			}
		}
	}
	if (npats < 1 && !Command && !Test) 
		usage();
	if (Command && Restricted) {
		printf("Can't send command in restricted mode\n");
		exit(1);
	}

	if (Verbose) {
#if 0
		if (freopen(LOGFILE, "a", stderr)==NULL) {
			printf("Can't open log file %s\n",LOGFILE);
			exit(0200);
		}
#endif
		setbuf(stderr, NULL);
	}
	if (Fromcu && !Quiet) {
		if (Verbose == 0)
			Verbose = 2;
	}
	vfile("%s %s for %s\n", Progname, VERSION, OS);

	{
		/* we write max_blocklen (data) + 18 (ZModem protocol overhead)
		 * + escape overhead (about 4 %), so buffer has to be
		 * somewhat larger than max_blklen 
		 */
		char *s=malloc(max_blklen+1024);
		if (!s)
		{
			fprintf(stderr,"lsz: out of memory\n");
			exit(1);
		}
#ifdef SETVBUF_REVERSED
		setvbuf(stdout,_IOFBF,s,max_blklen+1024);
#else
		setvbuf(stdout,s,_IOFBF,max_blklen+1024);
#endif
	}
	blklen=start_blklen;

	mode(1);

#ifndef linux
	if (signal(SIGINT, bibi) == SIG_IGN) {
		signal(SIGINT, SIG_IGN); signal(SIGKILL, SIG_IGN);
	} else {
		signal(SIGINT, bibi); signal(SIGKILL, bibi);
	}
	if ( !Fromcu)
		signal(SIGQUIT, SIG_IGN);
	signal(SIGTERM, bibi);
#endif

	if ( !Modem2) {
		if (!Nozmodem) {
			printf("rz\r");  fflush(stdout);
		}
		countem(npats, patts);
		if (!Nozmodem) {
			stohdr(0L);
			if (Command)
				Txhdr[ZF0] = ZCOMMAND;
			zshhdr(ZRQINIT, Txhdr);
#if defined(ENABLE_TIMESYNC)
			if (Rxflags2 != ZF1_TIMESYNC)
				/* disable timesync if there are any flags we don't know.
				 * dsz/gsz seems to use some other flags! */
				enable_timesync=FALSE;
			if (Rxflags2 & ZF1_TIMESYNC && enable_timesync) {
				Totalleft+=6; /* TIMESYNC never needs more */
				Filesleft++;
			}
#endif
		}
	}
	fflush(stdout);

	if (Command) {
		if (getzrxinit()) {
			Exitcode=0200; canit();
		}
		else if (zsendcmd(Cmdstr, 1+strlen(Cmdstr))) {
			Exitcode=0200; canit();
		}
	} else if (wcsend(npats, patts)==ERROR) {
		Exitcode=0200;
		canit();
	}
	fflush(stdout);
	mode(0);
	dm = ((errcnt != 0) | Exitcode);
	if (dm) {
		cucheck();  exit(dm);
	}
	exit(SS_NORMAL);
	/*NOTREACHED*/
}

wcsend(argc, argp)
char *argp[];
{
	register n;

	Crcflg=FALSE;
	firstsec=TRUE;
	bytcnt = -1;
	for (n=0; n<argc; ++n) {
		Totsecs = 0;
		if (wcs(argp[n])==ERROR)
			return ERROR;
	}
#if defined(ENABLE_TIMESYNC)
	if (Rxflags2 & ZF1_TIMESYNC && enable_timesync)
	{
		/* implement Peter Mandrellas extension */
		/* yes, this *has* a minor race condition */
		char tmp[PATH_MAX];
		const char *p;
		FILE *f;
		if (Verbose)
			fprintf(stderr, "\r\nAnswering TIMESYNC");
		p=getenv("TMPDIR");
		if (!p)
			p=getenv("TMP");
		if (!p)
			p="/tmp";
		strcpy(tmp,p);
		strcat(tmp,"/$time$.t");
		f=fopen(tmp,"w");
		if (f)
		{
			char buf[30];
			time_t t=time(NULL);
			struct tm *tm=localtime(&t); /* sets timezone */
#ifdef HAVE_STRFTIME
			strftime(buf,sizeof(buf)-1,"%H:%M:%S",tm);
			if (Verbose)
				fprintf(stderr, " at %s",buf);
#endif
#if defined(HAVE_TIMEZONE_VAR)
			fprintf(f,"%ld\r\n",timezone / 60);
			if (Verbose)
				fprintf(stderr, " (tz %ld)\r\n",timezone/60);
#else
			if (Verbose)
				fprintf(stderr, " (tz unknown)\r\n");
#endif
			fclose(f);
			if (wcs(tmp)==ERROR)
				if (Verbose)
					fprintf(stderr, "\r\nTIMESYNC: failed\n");
			else
			{
				if (Verbose)
					fprintf(stderr, "\r\nTIMESYNC: ok\n");
				Filcnt--;
			}
			unlink(tmp);
		}
		else 
		{
			if (!Verbose)
				fprintf(stderr, "\r\nAnswering TIMESYNC\n");
			else
				fprintf(stderr, "\r\n");
			fprintf(stderr, "  cannot open tmpfile %s: %s\r\n",tmp, 
				strerror(errno));
		}
	}
#endif
	Totsecs = 0;
	if (Filcnt==0) {	/* bitch if we couldn't open ANY files */
		if ( !Modem2) {
			Command = TRUE;
			Cmdstr = "echo \"lsz: Can't open any requested files\"";
			if (getnak()) {
				Exitcode=0200; canit();
			}
			if (!Zmodem)
				canit();
			else if (zsendcmd(Cmdstr, 1+strlen(Cmdstr))) {
				Exitcode=0200; canit();
			}
			Exitcode = 1; return OK;
		}
		canit();
		fprintf(stderr,"\r\nCan't open any requested files.\r\n");
		return ERROR;
	}
	if (Zmodem)
		saybibi();
	else if ( !Modem2)
		wctxpn("");
	return OK;
}

wcs(oname)
char *oname;
{
	register c;
	register char *p;
	struct stat f;
	char name[PATHLEN];

	strcpy(name, oname);

	if (Restricted) {
		/* restrict pathnames to current tree or uucppublic */
		if ( strstr(name, "../")
#ifdef PUBDIR
		 || (name[0]== '/' && strncmp(name, MK_STRING(PUBDIR),
		 	strlen(MK_STRING(PUBDIR))))
#endif
		) {
			canit();
			fprintf(stderr,"\r\nlsz:\tSecurity Violation\r\n");
			return ERROR;
		}
	}

	if ( !strcmp(oname, "-")) {
		if ((p = getenv("ONAME")) && *p)
			strcpy(name, p);
		else
			sprintf(name, "s%d.lsz", getpid());
		in = stdin;
	}
	else if ((in=fopen(oname, "r"))==NULL) {
		++errcnt;
		return OK;	/* pass over it, there may be others */
	}
	{
		static char *s=NULL;
		if (!s) {
			s=malloc(16384);
			if (!s) {
				fprintf(stderr,"lsz: out of memory\n");
				exit(1);
			}
		}
#ifdef SETVBUF_REVERSED
		setvbuf(in,_IOFBF,s,16384);
#else
		setvbuf(in,s,_IOFBF,16384);
#endif
	}
	timing(1);
	Eofseen = 0;  vpos = 0;
	/* Check for directory or block special files */
	fstat(fileno(in), &f);
	c = f.st_mode & S_IFMT;
	if (c == S_IFDIR || c == S_IFBLK) {
		fclose(in);
		return OK;
	}

	++Filcnt;
	switch (wctxpn(name)) {
	case ERROR:
		return ERROR;
	case ZSKIP:
		return OK;
	}
	if (!Zmodem && wctx(f.st_size)==ERROR)
		return ERROR;
	if (Unlinkafter)
		unlink(oname);
	return 0;
}

/*
 * generate and transmit pathname block consisting of
 *  pathname (null terminated),
 *  file length, mode time and file mode in octal
 *  as provided by the Unix fstat call.
 *  N.B.: modifies the passed name, may extend it!
 */
wctxpn(name)
char *name;
{
	register char *p, *q;
	char name2[PATHLEN];
	struct stat f;

	if (Modem2) {
		if ((in!=stdin) && *name && fstat(fileno(in), &f)!= -1) {
			fprintf(stderr, "Sending %s, %ld blocks: ",
			  name, (long) (f.st_size>>7));
		}
		fprintf(stderr, "Give your local XMODEM receive command now.\r\n");
		return OK;
	}
	if ( !Zmodem)
		if (getnak())
			return ERROR;

	q = (char *) 0;
	if (Dottoslash) {		/* change . to . */
		for (p=name; *p; ++p) {
			if (*p == '/')
				q = p;
			else if (*p == '.')
				*(q=p) = '/';
		}
		if (q && strlen(++q) > 8) {	/* If name>8 chars */
			q += 8;			/*   make it .ext */
			strcpy(name2, q);	/* save excess of name */
			*q = '.';
			strcpy(++q, name2);	/* add it back */
		}
	}

	for (p=name, q=txbuf ; *p; )
		if ((*q++ = *p++) == '/' && !Fullname)
			q = txbuf;
	*q++ = 0;
	p=q;
	while (q < (txbuf + MAX_BLOCK))
		*q++ = 0;
	if (!Ascii && (in!=stdin) && *name && fstat(fileno(in), &f)!= -1)
		sprintf(p, "%lu %lo %o 0 %d %ld", (long) f.st_size, f.st_mtime,
		  f.st_mode, Filesleft, Totalleft);
	fprintf(stderr, "Sending: %s\n",name);
	Totalleft -= f.st_size;
	Filesize = f.st_size;
	if (--Filesleft <= 0)
		Totalleft = 0;
	if (Totalleft < 0)
		Totalleft = 0;

	/* force 1k blocks if name won't fit in 128 byte block */
	if (txbuf[125])
		blklen=1024;
	else {		/* A little goodie for IMP/KMD */
		txbuf[127] = (f.st_size + 127) >>7;
		txbuf[126] = (f.st_size + 127) >>15;
	}
	if (Zmodem)
		return zsendfile(txbuf, 1+strlen(p)+(p-txbuf));
	if (wcputsec(txbuf, 0, 128)==ERROR)
		return ERROR;
	return OK;
}

getnak()
{
	register firstch;

	Lastrx = 0;
	for (;;) {
		switch (firstch = readline(800)) {
		case ZPAD:
			if (getzrxinit())
				return ERROR;
			Ascii = 0;	/* Receiver does the conversion */
			return FALSE;
		case TIMEOUT:
			zperr("Timeout on pathname");
			return TRUE;
		case WANTG:
#ifdef MODE2OK
			mode(2);	/* Set cbreak, XON/XOFF, etc. */
#endif
			Optiong = TRUE;
			blklen=1024;
		case WANTCRC:
			Crcflg = TRUE;
		case NAK:
			return FALSE;
		case CAN:
			if ((firstch = readline(20)) == CAN && Lastrx == CAN)
				return TRUE;
		default:
			break;
		}
		Lastrx = firstch;
	}
}


static int 
wctx(off_t flen)
{
	register int thisblklen;
	register int sectnum, attempts, firstch;
	long charssent;

	charssent = 0;  firstsec=TRUE;  thisblklen = blklen;
	vfile("wctx:file length=%ld", (long) flen);

	while ((firstch=readline(Rxtimeout))!=NAK && firstch != WANTCRC
	  && firstch != WANTG && firstch!=TIMEOUT && firstch!=CAN)
		;
	if (firstch==CAN) {
		zperr("Receiver Cancelled");
		return ERROR;
	}
	if (firstch==WANTCRC)
		Crcflg=TRUE;
	if (firstch==WANTG)
		Crcflg=TRUE;
	sectnum=0;
	for (;;) {
		if (flen <= (charssent + 896L))
			thisblklen = 128;
		if ( !filbuf(txbuf, thisblklen))
			break;
		if (wcputsec(txbuf, ++sectnum, thisblklen)==ERROR)
			return ERROR;
		charssent += thisblklen;
	}
	fclose(in);
	attempts=0;
	do {
		purgeline();
		sendline(EOT);
		fflush(stdout);
		++attempts;
	}
		while ((firstch=(readline(Rxtimeout)) != ACK) && attempts < RETRYMAX);
	if (attempts == RETRYMAX) {
		zperr("No ACK on EOT");
		return ERROR;
	}
	else
		return OK;
}

wcputsec(buf, sectnum, cseclen)
char *buf;
int sectnum;
int cseclen;	/* data length of this sector to send */
{
	register checksum, wcj;
	register char *cp;
	unsigned oldcrc;
	int firstch;
	int attempts;

	firstch=0;	/* part of logic to detect CAN CAN */

	if (Verbose>1)
		fprintf(stderr, "\rYmodem sectors/kbytes sent: %3d/%2dk", Totsecs, Totsecs/8 );
	for (attempts=0; attempts <= RETRYMAX; attempts++) {
		Lastrx= firstch;
		sendline(cseclen==1024?STX:SOH);
		sendline(sectnum);
		sendline(-sectnum -1);
		oldcrc=checksum=0;
		for (wcj=cseclen,cp=buf; --wcj>=0; ) {
			sendline(*cp);
			oldcrc=updcrc((0377& *cp), oldcrc);
			checksum += *cp++;
		}
		if (Crcflg) {
			oldcrc=updcrc(0,updcrc(0,oldcrc));
			sendline((int)oldcrc>>8);
			sendline((int)oldcrc);
		}
		else
			sendline(checksum);

		if (Optiong) {
			firstsec = FALSE; return OK;
		}
		firstch = readline(Rxtimeout);
gotnak:
		switch (firstch) {
		case CAN:
			if(Lastrx == CAN) {
cancan:
				zperr("Cancelled");  return ERROR;
			}
			break;
		case TIMEOUT:
			zperr("Timeout on sector ACK"); continue;
		case WANTCRC:
			if (firstsec)
				Crcflg = TRUE;
		case NAK:
			zperr("NAK on sector"); continue;
		case ACK: 
			firstsec=FALSE;
			Totsecs += (cseclen>>7);
			return OK;
		case ERROR:
			zperr("Got burst for sector ACK"); break;
		default:
			zperr("Got %02x for sector ACK", firstch); break;
		}
		for (;;) {
			Lastrx = firstch;
			if ((firstch = readline(Rxtimeout)) == TIMEOUT)
				break;
			if (firstch == NAK || firstch == WANTCRC)
				goto gotnak;
			if (firstch == CAN && Lastrx == CAN)
				goto cancan;
		}
	}
	zperr("Retry Count Exceeded");
	return ERROR;
}

/* fill buf with count chars padding with ^Z for CPM */
filbuf(buf, count)
register char *buf;
{
	register c, m;

	if ( !Ascii) {
		m = read(fileno(in), buf, count);
		if (m <= 0)
			return 0;
		while (m < count)
			buf[m++] = 032;
		return count;
	}
	m=count;
	if (Lfseen) {
		*buf++ = 012; --m; Lfseen = 0;
	}
	while ((c=getc(in))!=EOF) {
		if (c == 012) {
			*buf++ = 015;
			if (--m == 0) {
				Lfseen = TRUE; break;
			}
		}
		*buf++ =c;
		if (--m == 0)
			break;
	}
	if (m==count)
		return 0;
	else
		while (--m>=0)
			*buf++ = CPMEOF;
	return count;
}

/* Fill buffer with blklen chars */
zfilbuf()
{
	int n;

	n = fread(txbuf, 1, blklen, in);
	if (n < blklen)
		Eofseen = 1;
	return n;
}

/* VARARGS1 */
vfile(f, a, b, c)
register char *f;
{
	if (Verbose > 2) {
		fprintf(stderr, f, a, b, c);
		fprintf(stderr, "\n");
	}
}


RETSIGTYPE
alrm(int dummy)
{
	/* this doesn't need to do anything - it interrupts read(), and
	 * that's enough. */
}


/*
 * readline(timeout) reads character(s) from file descriptor 0
 * timeout is in tenths of seconds
 */
readline(timeout)
{
	register int c;
	static char buf[64];
	static char *bufptr=buf;
	static int bufleft=0;

	if (timeout==-1)
	{
		bufleft=0;
		return 0;
	}

	if (bufleft)
	{
		c=*bufptr & 0377;
		bufptr++;
		bufleft--;
		if (Verbose>5)
			fprintf(stderr, "ret %x\n", c);
		return c;
	}

	fflush(stdout);
	if (!no_timeout) {
		c = timeout/10;
		if (c<2)
			c=2;
		if (Verbose>5) {
			fprintf(stderr, "Timeout=%d Calling alarm(%d) ", timeout, c);
		}
		signal(SIGALRM, alrm); alarm(c);
	} else if (Verbose>5) 
		fprintf(stderr, "Calling read ");
	bufleft=read(iofd, buf, sizeof(buf));
	if (!no_timeout)
		alarm(0);
	if (Verbose>5)
		fprintf(stderr, "ret %x\n", buf[0]);
	if (bufleft<1)
		return TIMEOUT;
	bufptr=buf+1;
	bufleft--;

	return (buf[0]&0377);
}

flushmo()
{
	fflush(stdout);
}


purgeline()
{
	readline(-1);
#ifdef TCFLSH
	ioctl(iofd, TCFLSH, 0);
#else
	lseek(iofd, 0L, 2);
#endif
}

/* send cancel string to get the other end to shut up */
canit()
{
	static char canistr[] = {
	 24,24,24,24,24,24,24,24,24,24,8,8,8,8,8,8,8,8,8,8,0
	};

	printf(canistr);
	fflush(stdout);
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


char *babble[] = {
	"Send file(s) with ZMODEM/YMODEM/XMODEM Protocol",
	"	(Y) = Option applies to YMODEM only",
	"	(Z) = Option applies to ZMODEM only",
	"Usage:	lsz [-2+abdefkLlNnquvwYy] [-] file ...",
	"	lsz [-2Ceqv] -c COMMAND",
	"	lsb [-2adfkquv] [-] file ...",
	"	lsx [-2akquv] [-] file",
#ifdef CSTOPB
	"	2 Use 2 stop bits",
#endif
	"	+ Append to existing destination file (Z)",
	"	a (ASCII) change NL to CR/LF",
	"	b Binary file transfer override",
	"	c send COMMAND (Z)",
	"	d Change '.' to '/' in pathnames (Y/Z)",
	"	e Escape all control characters (Z)",
	"	f send Full pathname (Y/Z)",
	"	i send COMMAND, ack Immediately (Z)",
	"	h Print this usage message",
	"	k Send 1024 byte packets (Y)",
	"	L N Limit subpacket length to N bytes (Z)",
	"	l N Limit frame length to N bytes (l>=L) (Z)",
	"	n send file if source newer (Z)",
	"	N send file if source newer or longer (Z)",
	"	o Use 16 bit CRC instead of 32 bit CRC (Z)",
	"	p Protect existing destination file (Z)",
	"	r Resume/Recover interrupted file transfer (Z)",
	"	q Quiet (no progress reports)",
	"	u Unlink file after transmission",
	"	v Verbose - provide debugging information",
	"	w N Window is N bytes (Z)",
	"	Y Yes, overwrite existing file, skip if not present at rx (Z)",
	"	y Yes, overwrite existing file (Z)",
	"	- as pathname sends standard input",
	""
};

usage()
{
	char **pp;

	for (pp=babble; **pp; ++pp)
		fprintf(stderr, "%s\n", *pp);
	fprintf(stderr, "\t%s version %s for %s %s\n", Progname, VERSION,
		CPU, OS);
	cucheck();
	exit(SS_NORMAL);
}

/*
 * Get the receiver's init parameters
 */
getzrxinit()
{
	register n;
	struct stat f;

	for (n=10; --n>=0; ) {
		
		switch (zgethdr(Rxhdr, 1)) {
		case ZCHALLENGE:	/* Echo receiver's challenge numbr */
			stohdr(Rxpos);
			zshhdr(ZACK, Txhdr);
			continue;
		case ZCOMMAND:		/* They didn't see out ZRQINIT */
			stohdr(0L);
			zshhdr(ZRQINIT, Txhdr);
			continue;
		case ZRINIT:
			Rxflags = 0377 & Rxhdr[ZF0];
			Rxflags2 = 0377 & Rxhdr[ZF1];
			Txfcs32 = (Wantfcs32 && (Rxflags & CANFC32));
			Zctlesc |= Rxflags & TESCCTL;
			Rxbuflen = (0377 & Rxhdr[ZP0])+((0377 & Rxhdr[ZP1])<<8);
			if ( !(Rxflags & CANFDX))
				Txwindow = 0;
			vfile("Rxbuflen=%d Tframlen=%d", Rxbuflen, Tframlen);
			if ( !Fromcu)
				signal(SIGINT, SIG_IGN);
#ifdef MODE2OK
			mode(2);	/* Set cbreak, XON/XOFF, etc. */
#endif
#ifndef READCHECK
			/* Use MAX_BLOCK byte frames if no sample/interrupt */
			if (Rxbuflen < 32 || Rxbuflen > MAX_BLOCK) {
				Rxbuflen = MAX_BLOCK;
				vfile("Rxbuflen=%d", Rxbuflen);
			}
#endif
			/* Override to force shorter frame length */
			if (Rxbuflen && (Rxbuflen>Tframlen) && (Tframlen>=32))
				Rxbuflen = Tframlen;
			if ( !Rxbuflen && (Tframlen>=32) && (Tframlen<=MAX_BLOCK))
				Rxbuflen = Tframlen;
			vfile("Rxbuflen=%d", Rxbuflen);

			/* If using a pipe for testing set lower buf len */
			fstat(iofd, &f);
			if ((f.st_mode & S_IFMT) != S_IFCHR) {
				Rxbuflen = MAX_BLOCK;
			}
			/*
			 * If input is not a regular file, force ACK's to
			 *  prevent running beyond the buffer limits
			 */
			if ( !Command) {
				fstat(fileno(in), &f);
				if ((f.st_mode & S_IFMT) != S_IFREG) {
					Canseek = -1;
					return ERROR;
				}
			}
			/* Set initial subpacket length */
			if (blklen < 1024) {	/* Command line override? */
				if (Baudrate > 300)
					blklen = 256;
				if (Baudrate > 1200)
					blklen = 512;
				if (Baudrate > 2400)
					blklen = 1024;
			}
			if (Rxbuflen && blklen>Rxbuflen)
				blklen = Rxbuflen;
			if (blkopt && blklen > blkopt)
				blklen = blkopt;
			vfile("Rxbuflen=%d blklen=%d", Rxbuflen, blklen);
			vfile("Txwindow = %u Txwspac = %d", Txwindow, Txwspac);

			return (sendzsinit());
		case ZCAN:
		case TIMEOUT:
			return ERROR;
		case ZRQINIT:
			if (Rxhdr[ZF0] == ZCOMMAND)
				continue;
		default:
			zshhdr(ZNAK, Txhdr);
			continue;
		}
	}
	return ERROR;
}

/* Send send-init information */
sendzsinit()
{
	register c;

	if (Myattn[0] == '\0' && (!Zctlesc || (Rxflags & TESCCTL)))
		return OK;
	errors = 0;
	for (;;) {
		stohdr(0L);
		if (Zctlesc) {
			Txhdr[ZF0] |= TESCCTL; zshhdr(ZSINIT, Txhdr);
		}
		else
			zsbhdr(ZSINIT, Txhdr);
		zsdata(Myattn, 1+strlen(Myattn), ZCRCW);
		c = zgethdr(Rxhdr, 1);
		switch (c) {
		case ZCAN:
			return ERROR;
		case ZACK:
			return OK;
		default:
			if (++errors > 19)
				return ERROR;
			continue;
		}
	}
}

/* Send file name and related info */
zsendfile(buf, blen)
char *buf;
{
	register c;
	register unsigned long crc;

	for (;;) {
		Txhdr[ZF0] = Lzconv;	/* file conversion request */
		Txhdr[ZF1] = Lzmanag;	/* file management request */
		if (Lskipnocor)
			Txhdr[ZF1] |= ZMSKNOLOC;
		Txhdr[ZF2] = Lztrans;	/* file transport request */
		Txhdr[ZF3] = 0;
		zsbhdr(ZFILE, Txhdr);
		zsdata(buf, blen, ZCRCW);
again:
		c = zgethdr(Rxhdr, 1);
		switch (c) {
		case ZRINIT:
			while ((c = readline(50)) > 0)
				if (c == ZPAD) {
					goto again;
				}
			/* **** FALL THRU TO **** */
		default:
			continue;
		case ZCAN:
		case TIMEOUT:
		case ZABORT:
		case ZFIN:
			return ERROR;
		case ZCRC:
			crc = 0xFFFFFFFFL;
#ifdef HAVE_MMAP
			if (mm_addr) {
				size_t i;
				char *p=mm_addr;
				for (i=0;i<Rxpos && i<mm_size;i++,p++) {
					crc = UPDC32(*p, crc);
				}
				crc = ~crc;
			} else
#endif
			if (Canseek >= 0) {
				while (((c = getc(in)) != EOF) && --Rxpos)
					crc = UPDC32(c, crc);
				crc = ~crc;
				clearerr(in);	/* Clear EOF */
				fseek(in, 0L, 0);
			}
			stohdr(crc);
			zsbhdr(ZCRC, Txhdr);
			goto again;
		case ZSKIP:
			if (in)
				fclose(in);
			return c;
		case ZRPOS:
			/*
			 * Suppress zcrcw request otherwise triggered by
			 * lastyunc==bytcnt
			 */
#ifdef HAVE_MMAP
			if (!mm_addr)
#endif
			if (Rxpos && fseek(in, Rxpos, 0))
				return ERROR;
			bytcnt = Txpos = Rxpos;
			Lastsync = Rxpos -1;
			return zsendfdata();
		}
	}
}

/* Send the data in the file */
zsendfdata()
{
	register c, e, n;
	register newcnt;
	register long tcount = 0;
	int junkcount;		/* Counts garbage chars received by TX */
	static int tleft = 6;	/* Counter for test mode */
	long last_txpos=0;
	long last_bps=0;
	long not_printed=0;
	static long total_sent=0;

#ifdef HAVE_MMAP
	{
		struct stat st;
		if (fstat(fileno(in),&st)==0)
		{
			mm_size=st.st_size;
	    		mm_addr = mmap (0, mm_size, PROT_READ, 
	    			MAP_SHARED, fileno(in), 0);
	    		if ((caddr_t) mm_addr==(caddr_t) -1)
	    			mm_addr=NULL;
	    		else {
	    			fclose(in);
	    			in=NULL;
	    		}
		}
	}
#endif

	Lrxpos = 0;
	junkcount = 0;
	Beenhereb4 = 0;
somemore:
	if (setjmp(intrjmp)) {
waitack:
		junkcount = 0;
		c = getinsync(0);
gotack:
		switch (c) {
		default:
		case ZCAN:
			if (in)
				fclose(in);
			return ERROR;
		case ZSKIP:
			if (in)
				fclose(in);
			return c;
		case ZACK:
		case ZRPOS:
			break;
		case ZRINIT:
			return OK;
		}
#ifdef READCHECK
		/*
		 * If the reverse channel can be tested for data,
		 *  this logic may be used to detect error packets
		 *  sent by the receiver, in place of setjmp/longjmp
		 *  rdchk(fdes) returns non 0 if a character is available
		 */
		while (rdchk(iofd)) {
#ifdef READCHECK_READS
			switch (checked)
#else
			switch (readline(1))
#endif
			{
			case CAN:
			case ZPAD:
				c = getinsync(1);
				goto gotack;
			case XOFF:		/* Wait a while for an XON */
#ifndef linux
			case XOFF|0200:
#endif
				readline(100);
			}
		}
#endif
	}

#ifndef linux
	if ( !Fromcu)
		signal(SIGINT, onintr);
#endif

	newcnt = Rxbuflen;
	Txwcnt = 0;
	stohdr(Txpos);
	zsbhdr(ZDATA, Txhdr);

	/*
	 * Special testing mode.  This should force receiver to Attn,ZRPOS
	 *  many times.  Each time the signal should be caught, causing the
	 *  file to be started over from the beginning.
	 */
	if (Test) {
		if ( --tleft)
			while (tcount < 20000) {
				printf(qbf); fflush(stdout);
				tcount += strlen(qbf);
#ifdef READCHECK
				while (rdchk(iofd)) {
#ifdef READCHECK_READS
					switch (checked)
#else
					switch (readline(1))
#endif
					{
					case CAN:
					case ZPAD:
#ifdef TCFLSH
						ioctl(iofd, TCFLSH, 1);
#endif
						goto waitack;
					case XOFF:	/* Wait for XON */
#ifndef linux
					case XOFF|0200:
#endif
						readline(100);
					}
				}
#endif
			}
		signal(SIGINT, SIG_IGN); canit();
		sleep(3); purgeline(); mode(0);
		printf("\nlsz: Tcount = %ld\n", tcount);
		if (tleft) {
			printf("ERROR: Interrupts Not Caught\n");
			exit(1);
		}
		exit(SS_NORMAL);
	}

	do {
#ifdef NEW_ERROR
		int old=blklen;
		blklen=calc_blklen(total_sent);
		total_sent+=blklen+OVERHEAD;
		if (Verbose >2 && blklen!=old)
			fprintf(stderr,"blklen now %ld\n",blklen);
#endif
#ifdef HAVE_MMAP
		if (mm_addr) {
			if (Txpos+blklen<mm_size) 
				n=blklen;
			else {
				n=mm_size-Txpos;
				Eofseen=1;
			}
		} else 
#endif
		n = zfilbuf();
		if (Eofseen)
			e = ZCRCE;
		else if (junkcount > 3)
			e = ZCRCW;
		else if (bytcnt == Lastsync)
			e = ZCRCW;
		else if (Rxbuflen && (newcnt -= n) <= 0)
			e = ZCRCW;
		else if (Txwindow && (Txwcnt += n) >= Txwspac) {
			Txwcnt = 0;  e = ZCRCQ;
		}
		else
			e = ZCRCG;
		if (Verbose>1
			&& (not_printed > 5 || Txpos > last_bps / 2 + last_txpos)) {
			int minleft =  0;
			int secleft =  0;
			last_bps=(Txpos/timing(0));
			if (last_bps > 0) {
				minleft =  (Filesize-Txpos)/last_bps/60;
				secleft =  ((Filesize-Txpos)/last_bps)%60;
			}
			fprintf(stderr, "\rBytes Sent:%7ld/%7ld   BPS:%-6d ETA %02d:%02d  ",
			 Txpos, Filesize, last_bps, minleft, secleft);
			last_txpos=Txpos;
		} else if (Verbose)
			not_printed++;
#ifdef HAVE_MMAP
		if (mm_addr)
			zsdata(mm_addr+Txpos,n,e);
		else
#endif
		zsdata(txbuf, n, e);
		bytcnt = Txpos += n;
		if (e == ZCRCW)
			goto waitack;
#ifdef READCHECK
		/*
		 * If the reverse channel can be tested for data,
		 *  this logic may be used to detect error packets
		 *  sent by the receiver, in place of setjmp/longjmp
		 *  rdchk(fdes) returns non 0 if a character is available
		 */
		fflush(stdout);
		while (rdchk(iofd)) {
#ifdef READCHECK_READS
			switch (checked)
#else
			switch (readline(1))
#endif
			{
			case CAN:
			case ZPAD:
				c = getinsync(1);
				if (c == ZACK)
					break;
#ifdef TCFLSH
				ioctl(iofd, TCFLSH, 1);
#endif
				/* zcrce - dinna wanna starta ping-pong game */
				zsdata(txbuf, 0, ZCRCE);
				goto gotack;
			case XOFF:		/* Wait a while for an XON */
#ifndef linux
			case XOFF|0200:
#endif
				readline(100);
			default:
				++junkcount;
			}
		}
#endif	/* READCHECK */
		if (Txwindow) {
			while ((tcount = Txpos - Lrxpos) >= Txwindow) {
				vfile("%ld window >= %u", tcount, Txwindow);
				if (e != ZCRCQ)
					zsdata(txbuf, 0, e = ZCRCQ);
				c = getinsync(1);
				if (c != ZACK) {
#ifdef TCFLSH
					ioctl(iofd, TCFLSH, 1);
#endif
					zsdata(txbuf, 0, ZCRCE);
					goto gotack;
				}
			}
			vfile("window = %ld", tcount);
		}
	} while (!Eofseen);
	if (Verbose > 1)
		fprintf(stderr, "\rBytes Sent:%7ld   BPS:%-6d                       \n",
		Filesize,last_bps);
	if ( !Fromcu)
		signal(SIGINT, SIG_IGN);

	for (;;) {
		stohdr(Txpos);
		zsbhdr(ZEOF, Txhdr);
		switch (getinsync(0)) {
		case ZACK:
			continue;
		case ZRPOS:
			goto somemore;
		case ZRINIT:
			return OK;
		case ZSKIP:
			if (in)
				fclose(in);
			return c;
		default:
			if (in)
				fclose(in);
			return ERROR;
		}
	}
}

#ifdef NEW_ERROR
int
calc_blklen(long total_sent)
{
	static long total_bytes=0;
	static int calcs_done=0;
	static long last_error_count=0;
	static int last_blklen=0;
	static long last_bytes_per_error=0;
	long best_bytes=0;
	long best_size=0;
	long bytes_per_error;
	long d;
	int i;
	if (total_bytes==0)
	{
		/* called from countem */
		total_bytes=total_sent;
		return 0;
	}

	/* it's not good to calc blklen too early */
	if (calcs_done++ < 5) {
		if (error_count && start_blklen >1024)
			return last_blklen=1024;
		else 
			last_blklen/=2;
		return last_blklen=start_blklen;
	}

	if (!error_count) {
		/* that's fine */
		if (start_blklen==max_blklen)
			return start_blklen;
		bytes_per_error=LONG_MAX;
		goto calcit;
	}

	if (error_count!=last_error_count) {
		/* the last block was bad. shorten blocks until one block is
		 * ok. this is because very often many errors come in an
		 * short period */
		if (error_count & 2)
		{
			last_blklen/=2;
			if (last_blklen < 32)
				last_blklen = 32;
			else if (last_blklen > 512)
				last_blklen=512;
			if (Verbose > 3)
				fprintf(stderr,"calc_blklen: reduced to %d due to error\n",
					last_blklen);
			last_error_count=error_count;
			last_bytes_per_error=0; /* force recalc */
		}
		return last_blklen;
	}

	bytes_per_error=total_sent / error_count;
		/* we do not get told about every error! 
		 * from my experience the value is ok */
	bytes_per_error/=2;
	/* there has to be a margin */
	if (bytes_per_error<100)
		bytes_per_error=100;

	/* be nice to the poor machine and do the complicated things not
	 * too often
	 */
	if (last_bytes_per_error>bytes_per_error)
		d=last_bytes_per_error-bytes_per_error;
	else
		d=bytes_per_error-last_bytes_per_error;
	if (d<4)
	{
		if (Verbose > 3)
		{
			fprintf(stderr,"calc_blklen: returned old value %d due to low bpe diff\n",
				last_blklen);
			fprintf(stderr,"calc_blklen: old %ld, new %ld, d %ld\n",
				last_bytes_per_error,bytes_per_error,d );
		}
		return last_blklen;
	}
	last_bytes_per_error=bytes_per_error;

calcit:
	if (Verbose > 3)
		fprintf(stderr,"calc_blklen: calc total_bytes=%ld, bpe=%ld\n",
			total_bytes,bytes_per_error);
	for (i=32;i<=max_blklen;i*=2) {
		long ok; /* some many ok blocks do we need */
		long failed; /* and that's the number of blocks not transmitted ok */
		long transmitted;
		ok=total_bytes / i + 1;
		failed=((long) i + OVERHEAD) * ok / bytes_per_error;
		transmitted=ok * ((long) i+OVERHEAD)  
			+ failed * ((long) i+OVERHEAD+OVER_ERR);
		if (Verbose > 4)
			fprintf(stderr,"calc_blklen: blklen %d, ok %ld, failed %ld -> %ld\n",
				i,ok,failed,transmitted);
		if (transmitted < best_bytes || !best_bytes)
		{
			best_bytes=transmitted;
			best_size=i;
		}
	}
	if (best_size > 2*last_blklen)
		best_size=2*last_blklen;
	last_blklen=best_size;
	if (Verbose > 3)
		fprintf(stderr,"calc_blklen: returned %d as best\n",
			last_blklen);
	return last_blklen;
}
#endif

/*
 * Respond to receiver's complaint, get back in sync with receiver
 */
getinsync(flag)
{
	register c;

	for (;;) {
		if (Test) {
			printf("\r\n\n\n***** Signal Caught *****\r\n");
			Rxpos = 0; c = ZRPOS;
		} else
			c = zgethdr(Rxhdr, 0);
		switch (c) {
		case ZCAN:
		case ZABORT:
		case ZFIN:
		case TIMEOUT:
			return ERROR;
		case ZRPOS:
			/* ************************************* */
			/*  If sending to a buffered modem, you  */
			/*   might send a break at this point to */
			/*   dump the modem's buffer.		 */
			clearerr(in);	/* In case file EOF seen */
#ifdef HAVE_MMAP
			if (!mm_addr)
#endif
			if (fseek(in, Rxpos, 0))
				return ERROR;
			Eofseen = 0;
			bytcnt = Lrxpos = Txpos = Rxpos;
			if (Lastsync == Rxpos) {
#ifndef NEW_ERROR
				if (++Beenhereb4 > 4)
					if (blklen > 32)
					{
						blklen /= 2;
						if (Verbose > 1) {
							fprintf(stderr,"\rFalldown to %ld blklen\r\n",
								blklen);
						}
					}
#else
				error_count++;
#endif
			}
			Lastsync = Rxpos-1;
			return c;
		case ZACK:
			Lrxpos = Rxpos;
			if (flag || Txpos == Rxpos)
				return ZACK;
			continue;
		case ZRINIT:
		case ZSKIP:
			if (in)
				fclose(in);
			return c;
		case ERROR:
		default:
#ifdef NEW_ERROR
				error_count++;
#endif
			zsbhdr(ZNAK, Txhdr);
			continue;
		}
	}
}


/* Say "bibi" to the receiver, try to do it cleanly */
saybibi()
{
	for (;;) {
		stohdr(0L);		/* CAF Was zsbhdr - minor change */
		zshhdr(ZFIN, Txhdr);	/*  to make debugging easier */
		switch (zgethdr(Rxhdr, 0)) {
		case ZFIN:
			sendline('O'); sendline('O'); flushmo();
		case ZCAN:
		case TIMEOUT:
			return;
		}
	}
}

/* Local screen character display function */
bttyout(c)
{
	if (Verbose)
		putc(c, stderr);
}

/* Send command and related info */
zsendcmd(buf, blen)
char *buf;
{
	register c;
	long cmdnum;

	cmdnum = getpid();
	errors = 0;
	for (;;) {
		stohdr(cmdnum);
		Txhdr[ZF0] = Cmdack1;
		zsbhdr(ZCOMMAND, Txhdr);
		zsdata(buf, blen, ZCRCW);
listen:
		Rxtimeout = 100;		/* Ten second wait for resp. */
		c = zgethdr(Rxhdr, 1);

		switch (c) {
		case ZRINIT:
			goto listen;	/* CAF 8-21-87 */
		case ERROR:
		case TIMEOUT:
			if (++errors > Cmdtries)
				return ERROR;
			continue;
		case ZCAN:
		case ZABORT:
		case ZFIN:
		case ZSKIP:
		case ZRPOS:
			return ERROR;
		default:
			if (++errors > 20)
				return ERROR;
			continue;
		case ZCOMPL:
			Exitcode = Rxpos;
			saybibi();
			return OK;
		case ZRQINIT:
			vfile("******** RZ *******");
			system("rz");
			vfile("******** SZ *******");
			goto listen;
		}
	}
}

/*
 * If called as lsb use YMODEM protocol
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
	if ((s[0]=='s' && s[1]=='b')
		|| (s[0]=='l' && s[1]=='s' && s[2]=='b')) {
		Nozmodem = TRUE; blklen=1024;
	}
	if ((s[0]=='s' && s[1]=='x')
		|| (s[0]=='l' && s[1]=='s' && s[2]=='x')) {
		Modem2 = TRUE;
	}
}

countem(argc, argv)
register char **argv;
{
	register c;
	struct stat f;

	for (Totalleft = 0, Filesleft = 0; --argc >=0; ++argv) {
		f.st_size = -1;
		if (Verbose>2) {
			fprintf(stderr, "\nCountem: %03d %s ", argc, *argv);
			fflush(stderr);
		}
		if (access(*argv, 04) >= 0 && stat(*argv, &f) >= 0) {
			c = f.st_mode & S_IFMT;
			if (c != S_IFDIR && c != S_IFBLK) {
				++Filesleft;  Totalleft += f.st_size;
			}
		}
		if (Verbose>2)
			fprintf(stderr, " %ld", (long) f.st_size);
	}
	if (Verbose>2)
		fprintf(stderr, "\ncountem: Total %d %ld\n",
		  Filesleft, Totalleft);
#ifdef NEW_ERROR
	calc_blklen(Totalleft);
#endif
}

chartest(m)
{
	register n;

	mode(m);
	printf("\r\n\nCharacter Transparency Test Mode %d\r\n", m);
	printf("If Pro-YAM/ZCOMM is not displaying ^M hit ALT-V NOW.\r\n");
	printf("Hit Enter.\021");  fflush(stdout);
	readline(500);

	for (n = 0; n < 256; ++n) {
		if (!(n%8))
			printf("\r\n");
		printf("%02x ", n);  fflush(stdout);
		sendline(n);	flushmo();
		printf("  ");  fflush(stdout);
		if (n == 127) {
			printf("Hit Enter.\021");  fflush(stdout);
			readline(500);
			printf("\r\n");  fflush(stdout);
		}
	}
	printf("\021\r\nEnter Characters, echo is in hex.\r\n");
	printf("Hit SPACE or pause 40 seconds for exit.\r\n");

	while (n != TIMEOUT && n != ' ') {
		n = readline(400);
		printf("%02x\r\n", n);
		fflush(stdout);
	}
	printf("\r\nMode %d character transparency test ends.\r\n", m);
	fflush(stdout);
}

/* End of lsz.c */


