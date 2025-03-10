
/* General Numerical routines support functions, */
/* and common Argyll support functions. */
/* (Perhaps these should be moved out of numlib at some stange ?) */

/*
 * Copyright 1997 - 2010 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <ctype.h>
#if defined (NT)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#ifdef UNIX
#include <unistd.h>
#include <sys/param.h>
#include <sys/utsname.h>
#include <pthread.h>
#endif
#ifndef SALONEINSTLIB
#include "aconfig.h"
#else
#include "sa_config.h"
#endif

#define NUMSUP_C
#include "numsup.h"

/* 
 * TODO: Should probably break all the non-numlib stuff out into
 *       a separate library, so that it can be ommitted.
 *       Or enhance it so that numerical callers of error()
 *       can get a callback on out of memory etc. ???
 *
 */

/* Globals */

char *exe_path = "\000";			/* Directory executable resides in ('/' dir separator) */
//char *error_program = "Unknown";	/* Name to report as responsible for an error */

static int g_log_init = 0;	/* Initialised ? */
static int g_deb_init = 0;	/* Debug output Initialised ? */
extern a1log default_log;
extern a1log *g_log;

/* Should Vector/Matrix Support functions return NULL on error, */
/* or call error() ? */
int ret_null_on_malloc_fail = 0;	/* Call error() */

/******************************************************************/
/* Executable path routine. Sets default error_program too. */
/******************************************************************/

/* Pass in argv[0] from main() */
/* Sets the error_program name too */
void set_exe_path(char *argv0) {
	int i;

	g_log->tag = argv0;
	i = strlen(argv0);
	if ((exe_path = malloc(i + 5)) == NULL) {
		a1loge(g_log, 1, "set_exe_path: malloc %d bytes failed\n",i+5);
		return;
	}
	strcpy(exe_path, argv0);

#ifdef NT	/* CMD.EXE doesn't give us the full path in argv[0] :-( */
			/* so we need to fix this */
	{
		char *tpath = NULL;
		int pl;

		/* Retry until we don't truncate the returned path */
		for (pl = 100; ; pl *= 2) {
			if (tpath != NULL)
				free(tpath);
			if ((tpath = malloc(pl)) == NULL) {
				a1loge(g_log, 1, "set_exe_path: malloc %d bytes failed\n",pl);
				exe_path[0] = '\000';
			return;
			}
			if ((i = GetModuleFileName(NULL, tpath, pl)) == 0) {
				a1loge(g_log, 1, "set_exe_path: GetModuleFileName '%s' failed with%d\n",
				                                                tpath,GetLastError());
				exe_path[0] = '\000';
				return;
			}
			if (i < pl)		/* There was enough space */
				break;
		}
		free(exe_path);
		exe_path = tpath;

		/* Convert from MSWindows to UNIX file separator convention */
		for (i = 0; ;i++) {
			if (exe_path[i] == '\000')
				break;
			if (exe_path[i] == '\\')
				exe_path[i] = '/';
		}
	}
#else		/* Neither does UNIX */

	/* Should use readlink("/proc/self/exe", dest, PATH_MAX) on Linux... */
	/* Should use _NSGetExecutablePath() on OS X */

	if (*exe_path != '/') {			/* Not already absolute */
		char *p, *cp;
		if (strchr(exe_path, '/') != 0) {	/* relative path */
			cp = ".:";				/* Fake a relative PATH */
		} else  {
			cp = getenv("PATH");
		}
		if (cp != NULL) {
			int found = 0;
			while((p = strchr(cp,':')) != NULL
			 || *cp != '\000') {
				char b1[PATH_MAX], b2[PATH_MAX];
 				int ll;
				if (p == NULL)
					ll = strlen(cp);
				else
					ll = p - cp;
				if ((ll + 1 + strlen(exe_path) + 1) > PATH_MAX) {
					a1loge(g_log, 1, "set_exe_path: Search path exceeds PATH_MAX\n");
					exe_path[0] = '\000';
					return;
				}
				strncpy(b1, cp, ll);		/* Element of path to search */
				b1[ll] = '\000';
				strcat(b1, "/");
				strcat(b1, exe_path);		/* Construct path */
				if (realpath(b1, b2)) {
					if (access(b2, 0) == 0) {	/* See if exe exits */
						found = 1;
						free(exe_path);
						if ((exe_path = malloc(strlen(b2)+1)) == NULL) {
							a1loge(g_log, 1, "set_exe_path: malloc %d bytes failed\n",strlen(b2)+1);
							exe_path[0] = '\000';
							return;
						}
						strcpy(exe_path, b2);
						break;
					}
				}
				if (p == NULL)
					break;
				cp = p + 1;
			}
			if (found == 0)
				exe_path[0] = '\000';
		}
	}
#endif
	/* strip the executable path to the base */
	for (i = strlen(exe_path)-1; i >= 0; i--) {
		if (exe_path[i] == '/') {
			char *tpath;
			if ((tpath = malloc(strlen(exe_path + i))) == NULL) {
				a1loge(g_log, 1, "set_exe_path: malloc %d bytes failed\n",strlen(exe_path + i));
				exe_path[0] = '\000';
				return;
			}
			strcpy(tpath, exe_path + i + 1);
			g_log->tag = tpath;				/* Set g_log->tag to base name */
			exe_path[i+1] = '\000';				/* (The malloc never gets free'd) */
			break;
		}
	}
	/* strip off any .exe from the g_log->tag to be more readable */
	i = strlen(g_log->tag);
	if (i >= 4
	 && g_log->tag[i-4] == '.'
	 && (g_log->tag[i-3] == 'e' || g_log->tag[i-3] == 'E')
	 && (g_log->tag[i-2] == 'x' || g_log->tag[i-2] == 'X')
	 && (g_log->tag[i-1] == 'e' || g_log->tag[i-1] == 'E'))
		g_log->tag[i-4] = '\000';

//	a1logd(g_log, 1, "exe_path = '%s'\n",exe_path);
//	a1logd(g_log, 1, "g_log->tag = '%s'\n",g_log->tag);
}

/******************************************************************/
/* Check "ARGYLL_NOT_INTERACTIVE" environment variable.           */
/******************************************************************/

/* Check if the "ARGYLL_NOT_INTERACTIVE" environment variable is */
/* set, and set cr_char to '\n' if it is. */
/* This should be called _before_ any stdout is used */

int not_interactive = 0;	/* 1 = not_interactive */
#ifdef NT
DWORD stdin_type = FILE_TYPE_CHAR;
#endif
char cr_char = '\r';		/* For update on one line messages */
char *fl_end = "";			/* For strings with no \n and a do_fflush() */

void check_if_not_interactive() {
	char *ev;

#ifdef NEVER
# ifdef NT
	// ?? Should we ??
	// - but shouldn't the UTF-8 code page trigger this anyway ??
	_setmode(_fileno(stdin), 0x00040000); // _O_U8TEXT
	_setmode(_fileno(stdout), 0x00040000); // _O_U8TEXT
	_setmode(_fileno(stdserr), 0x00040000); // _O_U8TEXT
# endif
#endif

	fl_end = "";

	if ((ev = getenv("ARGYLL_NOT_INTERACTIVE")) != NULL) {
#ifdef NT
		HANDLE stdinh;
#endif

		not_interactive = 1;
		cr_char = '\n';

#ifdef NT
		stdin_type = FILE_TYPE_CHAR;

		/* Set no buffering so that messages arrive in the right sequence */
		setvbuf(stdout, NULL, _IONBF, 1024);

		/* Since we can't force the pipe to be in OVERLAPPED mode, we have */
		/* to use NOWAIT mode. */
		if ((stdinh = GetStdHandle(STD_INPUT_HANDLE)) != INVALID_HANDLE_VALUE) {
			stdin_type = GetFileType(stdinh);
			if (stdin_type == FILE_TYPE_PIPE) {
				DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
				SetNamedPipeHandleState(stdinh, &mode, NULL, NULL);
			}
		}
#else
		/* Set line buffering so that messages arrive in the right sequence */
		setvbuf(stdout, NULL, _IOLBF, 1024);
#endif

	} else {
#ifdef NT
		stdin_type = FILE_TYPE_CHAR;
#endif
		not_interactive = 0;
		cr_char = '\r';
	}
}

/* Flush out prompts */
void do_fflush() {
	fflush(stdout);
}

/******************************************************************/
/* Default verbose/debug/error loger                              */
/* It's values can be overridden to redirect these messages.      */
/******************************************************************/

static void va_loge(a1log *p, char *fmt, ...);

#ifdef NT

/* Get a string describing the MWin operating system */

typedef struct {
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    WCHAR  szCSDVersion[128];
    WORD   wServicePackMajor;
    WORD   wServicePackMinor;
    WORD   wSuiteMask;
    BYTE  wProductType;
    BYTE  wReserved;
} osversioninfoexw;

#ifndef VER_NT_DOMAIN_CONTROLLER
# define VER_NT_DOMAIN_CONTROLLER 0x0000002
# define VER_NT_SERVER 0x0000003
# define VER_NT_WORKSTATION 0x0000001 
#endif

static char *get_sys_info() {
	static char sysinfo[100] = { "Unknown" };
	LONG (WINAPI *pfnRtlGetVersion)(osversioninfoexw*);

	*(FARPROC *)&pfnRtlGetVersion
	 = GetProcAddress(GetModuleHandle("ntdll.dll"), "RtlGetVersion");
	if (pfnRtlGetVersion != NULL) {
	    osversioninfoexw ver = { 0 };
	    ver.dwOSVersionInfoSize = sizeof(ver);

	    if (pfnRtlGetVersion(&ver) == 0) {
			if (ver.dwMajorVersion > 6 || (ver.dwMajorVersion == 6 && ver.dwMinorVersion > 3)) {
				if (ver.wProductType == VER_NT_WORKSTATION)
					sprintf(sysinfo,"Windows V%d.%d SP %d",
						ver.dwMajorVersion,ver.dwMinorVersion,
						ver.wServicePackMajor);
				else
					sprintf(sysinfo,"Windows Server 2016 V%d.%d SP %d",
						ver.dwMajorVersion,ver.dwMinorVersion,
						ver.wServicePackMajor);
				
			} else if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 3) {
				if (ver.wProductType == VER_NT_WORKSTATION)
					sprintf(sysinfo,"Windows V8.1 SP %d",
						ver.wServicePackMajor);
				else
					sprintf(sysinfo,"Windows Server 2012 R2 SP %d",
						ver.wServicePackMajor);

			} else if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 2) {
				if (ver.wProductType == VER_NT_WORKSTATION)
					sprintf(sysinfo,"Windows V8 SP %d",
						ver.wServicePackMajor);
				else
					sprintf(sysinfo,"Windows Server SP %d",
						ver.wServicePackMajor);

			} else if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 1) {
				if (ver.wProductType == VER_NT_WORKSTATION)
					sprintf(sysinfo,"Windows V7 SP %d",
						ver.wServicePackMajor);
				else
					sprintf(sysinfo,"Windows Server 2008 R2 SP %d",
						ver.wServicePackMajor);

			} else if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 0) {
				if (ver.wProductType == VER_NT_WORKSTATION)
					sprintf(sysinfo,"Windows Vista SP %d",
						ver.wServicePackMajor);
				else
					sprintf(sysinfo,"Windows Server 2008 SP %d",
						ver.wServicePackMajor);

			} else if (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 2) {
				// Actually could be Server 2003, Home Server, Server 2003 R2
				sprintf(sysinfo,"Windows XP Pro64 SP %d",
					ver.wServicePackMajor);

			} else if (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 1) {
				sprintf(sysinfo,"Windows XP SP %d",
						ver.wServicePackMajor);

			} else if (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 0) {
				sprintf(sysinfo,"Windows XP SP %d",
						ver.wServicePackMajor);

			} else {
				sprintf(sysinfo,"Windows Maj %d Min %d SP %d",
					ver.dwMajorVersion,ver.dwMinorVersion,
					ver.wServicePackMajor);
			}
		}
	}
	return sysinfo;
}


# define A1LOG_LOCK(log, deb)								\
	if (g_log_init == 0) {									\
	    InitializeCriticalSection(&log->lock);				\
		EnterCriticalSection(&log->lock);					\
		g_log_init = 1;										\
	} else {												\
		EnterCriticalSection(&log->lock);					\
	}														\
	if (deb && !g_deb_init) {								\
		va_loge(log, "\n#######################################################################\n");	\
		va_loge(log, "Argyll 'V%s' Build '%s' System '%s'\n",ARGYLL_VERSION_STR,ARGYLL_BUILD_STR, get_sys_info());	\
		g_deb_init = 1;										\
	}
# define A1LOG_UNLOCK(log) LeaveCriticalSection(&log->lock)
#endif

#ifdef UNIX

static char *get_sys_info() {
	static char sysinfo[500] = { "Unknown" };
	struct utsname ver;

	if (uname(&ver) == 0)
#if defined(__APPLE__)
		sprintf(sysinfo,"%s, %s, %s, %s (OS X %s)",ver.sysname, ver.version, ver.release, ver.machine, osx_get_version_str());
#else
		sprintf(sysinfo,"%s, %s, %s, %s",ver.sysname, ver.version, ver.release, ver.machine);
#endif
	return sysinfo;
}

# define A1LOG_LOCK(log, deb)								\
	if (g_log_init == 0) {									\
	    pthread_mutex_init(&log->lock, NULL);				\
		pthread_mutex_lock(&log->lock);						\
		g_log_init = 1;										\
	} else {												\
		pthread_mutex_lock(&log->lock);						\
	}														\
	if (deb && !g_deb_init) {								\
		va_loge(log, "\n#######################################################################\n");	\
		va_loge(log, "Argyll 'V%s' Build '%s' System '%s'\n",ARGYLL_VERSION_STR,ARGYLL_BUILD_STR, get_sys_info());	\
		g_deb_init = 1;										\
	}
# define A1LOG_UNLOCK(log) pthread_mutex_unlock(&log->lock)
#endif



/* Default verbose logging function - print to stdtout */
static void a1_default_v_log(void *cntx, a1log *p, char *fmt, va_list args) {
	vfprintf(stdout, fmt, args);
	fflush(stdout);
}

/* Default debug & error logging function - print to stderr */
static void a1_default_de_log(void *cntx, a1log *p, char *fmt, va_list args) {
	vfprintf(stderr, fmt, args);
	fflush(stderr);
}

#define a1_default_d_log a1_default_de_log
#define a1_default_e_log a1_default_de_log


/* Call log->loge() with variags */
static void va_loge(a1log *p, char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	p->loge(p->cntx, p, fmt, args);
	va_end(args);
}

/* Global log */
a1log default_log = {
	1,			/* Refcount of 1 because this is not allocated or free'd */
	"argyll",	/* Default tag */
	0,			/* Vebose off */
	0,			/* Debug off */
	NULL,		/* Context */
	&a1_default_v_log,	/* Default verbose to stdout */
	&a1_default_d_log,	/* Default debug to stderr */
	&a1_default_e_log,	/* Default error to stderr */
	0,					/* error code 0 */			
	{ '\000' }			/* No error message */
};
a1log *g_log = &default_log;

/* If log NULL, allocate a new log and return it, */
/* otherwise increment reference count and return existing log, */
/* exit() if malloc fails. */
a1log *new_a1log(
	a1log *log,						/* Existing log to reference, NULL if none */
	int verb,						/* Verbose level to set */
	int debug,						/* Debug level to set */
	void *cntx,						/* Function context value */
		/* Vebose log function to call - stdout if NULL */
	void (*logv)(void *cntx, a1log *p, char *fmt, va_list args),
		/* Debug log function to call - stderr if NULL */
	void (*logd)(void *cntx, a1log *p, char *fmt, va_list args),
		/* Warning/error Log function to call - stderr if NULL */
	void (*loge)(void *cntx, a1log *p, char *fmt, va_list args)
) {
	if (log != NULL) {
		log->refc++;
		return log;
	}
	if ((log = (a1log *)calloc(sizeof(a1log), 1)) == NULL) {
		a1loge(g_log, 1, "new_a1log: malloc of a1log failed, calling exit(1)\n");
		exit(1);
	}
	log->refc = 1;
	log->verb = verb;
	log->debug = debug;

	log->cntx = cntx;
	if (logv != NULL)
		log->logv = logv;
	else
		log->logv = a1_default_v_log;

	if (logd != NULL)
		log->logd = logd;
	else
		log->logd = a1_default_d_log;

	if (loge != NULL)
		log->loge = loge;
	else
		log->loge = a1_default_e_log;

	log->errc = 0;
	log->errm[0] = '\000';

	return log;
}

/* Same as above but set default functions */
a1log *new_a1log_d(a1log *log) {
	return new_a1log(log, 0, 0, NULL, NULL, NULL, NULL);
}

/* Decrement reference count and free log. */
/* Returns NULL */
a1log *del_a1log(a1log *log) {
	if (log != NULL) {
		if (--log->refc <= 0) {
#ifdef NT
			DeleteCriticalSection(&log->lock);
#endif
#ifdef UNIX
			pthread_mutex_destroy(&log->lock);
#endif
			free(log);
		}
	}
	return NULL;
}

/* Set the debug level. */
void a1log_debug(a1log *log, int level) {
	if (log != NULL) {
		log->debug = level;
	}
}

/* Set the vebosity level. */
void a1log_verb(a1log *log, int level) {
	if (log != NULL) {
		log->verb = level;
	}
}

/* Set the tag. Note that the tage string is NOT copied, just referenced */
void a1log_tag(a1log *log, char *tag) {
	if (log != NULL) {
		log->tag = tag;
	}
}

/* Log a verbose message if level >= verb */
void a1logv(a1log *log, int level, char *fmt, ...) {

	if (log != NULL) {
		if (log->verb >= level) {
			va_list args;

			A1LOG_LOCK(log, 0);
			va_start(args, fmt);
			log->logv(log->cntx, log, fmt, args);
			va_end(args);
			A1LOG_UNLOCK(log);
		}
	}
}

/* Log a debug message if level >= debug */
void a1logd(a1log *log, int level, char *fmt, ...) {
	if (log != NULL) {
		if (log->debug >= level) {
			va_list args;
	
			A1LOG_LOCK(log, 1);
			va_start(args, fmt);
			log->logd(log->cntx, log, fmt, args);
			va_end(args);
			A1LOG_UNLOCK(log);
		}
	}
}

/* log a warning message to the verbose, debug and error output, */
void a1logw(a1log *log, char *fmt, ...) {
	if (log != NULL) {
		va_list args;
	
		/* log to all the outputs, but only log once */
		A1LOG_LOCK(log, 0);
		va_start(args, fmt);
		log->loge(log->cntx, log, fmt, args);
		va_end(args);
		A1LOG_UNLOCK(log);
		if (log->logd != log->loge) {
			A1LOG_LOCK(log, 1);
			va_start(args, fmt);
			log->logd(log->cntx, log, fmt, args);
			va_end(args);
			A1LOG_UNLOCK(log);
		}
		if (log->logv != log->loge && log->logv != log->logd) {
			A1LOG_LOCK(log, 0);
			va_start(args, fmt);
			log->logv(log->cntx, log, fmt, args);
			va_end(args);
			A1LOG_UNLOCK(log);
		}
	}
}

/* log an error message to the verbose, debug and error output, */
/* and latch the error if it is the first. */
/* ecode = system, icoms or instrument error */
void a1loge(a1log *log, int ecode, char *fmt, ...) {
	if (log != NULL) {
		va_list args;
	
		if (log->errc == 0) {
			A1LOG_LOCK(log, 0);
			log->errc = ecode;
			va_start(args, fmt);
			vsnprintf(log->errm, A1_LOG_BUFSIZE, fmt, args);
			va_end(args);
			A1LOG_UNLOCK(log);
		}
		va_start(args, fmt);
		/* log to all the outputs, but only log once */
		A1LOG_LOCK(log, 0);
		va_start(args, fmt);
		log->loge(log->cntx, log, fmt, args);
		va_end(args);
		A1LOG_UNLOCK(log);
		if (log->logd != log->loge) {
			A1LOG_LOCK(log, 1);
			va_start(args, fmt);
			log->logd(log->cntx, log, fmt, args);
			va_end(args);
			A1LOG_UNLOCK(log);
		}
		if (log->logv != log->loge && log->logv != log->logd) {
			A1LOG_LOCK(log, 0);
			va_start(args, fmt);
			log->logv(log->cntx, log, fmt, args);
			va_end(args);
			A1LOG_UNLOCK(log);
		}
	}
}

/* Unlatch an error message. */
/* This just resets errc and errm */
void a1logue(a1log *log) {
	if (log != NULL) {
		log->errc = 0;
		log->errm[0] = '\000';
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Print bytes as hex to FILE */
/* base is the base of the displayed offset */
void dump_bytes(FILE *fp, char *pfx, unsigned char *buf, int base, int len) {
	int i, j, ii;
	char oline[200] = { '\000' }, *bp = oline;
	if (pfx == NULL)
		pfx = "";
	for (i = j = 0; i < len; i++) {
		if ((i % 16) == 0)
			bp += sprintf(bp,"%s%04x:",pfx,base+i);
		bp += sprintf(bp," %02x",buf[i]);
		if ((i+1) >= len || ((i+1) % 16) == 0) {
			for (ii = i; ((ii+1) % 16) != 0; ii++)
				bp += sprintf(bp,"   ");
			bp += sprintf(bp,"  ");
			for (; j <= i; j++) {
				if (!(buf[j] & 0x80) && isprint(buf[j]))
					bp += sprintf(bp,"%c",buf[j]);
				else
					bp += sprintf(bp,".");
			}
			bp += sprintf(bp,"\n");
			fprintf(fp,"%s",oline);
			bp = oline;
		}
	}
}


/* Print bytes as hex to debug log */
/* base is the base of the displayed offset */
void adump_bytes(a1log *log, char *pfx, unsigned char *buf, int base, int len) {
	int i, j, ii;
	char oline[200] = { '\000' }, *bp = oline;
	if (pfx == NULL)
		pfx = "";
	for (i = j = 0; i < len; i++) {
		if ((i % 16) == 0)
			bp += sprintf(bp,"%s%04x:",pfx,base+i);
		bp += sprintf(bp," %02x",buf[i]);
		if ((i+1) >= len || ((i+1) % 16) == 0) {
			for (ii = i; ((ii+1) % 16) != 0; ii++)
				bp += sprintf(bp,"   ");
			bp += sprintf(bp,"  ");
			for (; j <= i; j++) {
				if (!(buf[j] & 0x80) && isprint(buf[j]))
					bp += sprintf(bp,"%c",buf[j]);
				else
					bp += sprintf(bp,".");
			}
			bp += sprintf(bp,"\n");
			a1logd(log,0,"%s",oline);
			bp = oline;
		}
	}
}

/******************************************************************/
/* Default verbose/warning/error output routines                  */
/* These fall through to, and can be re-director using the        */
/* above log class.                                               */
/******************************************************************/

/* Some utilities to allow us to format output to log functions */
/* (Caller aquires lock) */
static void g_logv(char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	g_log->logv(g_log->cntx, g_log, fmt, args);
	va_end(args);
}

static void g_loge(char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	g_log->loge(g_log->cntx, g_log, fmt, args);
	va_end(args);
}

void
verbose(int level, char *fmt, ...) {
	if (g_log->verb >= level) {
		va_list args;

		A1LOG_LOCK(g_log, 0);
		g_logv("%s: ",g_log->tag);
		va_start(args, fmt);
		g_log->logv(g_log->cntx, g_log, fmt, args);
		va_end(args);
		g_logv("\n");
		A1LOG_UNLOCK(g_log);
	}
}

void
warning(char *fmt, ...) {
	va_list args;

	A1LOG_LOCK(g_log, 0);
	g_loge("%s: Warning - ",g_log->tag);
	va_start(args, fmt);
	g_log->loge(g_log->cntx, g_log, fmt, args);
	va_end(args);
	g_loge("\n");
	A1LOG_UNLOCK(g_log);
}

ATTRIBUTE_NORETURN void
error(char *fmt, ...) {
	va_list args;

	A1LOG_LOCK(g_log, 0);
	g_loge("%s: Error - ",g_log->tag);
	va_start(args, fmt);
	g_log->loge(g_log->cntx, g_log, fmt, args);
	va_end(args);
	g_loge("\n");
	A1LOG_UNLOCK(g_log);

	exit(1);
}


/******************************************************************/
/* Suplimental allcation functions */
/******************************************************************/

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)(-1))
#endif

/* a * b */
static size_t ssat_mul(size_t a, size_t b) {
	size_t c;

	if (a == 0 || b == 0)
		return 0;

	if (a > (SIZE_MAX/b))
		return SIZE_MAX;
	else
		return a * b;
}

/* reallocate and clear new allocation */
void *recalloc(		/* Return new address */
void *ptr,					/* Current address */
size_t cnum,				/* Current number and unit size */
size_t csize,
size_t nnum,				/* New number and unit size */
size_t nsize
) {
	int ind = 0;
	size_t ctot, ntot;

	if (ptr == NULL)
		return calloc(nnum, nsize); 

	if ((ntot = ssat_mul(nnum, nsize)) == SIZE_MAX)
		return NULL;			/* Overflow */

	if ((ctot = ssat_mul(cnum, csize)) == SIZE_MAX)
		return NULL;			/* Overflow */

	ptr = realloc(ptr, ntot);

	if (ptr != NULL && ntot > ctot)
		memset((char *)ptr + ctot, 0, ntot - ctot);			/* Clear the new region */

	return ptr;
}

/******************************************************************/
/* OS X support functions                                         */
/******************************************************************/

#if defined(__APPLE__)

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1050
# include <objc/runtime.h>
# include <objc/message.h>
#else
# include <objc/objc-runtime.h>
#endif
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
# include <objc/objc-auto.h>
#endif

#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFStream.h>
#include <CoreFoundation/CFPropertyList.h>

/* OS X version info. Apple has not maintained any consistent function to do this ! */
/* (This code is from "Mecki" via stackoverflow) */

static bool osx_versionOK = false;
static bool osx_onceToken = false;
static unsigned osx_versions[3] = { 0, 0, 0 };
static char osx_versions_str[40] = { "0.0.0" };

#if __MAC_OS_X_VERSION_MAX_ALLOWED < 1050

#include <Carbon/Carbon.h>

void initMacOSVersion() {
    SInt32 vers;
    SInt32 maj, min, bug;

	osx_onceToken = true;

    Gestalt(gestaltSystemVersion, &vers);
	maj = vers/0x1000 * 10 + (vers/0x100 % 0x10) ;
	min = (vers/0x10) % 0x10;
	bug = (vers) % 0x10;

	osx_versions[0] = maj;
	osx_versions[1] = min;
	osx_versions[2] = bug;

	sprintf(osx_versions_str, "%d.%d.%d", maj, min, bug);

	osx_versionOK = true;
}

#else

#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFStream.h>
#include <CoreFoundation/CFPropertyList.h>

void initMacOSVersion() {
	osx_onceToken = true;

	// `Gestalt()` actually gets the system version from this file.
	// Even `if (@available(macOS 10.x, *))` gets the version from there.
	CFURLRef url = CFURLCreateWithFileSystemPath(
		NULL, CFSTR("/System/Library/CoreServices/SystemVersion.plist"),
		kCFURLPOSIXPathStyle, false);
	if (!url) return;

	CFReadStreamRef readStr = CFReadStreamCreateWithFile(NULL, url);
	CFRelease(url);
	if (!readStr) return;

	if (!CFReadStreamOpen(readStr)) {
		CFRelease(readStr);
		return;
	}

	CFErrorRef outError = NULL;
	CFPropertyListRef propList = CFPropertyListCreateWithStream(
		NULL, readStr, 0, kCFPropertyListImmutable, NULL, &outError);
	CFRelease(readStr);
	if (!propList) {
		CFShow(outError);
		CFRelease(outError);
		return;
	}

	if (CFGetTypeID(propList) != CFDictionaryGetTypeID()) {
		CFRelease(propList);
		return;
	}

	CFDictionaryRef dict = propList;
	CFTypeRef ver = CFDictionaryGetValue(dict, CFSTR("ProductVersion"));
	if (ver) CFRetain(ver);
	CFRelease(dict);
	if (!ver) return;

	if (CFGetTypeID(ver) != CFStringGetTypeID()) {
		CFRelease(ver);
		return;
	}

	CFStringRef verStr = ver;
	// `1 +` for the terminating NUL (\0) character
	CFIndex size = 1 + CFStringGetMaximumSizeForEncoding(
		CFStringGetLength(verStr), kCFStringEncodingASCII);
	// `calloc` initializes the memory with all zero (all \0)
	char * cstr = calloc(1, size);
	if (!cstr) {
		CFRelease(verStr);
		return;
	}

	CFStringGetBytes(ver, CFRangeMake(0, CFStringGetLength(verStr)),
		kCFStringEncodingASCII, '?', false, (UInt8 *)cstr, size, NULL);
	CFRelease(verStr);

	int scans = sscanf(cstr, "%u.%u.%u",
		&osx_versions[0], &osx_versions[1], &osx_versions[2]);
	free(cstr);

	// There may only be two values, but only one is definitely wrong.
	// As `version` is `static`, its zero initialized.
	osx_versionOK = (scans >= 2);

	sprintf(osx_versions_str, "%d.%d.%d", osx_versions[0], osx_versions[1], osx_versions[2]);
}

#endif

/* Get the OS X version number. */
/* Return maj + min/100.0 + bugfix/10000.0 */
/* (Returns 0.0 if unable to get version */
double osx_get_version() {
	double rv = 0.0;

	if (!osx_onceToken)
		initMacOSVersion();

	if (osx_versionOK)
		rv = osx_versions[0] + osx_versions[1]/100.0 + osx_versions[2]/10000.0;

	return rv;
}

/* Get text OS X verion number, i.e. "10.3.1" */
char *osx_get_version_str() {
	if (!osx_onceToken)
		initMacOSVersion();

	return osx_versions_str;
}

/*
	OS X 10.9+ App Nap problems bug:

	<http://stackoverflow.com/questions/22784886/what-can-make-nanosleep-drift-with-exactly-10-sec-on-mac-os-x-10-9>

	NSProcessInfo variables:

	<https://developer.apple.com/library/prerelease/ios/documentation/Cocoa/Reference/Foundation/Classes/NSProcessInfo_Class/#//apple_ref/c/tdef/NSActivityOptions>

	 typedef enum : uint64_t { NSActivityIdleDisplaySleepDisabled = (1ULL << 40),
		NSActivityIdleSystemSleepDisabled = (1ULL << 20),
		NSActivitySuddenTerminationDisabled = (1ULL << 14),
		NSActivityAutomaticTerminationDisabled = (1ULL << 15),
		NSActivityUserInitiated = (0x00FFFFFFULL | NSActivityIdleSystemSleepDisabled ),
		NSActivityUserInitiatedAllowingIdleSystemSleep =
		           (NSActivityUserInitiated & ~NSActivityIdleSystemSleepDisabled ),
		NSActivityBackground = 0x000000FFULL,
		NSActivityLatencyCritical = 0xFF00000000ULL,
	} NSActivityOptions; 

	See <http://stackoverflow.com/questions/19847293/disable-app-nap-in-macos-10-9-mavericks-application>:

	@property (strong) id activity;

	if ([[NSProcessInfo processInfo] respondsToSelector:@selector(beginActivityWithOptions:reason:)]) {
    self.activity = [[NSProcessInfo processInfo] beginActivityWithOptions:0x00FFFFFF reason:@"receiving OSC messages"];
}

	<http://stackoverflow.com/questions/19671197/disabling-app-nap-with-beginactivitywithoptions>

	NSProcessInfo = interface(NSObject)['{B96935F6-3809-4A49-AD4F-CBBAB0F2C961}']
	function beginActivityWithOptions(options: NSActivityOptions; reason: NSString): NSObject; cdecl;

	<http://stackoverflow.com/questions/22164571/weird-behaviour-of-dispatch-after>

	New (10.15) objc_msgSend prototype:
	<https://www.mikeash.com/pyblog/objc_msgsends-new-prototype.html>

	Could get away with casting to old prototype on Intel ABI:

#define OBJC_MSGSEND ((id (*)(id, SEL, ...))objc_msgSend)

	but this will fail on ARM64 ABI, so we explicitly cast it.
*/

static int osx_userinitiated_cnt = 0;
static id osx_userinitiated_activity = nil;

/* Tell App Nap that this is user initiated */
void osx_userinitiated_start() {
	Class pic;		/* Process info class */
	SEL pis;		/* Process info selector */
	SEL bawo;		/* Begin Activity With Options selector */
	id pi;			/* Process info */
	id str;

	if (osx_userinitiated_cnt++ != 0)
		return;

	a1logd(g_log, 7, "OS X - User Initiated Activity start\n");
	
	/* We have to be conservative to avoid triggering an exception when run on older OS X, */
	/* since beginActivityWithOptions is only available in >= 10.9 */
	if ((pic = (Class)objc_getClass("NSProcessInfo")) == nil) {
		return;
	}

	if (class_getClassMethod(pic, (pis = sel_getUid("processInfo"))) == NULL) { 
		return;
	}

	if (class_getInstanceMethod(pic, (bawo = sel_getUid("beginActivityWithOptions:reason:"))) == NULL) {
		a1logd(g_log, 7, "OS X - beginActivityWithOptions not supported\n");
		return;
	}

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101500
	/* Get the process instance */
	if ((pi = ((id (*)(id, SEL))objc_msgSend)((id)pic, pis)) == nil) {
		return;
	}

	/* Create a reason string */
	str = ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSString"), sel_getUid("alloc"));
	str = ((id (*)(id, SEL, char*))objc_msgSend)(str, sel_getUid("initWithUTF8String:"), "ArgyllCMS");
			
	/* Start activity that tells App Nap to mind its own business. */
	/* NSActivityUserInitiatedAllowingIdleSystemSleep */
	osx_userinitiated_activity = ((id (*)(id, SEL, uint64_t, id))objc_msgSend)(pi, bawo, 0x00FFFFFFULL, str);

#else
	/* Get the process instance */
	if ((pi = objc_msgSend((id)pic, pis)) == nil) {
		return;
	}

	/* Create a reason string */
	str = objc_msgSend((id)objc_getClass("NSString"), sel_getUid("alloc"));
	str = objc_msgSend(str, sel_getUid("initWithUTF8String:"), "ArgyllCMS");
			
	/* Start activity that tells App Nap to mind its own business. */
	/* NSActivityUserInitiatedAllowingIdleSystemSleep */
	osx_userinitiated_activity = objc_msgSend(pi, bawo, 0x00FFFFFFULL, str);
#endif
}

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101500
/* Done with user initiated */
void osx_userinitiated_end() {
	if (osx_userinitiated_cnt > 0) {
		osx_userinitiated_cnt--;
		if (osx_userinitiated_cnt == 0 && osx_userinitiated_activity != nil) {
			a1logd(g_log, 7, "OS X - User Initiated Activity end");
			((id (*)(id, SEL, id))objc_msgSend)(
			             ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSProcessInfo"),
			             sel_getUid("processInfo")), sel_getUid("endActivity:"),
			             osx_userinitiated_activity);
			osx_userinitiated_activity = nil;
		}
	}
}
#else
/* Done with user initiated */
void osx_userinitiated_end() {
	if (osx_userinitiated_cnt > 0) {
		osx_userinitiated_cnt--;
		if (osx_userinitiated_cnt == 0 && osx_userinitiated_activity != nil) {
			a1logd(g_log, 7, "OS X - User Initiated Activity end");
			objc_msgSend(
			             objc_msgSend((id)objc_getClass("NSProcessInfo"),
			             sel_getUid("processInfo")), sel_getUid("endActivity:"),
			             osx_userinitiated_activity);
			osx_userinitiated_activity = nil;
		}
	}
}
#endif

static int osx_latencycritical_cnt = 0;
static id osx_latencycritical_activity = nil;

/* Tell App Nap that this is latency critical */
void osx_latencycritical_start() {
	Class pic;		/* Process info class */
	SEL pis;		/* Process info selector */
	SEL bawo;		/* Begin Activity With Options selector */
	id pi;		/* Process info */
	id str;

	if (osx_latencycritical_cnt++ != 0)
		return;

	a1logd(g_log, 7, "OS X - Latency Critical Activity start\n");
	
	/* We have to be conservative to avoid triggering an exception when run on older OS X */
	if ((pic = (Class)objc_getClass("NSProcessInfo")) == nil) {
		return;
	}

	if (class_getClassMethod(pic, (pis = sel_getUid("processInfo"))) == NULL) { 
		return;
	}

	if (class_getInstanceMethod(pic, (bawo = sel_getUid("beginActivityWithOptions:reason:"))) == NULL) {
		a1logd(g_log, 7, "OS X - beginActivityWithOptions not supported\n");
		return;
	}

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101500
	/* Get the process instance */
	if ((pi = ((id (*)(id, SEL))objc_msgSend)((id)pic, pis)) == nil) {
		return;
	}

	/* Create a reason string */
	str = ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSString"), sel_getUid("alloc"));
	str = ((id (*)(id, SEL, char*))objc_msgSend)(str, sel_getUid("initWithUTF8String:"), "Measuring Color");
			
	/* Start activity that tells App Nap to mind its own business. */
	/* NSActivityUserInitiatedAllowingIdleSystemSleep | NSActivityLatencyCritical */
	osx_latencycritical_activity = ((id (*)(id, SEL, uint64_t, id))objc_msgSend)(pi, bawo, 0x00FFFFFFULL | 0xFF00000000ULL, str);
#else
	/* Get the process instance */
	if ((pi = objc_msgSend((id)pic, pis)) == nil) {
		return;
	}

	/* Create a reason string */
	str = objc_msgSend((id)objc_getClass("NSString"), sel_getUid("alloc"));
	str = objc_msgSend(str, sel_getUid("initWithUTF8String:"), "Measuring Color");
			
	/* Start activity that tells App Nap to mind its own business. */
	/* NSActivityUserInitiatedAllowingIdleSystemSleep | NSActivityLatencyCritical */
	osx_latencycritical_activity = objc_msgSend(pi, bawo, 0x00FFFFFFULL | 0xFF00000000ULL, str);
#endif
}

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101500
/* Done with latency critical */
void osx_latencycritical_end() {
	if (osx_latencycritical_cnt > 0) {
		osx_latencycritical_cnt--;
		if (osx_latencycritical_cnt == 0 && osx_latencycritical_activity != nil) {
			a1logd(g_log, 7, "OS X - Latency Critical Activity end");
			((id (*)(id, SEL, id))objc_msgSend)(
			             ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSProcessInfo"),
			             sel_getUid("processInfo")), sel_getUid("endActivity:"),
			             osx_latencycritical_activity);
			osx_latencycritical_activity = nil;
		}
	}
}
#else
/* Done with latency critical */
void osx_latencycritical_end() {
	if (osx_latencycritical_cnt > 0) {
		osx_latencycritical_cnt--;
		if (osx_latencycritical_cnt == 0 && osx_latencycritical_activity != nil) {
			a1logd(g_log, 7, "OS X - Latency Critical Activity end");
			objc_msgSend(
			             objc_msgSend((id)objc_getClass("NSProcessInfo"),
			             sel_getUid("processInfo")), sel_getUid("endActivity:"),
			             osx_latencycritical_activity);
			osx_latencycritical_activity = nil;
		}
	}
}
#endif

#endif	/* __APPLE__ */

/******************************************************************/
/* Numerical Recipes Vector/Matrix Support functions              */
/******************************************************************/
/* Note the z suffix versions return zero'd vectors/matricies */
/* Note the a suffix versions allocates on the stack using aloca() */

#ifdef NT
# define alloca _alloca
#endif

/* Double Vector malloc/free */
double *dvector(
int nl,		/* Lowest index */
int nh		/* Highest index */
)	{
	double *v;

	if ((v = (double *) malloc((nh-nl+1) * sizeof(double))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dvector()");
	}
	return v-nl;
}

double *dvectorz(
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	double *v;

	if ((v = (double *) calloc(nh-nl+1, sizeof(double))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dvector()");
	}
	return v-nl;
}

void free_dvector(
double *v,
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	if (v == NULL)
		return;

	free((char *) (v+nl));
}

/* Double Vector on stack */
double *dvectora(
int nl,		/* Lowest index */
int nh		/* Highest index */
)	{
	double *v;

	if ((v = (double *) alloca((nh-nl+1) * sizeof(double))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Alloca failure in dvector()");
	}
	return v-nl;
}

/* --------------------- */
/* 2D Double matrix malloc/free */
double **dmatrix(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	double **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	/* One extra pointer before colums to hold main allocation address */
	if ((m = (double **) malloc((rows + 1) * sizeof(double *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (double *) malloc(rows * cols * sizeof(double))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

double **dmatrixz(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	double **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (double **) malloc((rows + 1) * sizeof(double *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (double *) calloc(rows * cols, sizeof(double))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

void free_dmatrix(
double **m,
int nrl,
int nrh,
int ncl,
int nch
) {
	if (m == NULL)
		return;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	free((char *)(m[nrl-1]));
	free((char *)(m+nrl-1));
}

/* In case rows have been swapped, reset the pointers */
void dmatrix_reset(
double **m,
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int cols;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	cols = nch - ncl + 1;

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;
}

/* 2D Double matrix on stack */
double **dmatrixa(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	double **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	/* One extra pointer before colums to hold main allocation address */
	if ((m = (double **) alloca((rows + 1) * sizeof(double *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Alloca failure in dmatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (double *) alloca(rows * cols * sizeof(double))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Alloca failure in dmatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

/* --------------------- */
/* 2D diagonal half matrix vector malloc/free */
/* A half matrix must have equal rows and columns, */
/* and the column address must always be >= than the row. */
double **dhmatrix(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i, j;
	int rows, cols;
	double **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if (rows != cols) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("dhmatrix() given unequal rows and columns");
	}

	if ((m = (double **) malloc((rows + 1) * sizeof(double *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dhmatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (double *) malloc((rows * rows + rows)/2 * sizeof(double))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dhmatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1, j = 1; i <= nrh; i++, j++) /* Set subsequent row addresses */
		m[i] = m[i-1] + j;			/* Start with 1 entry and increment */

	return m;
}

double **dhmatrixz(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i, j;
	int rows, cols;
	double **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if (rows != cols) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("dhmatrix() given unequal rows and columns");
	}

	if ((m = (double **) malloc((rows + 1) * sizeof(double *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dhmatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (double *) calloc((rows * rows + rows)/2, sizeof(double))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dhmatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1, j = 1; i <= nrh; i++, j++) /* Set subsequent row addresses */
		m[i] = m[i-1] + j;			/* Start with 1 entry and increment */

	return m;
}

void free_dhmatrix(
double **m,
int nrl,
int nrh,
int ncl,
int nch
) {
	if (m == NULL)
		return;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	free((char *)(m[nrl-1]));
	free((char *)(m+nrl-1));
}

/* --------------------- */
/* matrix copy */
void copy_dmatrix(
double **dst,
double **src,
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i, j;
	for (j = nrl; j <= nrh; j++)
		for (i = ncl; i <= nch; i++)
			dst[j][i] = src[j][i];
}

/* Copy a matrix to a 3x3 standard C array */
void copy_dmatrix_to3x3(
double dst[3][3],
double **src,
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i, j;
	if ((nrh - nrl) > 2)
		nrh = nrl + 2;
	if ((nch - ncl) > 2)
		nch = ncl + 2;
	for (j = nrl; j <= nrh; j++)
		for (i = ncl; i <= nch; i++)
			dst[j][i] = src[j][i];
}

/* -------------------------------------------------------------- */
/* Convert standard C type 2D array into an indirect referenced array */
double **convert_dmatrix(
double *a,	/* base address of normal C array, ie &a[0][0] */
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i, j, nrow = nrh-nrl+1, ncol = nch-ncl+1;
	double **m;

	/* Allocate pointers to rows */
	if ((m = (double **) malloc(nrow * sizeof(double*))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in convert_dmatrix()");
	}

	m -= nrl;

	m[nrl] = a - ncl;
	for(i=1, j = nrl+1; i < nrow; i++, j++)
		m[j] = m[j-1] + ncol;
	/* return pointer to array of pointers */
	return m;
}

/* Free the indirect array reference (but not array) */
void free_convert_dmatrix(
double **m,
int nrl,
int nrh,
int ncl,
int nch
) {
	if (m == NULL)
		return;

	free((char*) (m+nrl));
}

/* -------------------------- */
/* Float vector malloc/free */
float *fvector(
int nl,		/* Lowest index */
int nh		/* Highest index */
)	{
	float *v;

	if ((v = (float *) malloc((nh-nl+1) * sizeof(float))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in fvector()");
	}
	return v-nl;
}

float *fvectorz(
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	float *v;

	if ((v = (float *) calloc(nh-nl+1, sizeof(float))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in fvector()");
	}
	return v-nl;
}

void free_fvector(
float *v,
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	if (v == NULL)
		return;

	free((char *) (v+nl));
}

/* --------------------- */
/* 2D Float matrix malloc/free */
float **fmatrix(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	float **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (float **) malloc((rows + 1) * sizeof(float *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (float *) malloc(rows * cols * sizeof(float))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

float **fmatrixz(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	float **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (float **) malloc((rows + 1) * sizeof(float *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (float *) calloc(rows * cols, sizeof(float))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

void free_fmatrix(
float **m,
int nrl,
int nrh,
int ncl,
int nch
) {
	if (m == NULL)
		return;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	free((char *)(m[nrl-1]));
	free((char *)(m+nrl-1));
}

/* ------------------ */
/* Integer vector malloc/free */
int *ivector(
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	int *v;

	if ((v = (int *) malloc((nh-nl+1) * sizeof(int))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in ivector()");
	}
	return v-nl;
}

int *ivectorz(
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	int *v;

	if ((v = (int *) calloc(nh-nl+1, sizeof(int))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in ivector()");
	}
	return v-nl;
}

void free_ivector(
int *v,
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	if (v == NULL)
		return;

	free((char *) (v+nl));
}


/* ------------------------------ */
/* 2D integer matrix malloc/free */

int **imatrix(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	int **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (int **) malloc((rows + 1) * sizeof(int *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in imatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (int *) malloc(rows * cols * sizeof(int))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in imatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

int **imatrixz(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	int **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (int **) malloc((rows + 1) * sizeof(int *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in imatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (int *) calloc(rows * cols, sizeof(int))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in imatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

void free_imatrix(
int **m,
int nrl,
int nrh,
int ncl,
int nch
) {
	if (m == NULL)
		return;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	free((char *)(m[nrl-1]));
	free((char *)(m+nrl-1));
}

/* ------------------ */
/* Short vector malloc/free */
short *svector(
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	short *v;

	if ((v = (short *) malloc((nh-nl+1) * sizeof(short))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in svector()");
	}
	return v-nl;
}

short *svectorz(
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	short *v;

	if ((v = (short *) calloc(nh-nl+1, sizeof(short))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in svector()");
	}
	return v-nl;
}

void free_svector(
short *v,
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	if (v == NULL)
		return;

	free((char *) (v+nl));
}


/* ------------------------------ */
/* 2D short vector malloc/free */

short **smatrix(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	short **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (short **) malloc((rows + 1) * sizeof(short *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in smatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (short *) malloc(rows * cols * sizeof(short))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in smatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

short **smatrixz(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	short **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (short **) malloc((rows + 1) * sizeof(short *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in smatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (short *) calloc(rows * cols, sizeof(short))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in smatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

void free_smatrix(
short **m,
int nrl,
int nrh,
int ncl,
int nch
) {
	if (m == NULL)
		return;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	free((char *)(m[nrl-1]));
	free((char *)(m+nrl-1));
}

/***************************/
/* Basic matrix operations */
/***************************/

/* Transpose a 0 base matrix */
void matrix_trans(double **d, double **s, int nr,  int nc) {
	int i, j;

	for (i = 0; i < nr; i++) {
		for (j = 0; j < nc; j++) {
			d[j][i] = s[i][j];
		}
	}
}

/* Transpose a 0 base symetrical matrix in place */
void sym_matrix_trans(double **m, int n) {
	int i, j;

	for (i = 0; i < n; i++) {
		for (j = i+1; j < n; j++) {
			double tt = m[j][i]; 
			m[j][i] = m[i][j];
			m[i][j] = tt;
		}
	}
}

/* Multiply two 0 based matricies */
/* Return nz on matching error */
int matrix_mult(
	double **d,  int nr,  int nc,
	double **s1, int nr1, int nc1,
	double **s2, int nr2, int nc2
) {
	int i, j, k;
	double **_d = d;

	/* s1 and s2 must mesh */
	if (nc1 != nr2)
		return 1;

	/* Output rows = s1 rows */
	if (nr != nr1)
		return 2;

	/* Output colums = s2 columns */
	if (nc != nc2)
		return 3;

	if (d == s1 || d == s2)
		_d = dmatrix(0, nr-1, 0, nc-1);

	for (i = 0; i < nr1; i++) {
		for (j = 0; j < nc2; j++) { 
			_d[i][j] = 0.0;  
			for (k = 0; k < nc1; k++) {
				_d[i][j] += s1[i][k] * s2[k][j];
			}
		}
	}

	if (_d != d) {
		copy_dmatrix(d, _d, 0, nr-1, 0, nc-1);
		free_dmatrix(_d, 0, nr-1, 0, nc-1);
	}

	return 0;
}

/* Matrix multiply transpose of s1 by s2 */
/* 0 based matricies,  */
/* This is usefull for using results of lu_invert() */
int matrix_trans_mult(
	double **d,  int nr,  int nc,
	double **ts1, int nr1, int nc1,
	double **s2, int nr2, int nc2
) {
	int i, j, k;
	double **_d = d;

	/* s1 and s2 must mesh */
	if (nr1 != nr2)
		return 1;

	/* Output rows = s1 columns */
	if (nr != nc1)
		return 2;

	/* Output colums = s2 columns */
	if (nc != nc2)
		return 3;

	if (d == ts1 || d == s2)
		_d = dmatrix(0, nr-1, 0, nc-1);

	for (i = 0; i < nc1; i++) {
		for (j = 0; j < nc2; j++) { 
			_d[i][j] = 0.0;  
			for (k = 0; k < nr1; k++) {
				_d[i][j] += ts1[k][i] * s2[k][j];
			}
		}
	}

	if (_d != d) {
		copy_dmatrix(d, _d, 0, nr-1, 0, nc-1);
		free_dmatrix(_d, 0, nr-1, 0, nc-1);
	}

	return 0;
}

/* Matrix multiply s1 by transpose of s2 */
/* 0 based matricies,  */
int matrix_mult_trans(
	double **d,  int nr,  int nc,
	double **s1, int nr1, int nc1,
	double **ts2, int nr2, int nc2
) {
	int i, j, k;
	double **_d = d;

	/* s1 and s2 must mesh */
	if (nc1 != nc2)
		return 1;

	/* Output rows = s1 rows */
	if (nr != nr1)
		return 2;

	/* Output colums = s2 rows */
	if (nc != nr2)
		return 3;

	if (d == s1 || d == ts2)
		_d = dmatrix(0, nr-1, 0, nc-1);

	for (i = 0; i < nr1; i++) {
		for (j = 0; j < nr2; j++) { 
			_d[i][j] = 0.0;  
			for (k = 0; k < nc1; k++) {
				_d[i][j] += s1[i][k] * ts2[j][k];
			}
		}
	}

	if (_d != d) {
		copy_dmatrix(d, _d, 0, nr-1, 0, nc-1);
		free_dmatrix(_d, 0, nr-1, 0, nc-1);
	}

	return 0;
}

/* Multiply a 0 based matrix by a vector */
/* d may be same as v */
int matrix_vect_mult(
	double *d, int nd,
	double **m, int nr, int nc,
	double *v, int nv
) {
	int i, j;
	double *_v = v, vv[20];

	if (d == v) {
		if (nv <= 20) {
			_v = vv;
		} else {
			_v = dvector(0, nv-1);
		}
		for (j = 0; j < nv; j++)
			_v[j]  = v[j];
	}

	/* Input vector must match matrix columns */
	if (nv != nc)
		return 1;

	/* Output vector must match matrix rows */
	if (nd != nr)
		return 2;

	for (i = 0; i < nd; i++) {
		d[i] = 0.0;  
		for (j = 0; j < nv; j++) {
			d[i] += m[i][j] * _v[j];
		}
	}

	if (_v != v && _v != vv)
		free_dvector(_v, 0, nv-1);

	return 0;
}

/* Multiply a 0 based transposed matrix by a vector */
/* d may be same as v */
int matrix_trans_vect_mult(
	double *d, int nd,
	double **m, int nr, int nc,
	double *v, int nv
) {
	int i, j;
	double *_v = v, vv[20];

	if (d == v) {
		if (nv <= 20) {
			_v = vv;
		} else {
			_v = dvector(0, nv-1);
		}
		for (j = 0; j < nv; j++)
			_v[j]  = v[j];
	}

	/* Input vector must match matrix columns */
	if (nv != nr)
		return 1;

	/* Output vector must match matrix rows */
	if (nd != nc)
		return 2;

	for (i = 0; i < nd; i++) {
		d[i] = 0.0;  
		for (j = 0; j < nv; j++)
			d[i] += m[j][i] * _v[j];
	}

	if (_v != v && _v != vv)
		free_dvector(_v, 0, nv-1);

	return 0;
}

/* Add 0 based matricies */
void matrix_add(double **d,  double **s1, double **s2, int nr,  int nc) {
	int i, j;
	for (i = 0; i < nr; i++) {
		for (j = 0; j < nc; j++)
			d[i][j] = s1[i][j] + s2[i][j];
	}
}

/* Add scaled 0 based matricies */
void matrix_scaled_add(double **d,  double **s1, double scale, double **s2, int nr,  int nc) {
	int i, j;
	for (i = 0; i < nr; i++) {
		for (j = 0; j < nc; j++)
			d[i][j] = s1[i][j] + scale * s2[i][j];
	}
}

/* Copy a 0 base matrix */
void matrix_cpy(double **d, double **s, int nr,  int nc) {
	int i, j;
	for (i = 0; i < nr; i++) {
		for (j = 0; j < nc; j++)
			d[i][j] = s[i][j];
	}
}

/* Set a 0 base matrix */
void matrix_set(double **d, double v, int nr,  int nc) {
	int i, j;
	for (i = 0; i < nr; i++) {
		for (j = 0; j < nc; j++)
			d[i][j] = v;
	}
}

/* Return the maximum absolute difference between any corresponding elemnt */
double matrix_max_diff(double **d, double **s, int nr,  int nc) {
	int i, j;
	double md = 0.0;

	for (i = 0; i < nr; i++) {
		for (j = 0; j < nc; j++) {
			double tt = d[i][j] - s[i][j];
			tt = fabs(tt);
			if (tt > md)
				md = tt;
		}
	}
	return md;
}


/* Set zero based dvector */
void vect_set(double *d, double v, int len) {
	if (v == 0.0)
		memset((char *)d, 0, len * sizeof(double));
	else {
		int i;
		for (i = 0; i < len; i++)
			d[i] = v;
	}
}

/* Negate and copy a vector, d = -v */
/* d may be same as v */
void vect_neg(double *d, double *s, int len) {
	int i;
	for (i = 0; i < len; i++)
		d[i] = -s[i];
}

/* Add two vectors */
/* d may be same as v */
void vect_add(
	double *d,
	double *v, int len
) {
	int i;

	for (i = 0; i < len; i++)
		d[i] += v[i];
}

/* Add two vectors, d = s1 + s2 */
void vect_add3(
	double *d, double *s1, double *s2, int len
) {
	int i;
	for (i = 0; i < len; i++)
		d[i] = s1[i] + s2[i];
}

/* Subtract two vectors, d -= v */
/* d may be same as v */
void vect_sub(
	double *d, double *v, int len
) {
	int i;
	for (i = 0; i < len; i++)
		d[i] -= v[i];
}

/* Subtract two vectors, d = s1 - s2 */
void vect_sub3(
	double *d, double *s1, double *s2, int len
) {
	int i;
	for (i = 0; i < len; i++)
		d[i] = s1[i] - s2[i];
}

/* Invert and copy a vector, d = 1/s */
void vect_invert(double *d, double *s, int len) {
	int i;
	for (i = 0; i < len; i++)
		d[i] = 1.0/s[i];
}

/* Multiply the dest by the source, d *= s */
void vect_mul(
	double *d, double *s, int len
) {
	int i;
	for (i = 0; i < len; i++)
		d[i] *= s[i];
}

/* Multiply the elements of two vectors, d = s1 * s2 */
void vect_mul3(
	double *d, double *s1, double *s2, int len
) {
	int i;
	for (i = 0; i < len; i++)
		d[i] = s1[i] * s2[i];
}

/* Divide the destination by the source, d /= s1 */
void vect_div(
	double *d, double *s, int len
) {
	int i;
	for (i = 0; i < len; i++)
		d[i] /= s[i];
}

/* Divide the elements of two vectors, d = s1 / s2 */
void vect_div3(double *d, double *s1, double *s2, int len) {
	int i;
	for (i = 0; i < len; i++)
		d[i] = s1[i] / s2[i];
}

/* Divide the elements of two vectors, d = s1 / s2 */
/* Return 1.0 if s2 < 1e-6 */
void vect_div3_safe(double *d, double *s1, double *s2, int len) {
	int i;
	for (i = 0; i < len; i++) {
		if (fabs(s2[i]) >= 1e-6)
			d[i] = s1[i] / s2[i];
		else
			d[i] = 1.0;
	}
}
/* Multiply and divide, d *= s1 / s2 */
void vect_muldiv(double *d, double *s1, double *s2, int len) {
	int i;
	for (i = 0; i < len; i++)
		d[i] *= s1[i] / s2[i];
}

/* Multiply and divide, d *= s1 / s2 */
/* Don't change d if s2 < 1e-6 */
void vect_muldiv_safe(double *d, double *s1, double *s2, int len) {
	int i;
	for (i = 0; i < len; i++) {
		if (fabs(s2[i]) >= 1e-6)
			d[i] *= s1[i] / s2[i];
	}
}

/* Multiply and divide, d = s1 * s2 / s3 */
void vect_muldiv3(double *d, double *s1, double *s2, double *s3, int len) {
	int i;
	for (i = 0; i < len; i++)
		d[i] = s1[i] * s2[i] / s3[i];
}

/* Return the maximum elements from two vectors */
void vect_max_elem(double *d, double *s, int len) {
	int i;
	for (i = 0; i < len; i++)
		d[i] = (d[i] > s[i]) ? d[i] : s[i];
}

/* Return the maximum elements from two vectors */
void vect_max_elem3(double *d, double *s1, double *s2, int len) {
	int i;
	for (i = 0; i < len; i++)
		d[i] = (s1[i] > s2[i]) ? s1[i] : s2[i];
}

/* Scale a vector, */
/* d may be same as v */
void vect_scale(double *d, double *s, double scale, int len) {
	int i;

	for (i = 0; i < len; i++)
		d[i] = s[i] * scale;
}

/* 1 argument scale a vector, */
void vect_scale1(double *d, double scale, int len) {
	int i;

	for (i = 0; i < len; i++)
		d[i] *= scale;
}

/* Blend between s0 and s1 for bl 0..1 */
/* i.e. d = (1 - bl) * s0 + bl * s1 */
void vect_blend(double *d, double *s0, double *s1, double bl, int len) {
	int i;

	for (i = 0; i < len; i++)
		d[i] = (1.0 - bl) * s0[i] + bl * s1[i];
}

/* Scale s and add to d */
void vect_scaleadd(double *d, double *s, double scale, int len) {
	int i;

	for (i = 0; i < len; i++)
		d[i] += s[i] * scale;
}

/* Take dot product of two vectors */
double vect_dot(double *s1, double *s2, int len) {
	int i;
	double rv = 0.0;
	for (i = 0; i < len; i++)
		rv += s1[i] * s2[i];
	return rv;
}

/* Return the vectors magnitude (norm) */
double vect_mag(double *s, int len) {
	int i;
	double rv = 0.0;

	for (i = 0; i < len; i++)
		rv += s[i] * s[i];

	return sqrt(rv);
}

/* Return the vectors magnitude squared (norm squared) */
double vect_magsq(double *s, int len) {
	int i;
	double rv = 0.0;

	for (i = 0; i < len; i++)
		rv += s[i] * s[i];

	return rv;
}

/* Return the magnitude (norm) of the difference between two vectors */
double vect_diffmag(double *s1, double *s2, int len) {
	int i;
	double rv = 0.0;

	for (i = 0; i < len; i++) {
		double tt = s1[i] - s2[i];
		rv += tt * tt;
	}

	return sqrt(rv);
}

/* Return the sum of the vectors elements */
double vect_sum(double *s, int len) {
	int i;
	double rv = 0.0;

	for (i = 0; i < len; i++)
		rv += s[i];

	return rv;
}

/* Return the average value of the elements of a vector */
double vect_avg(double *s, int len) {
	int i;
	double rv = 0.0;

	if (len <= 0)
		return rv;

	for (i = 0; i < len; i++)
		rv += s[i];

	return rv/(double)len;
}

/* Return the normalized vectors */
/* Return nz if norm is zero */
int vect_normalize(double *d, double *s, int len) {
	int i;
	double nv = 0.0;
	int rv = 0;

	for (i = 0; i < len; i++)
		nv += s[i] * s[i];
	nv = sqrt(nv);

	if (nv < 1e-9) {
		nv = 1.0;
		rv = 1;
	} else {
		nv = 1.0/nv;
	}

	for (i = 0; i < len; i++)
		d[i] = s[i] * nv;

	return rv;
}

/* Return the vectors elements maximum absolute magnitude */
double vect_max_mag(double *s, int len) {
	int i;
	double rv = 0.0;

	for (i = 0; i < len; i++) {
		double tt = fabs(s[i]);
		if (tt > rv)
			rv = tt;
	}
	return rv;
}

/* Return the vectors elements maximum value */
double vect_max(double *s, int len) {
	int i;
	double rv = -DBL_MAX;

	for (i = 0; i < len; i++) {
		if (s[i] > rv)
			rv = s[i];
	}
	return rv;
}

/* Return the elements maximum value from two vectors */
double vect_max2(double *s1, int len1, double *s2, int len2) {
	int i;
	double rv = -DBL_MAX;

	for (i = 0; i < len1; i++) {
		if (s1[i] > rv)
			rv = s1[i];
	}

	for (i = 0; i < len2; i++) {
		if (s2[i] > rv)
			rv = s2[i];
	}
	return rv;
}

/* Return the vectors elements minimum value */
double vect_min(double *s, int len) {
	int i;
	double rv = DBL_MAX;

	for (i = 0; i < len; i++) {
		if (s[i] < rv)
			rv = s[i];
	}
	return rv;
}

/* Take absolute of each element */
void vect_abs(double *d, double *s, int len) {
	int i;

	for (i = 0; i < len; i++)
		d[i] = fabs(s[i]);
}

/* Take individual elements to signed power */
void vect_spow(double *d, double *s, double pv, int len) {
	int i;

	for (i = 0; i < len; i++) {
		/* pow() isn't guaranteed to behave ... */
		if (pv != 0.0) {
			if (pv < 0.0) {
				if (s[i] < 0.0)
					d[i] = 1.0/-pow(-s[i], -pv);
				else
					d[i] = 1.0/pow(s[i], -pv);
			} else {
				if (s[i] < 0.0)
					d[i] = -pow(-s[i], pv);
				else
					d[i] = pow(s[i], pv);
			}
		}
	}
}

/* Clip to a range */
/* Return NZ if any clipping occured */
/* d may be null */
int vect_clip(double *d, double *s, double min, double max, int len) {
	int i, clip = 0;

	for (i = 0; i < len; i++) {
		if (s[i] < min) {
			clip = 1;
			if (d != NULL)
				d[i] = min;
		} else if (s[i] > max) {
			clip = 1;
			if (d != NULL)
		 		d[i] = max;
		} else if (d != NULL) {
			d[i] = s[i];
		}
	}

	return clip;
}

/* Compare two vectors and return nz if they are the same */
int vect_cmp(double *s1, double *s2, int len) {
	int i;

	for (i = 0; i < len; i++) {
		if (s1[i] != s2[i])
			return 0;
	}
	return 1;
}

/* - - - - - - - - - - - - - - - - - - - - - - */

/* Linearly search a vector from 0 for a given value. */
/* The must be ordered from smallest to largest. */
/* The returned index is p[ix] <= val < p[ix+1] */
/* Clip to the range of the vector 0..len-1 */
int vect_lsearch(double *p, double in, int len) {
	int i;

	if (in < p[0])
		in = p[0];
	else if (in > p[len-1])
		in = p[len-1];

	/* Search for location of input within p[] */
	for (i = 0; i < (len-1); i++) {
		if (in >= p[i] && in < p[i+1])
			break;
	}
	return i;
}

/* Binary search a vector from 0 for a given value. */
/* The must be ordered from smallest to largest. */
/* The returned index is p[ix] <= val < p[ix+1] */
/* Clip to the range of the vector 0..len-1 */
int vect_bsearch(double *p, double in, int len) {
	int i0, i1, i2;
	double v0, v1, v2;

//fprintf(stderr,"~1 bsearch in %f len %d\n",in,len);
	i0 = 0;
	i2 = len - 1;
	v0 = p[i0];
	v2 = p[i2];

//fprintf(stderr,"~1 i0 %d v0 %f i2 %d v2 %f\n",i0,v0,i2,v2);

	if (in <= v0) {
//fprintf(stderr,"~1 clip low\n");
		i0 = i0;
	} else if (in >= v2) {
//fprintf(stderr,"~1 clip high\n");
		i0 = i2;
	} else {
		do {
			i1 = (i2 + i0)/2;		/* Trial point */
			v1 = p[i1];				/* Value at trial */
//fprintf(stderr,"~1 i0 %d v0 %f i1 %d v1 %f i2 %d v2 %f\n",i0,v0,i1,v1,i2,v2);
			if (v1 < in) {
				i0 = i1;			/* Take top half */
				v0 = v1;
			} else {
				i2 = i1;			/* Take bottom half */
				v2 = v1;
			}
		} while ((i2 - i0) > 1);
		
	}
//fprintf(stderr,"~1 bsearch returnin %d\n",i0);
	return i0;
}

/* Do a linear interpolation into a vector */
/* Input 0.0 .. 1.0, clips result if outside that range */
double vect_lerp(double *s, double in, int len) {
	int i;
	double out;

	if (in < 0.0)
		in = 0.0;
	else if (in > 1.0)
		in = 1.0;

	in *= (len-1.0);				/* fp index value */
	i = (int)floor(in);				/* Lower grid of point */

	if (i >= (len-2))				/* Force to lower of two */
		i = len-2;

	in = in - (double)i;			/* Weight to upper grid point */

	out = ((1.0 - in) * s[i]) + (in * s[i+1]);

	return out;
}

/* Do a reverse linear interpolation of a vector. */
/* This uses a simple search for the given value, */
/* and so will return the reverse interpolation of the */
/* matching span with the smallest index value. */ 
/* Output 0.0 .. 1.0, clips result if outside that range */
/* to the nearest index */
double vect_rev_lerp(double *s, double in, int len) {
	int i;
	double out;
	double minv = 1e38, maxv = -1e38;
	double minx, maxx;

	/* Search for location of input within s[] */
	for (i = 0; i < (len-1); i++) {
		if (in >= s[i] && in < s[i+1])
			break;

		if (s[i] < minv) {
			minv = s[i];
			minx = i;
		}
		if (s[i] > maxv) {
			maxv = s[i];
			maxx = i;
		}
	}

	/* in value is outside vector value range */
	if (i >= (len-1)) {
		if (in < minv)
			out = minx/(len-1.0);
		else 
			out = maxx/(len-1.0);
		
	} else {
		out = (double)i + (in - s[i])/(s[i+1] - s[i]);
		out /= (len-1.0);
	}

	return out;
}

/* Do a linear interpolation into a vector pair, position->value. */
/* It is assumed that p[] is in sorted smallest to largest order, */
/* and that the entries are distinct. */
/* If input is outside range of p[], then the returned value will be */
/* linearly extrapolated. */
double vect_lerp2x(double *p, double *v, double in, int len) {
	int i;
	double out;

	/* Locate pair to interpolate between */
	i = vect_bsearch(p, in, len);
//fprintf(stderr,"~1 bsearch returned %d\n",i);
	if (i > (len-1))
		i = len-1;
//fprintf(stderr,"~1 bsearch after clip %d\n",i);

	in = (in - p[i])/(p[i+1] - p[i]);
//fprintf(stderr,"~1 lerp blend f %f\n",in);

	out = ((1.0 - in) * v[i]) + (in * v[i+1]);
//fprintf(stderr,"~1 lerp2 interpin v[%d] %f and v[%d] %f returning %f\n", i,v[i],i+1,v[i+1],out);

	return out;
}

/* Same as above, but clip rather than extrapolating. */
double vect_lerp2(double *p, double *v, double in, int len) {
	double ret;

	if (in < p[0]) {
//fprintf(stderr,"~1 in %f < p[0] %f returning v[0] %f\n",in,p[0],v[0]);
		return v[0];
	} else if (in > p[len-1]) {
//fprintf(stderr,"~1 in %f > p[%d] %f returning v[%d] %f\n",in,len-1,p[len-1],len-1,v[len-1]);
		return v[len-1];
	}

	ret = vect_lerp2x(p, v, in, len);

//fprintf(stderr,"~1 in %f returning ler2x %f\n",in,ret);
	return ret;
}

/* - - - - - - - - - - - - - - - - - - - - - - */

/* Set zero based ivector */
void ivect_set(int *d, int v, int len) {
	if (v == 0)
		memset((char *)d, 0, len * sizeof(int));
	else {
		int i;
		for (i = 0; i < len; i++)
			d[i] = v;
	}
}


/* - - - - - - - - - - - - - - - - - - - - - - */

/* Print double matrix to FILE * */
/* id identifies matrix */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void dump_dmatrix(FILE *fp, char *id, char *pfx, double **a, int nr,  int nc) {
	int i, j;
	fprintf(fp, "%s%s[%d][%d]\n",pfx,id,nr,nc);

	for (j = 0; j < nr; j++) {
		fprintf(fp, "%s ",pfx);
		for (i = 0; i < nc; i++)
			fprintf(fp, "%f%s",a[j][i], i < (nc-1) ? ", " : "");
		fprintf(fp, "\n");
	}
}

/* Print double matrix to FILE * with formatting */
/* id identifies matrix */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void dump_dmatrix_fmt(FILE *fp, char *id, char *pfx, double **a, int nr, int nc, char *fmt) {
	int i, j;
	fprintf(fp, "%s%s[%d][%d]\n",pfx,id,nr,nc);

	for (j = 0; j < nr; j++) {
		fprintf(fp, "%s ",pfx);
		for (i = 0; i < nc; i++) {
			fprintf(fp, fmt, a[j][i]);
			if (i < (nc-1))
				fprintf(fp, "%s",", ");
		}
		fprintf(fp, "\n");
	}
}

/* Print float matrix to FILE * */
/* id identifies matrix */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void dump_fmatrix(FILE *fp, char *id, char *pfx, float **a, int nr,  int nc) {
	int i, j;
	fprintf(fp, "%s%s[%d][%d]\n",pfx,id,nr,nc);

	for (j = 0; j < nr; j++) {
		fprintf(fp, "%s ",pfx);
		for (i = 0; i < nc; i++)
			fprintf(fp, "%f%s",a[j][i], i < (nc-1) ? ", " : "");
		fprintf(fp, "\n");
	}
}

/* Print int matrix to FILE * */
/* id identifies matrix */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void dump_imatrix(FILE *fp, char *id, char *pfx, int **a, int nr,  int nc) {
	int i, j;
	fprintf(fp, "%s%s[%d][%d]\n",pfx,id,nr,nc);

	for (j = 0; j < nr; j++) {
		fprintf(fp, "%s ",pfx);
		for (i = 0; i < nc; i++)
			fprintf(fp, "%d%s",a[j][i], i < (nc-1) ? ", " : "");
		fprintf(fp, "\n");
	}
}

/* Print short matrix to FILE * */
/* id identifies matrix */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void dump_smatrix(FILE *fp, char *id, char *pfx, short **a, int nr,  int nc) {
	int i, j;
	fprintf(fp, "%s%s[%d][%d]\n",pfx,id,nr,nc);

	for (j = 0; j < nr; j++) {
		fprintf(fp, "%s ",pfx);
		for (i = 0; i < nc; i++)
			fprintf(fp, "%d%s",a[j][i], i < (nc-1) ? ", " : "");
		fprintf(fp, "\n");
	}
}

/* Print double vector to FILE * */
/* id identifies vector */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void dump_dvector(FILE *fp, char *id, char *pfx, double *a, int nc) {
	int i;
	fprintf(fp, "%s%s[%d]\n",pfx,id,nc);
	fprintf(fp, "%s ",pfx);
	for (i = 0; i < nc; i++)
		fprintf(fp, "%f%s",a[i], i < (nc-1) ? ", " : "");
	fprintf(fp, "\n");
}

/* Print double vector to FILE * with formatting */
/* id identifies vector */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void dump_dvector_fmt(FILE *fp, char *id, char *pfx, double *a, int nc, char *fmt) {
	int i;
	fprintf(fp, "%s%s[%d]\n",pfx,id,nc);
	fprintf(fp, "%s ",pfx);
	for (i = 0; i < nc; i++) {
		fprintf(fp, fmt, a[i]);
		if (i < (nc-1))
			fprintf(fp, "%s",", ");
	}
	fprintf(fp, "\n");
}

/* Print float vector to FILE * */
/* id identifies vector */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void dump_fvector(FILE *fp, char *id, char *pfx, float *a, int nc) {
	int i;
	fprintf(fp, "%s%s[%d]\n",pfx,id,nc);
	fprintf(fp, "%s ",pfx);
	for (i = 0; i < nc; i++)
		fprintf(fp, "%f%s",a[i], i < (nc-1) ? ", " : "");
	fprintf(fp, "\n");
}

/* Print int vector to FILE * */
/* id identifies vector */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void dump_ivector(FILE *fp, char *id, char *pfx, int *a, int nc) {
	int i;
	fprintf(fp, "%s%s[%d]\n",pfx,id,nc);
	fprintf(fp, "%s ",pfx);
	for (i = 0; i < nc; i++)
		fprintf(fp, "%d%s",a[i], i < (nc-1) ? ", " : "");
	fprintf(fp, "\n");
}

/* Print short vector to FILE * */
/* id identifies vector */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void dump_svector(FILE *fp, char *id, char *pfx, short *a, int nc) {
	int i;
	fprintf(fp, "%s%s[%d]\n",pfx,id,nc);
	fprintf(fp, "%s ",pfx);
	for (i = 0; i < nc; i++)
		fprintf(fp, "%d%s",a[i], i < (nc-1) ? ", " : "");
	fprintf(fp, "\n");
}

/* Format double matrix as C code to FILE */
/* id is variable name */
/* pfx used at start of each line */
/* hb sets horizontal element limit to wrap */
/* Assumed indexed from 0 */
void acode_dmatrix(FILE *fp, char *id, char *pfx, double **a, int nr,  int nc, int hb) {
	int i, j;
	fprintf(fp, "%sdouble %s[%d][%d] = {\n",pfx,id,nr,nc);

	for (j = 0; j < nr; j++) {
		fprintf(fp, "%s\t{ ",pfx);
		for (i = 0; i < nc; i++) {
			fprintf(fp, "%f%s",a[j][i], i < (nc-1) ? ", " : "");
			if ((i % hb) == (hb-1))
				fprintf(fp, "\n%s\t  ",pfx);
		}
		fprintf(fp, " }%s\n", j < (nr-1) ? "," : "");
	}
	fprintf(fp, "%s};\n",pfx);
}

/* Format double vector as C code to FILE */
/* id is variable name */
/* pfx used at start of each line */
/* hb sets horizontal element limit to wrap */
/* Assumed indexed from 0 */
void acode_dvector(FILE *fp, char *id, char *pfx, double *v, int nc, int hb) {
	int i;
	fprintf(fp, "%sdouble %s[%d] = { ",pfx,id,nc);

	for (i = 0; i < nc; i++) {
		fprintf(fp, "%f%s",v[i], i < (nc-1) ? ", " : "");
		if ((i % hb) == (hb-1))
			fprintf(fp, "\n%s\t  ",pfx);
	}
	fprintf(fp, "%s};\n",pfx);
}

/* Format unsigned char vector as C code to FILE */
/* id is variable name */
/* pfx used at start of each line */
/* hb sets horizontal element limit to wrap */
/* Assumed indexed from 0 */
void acode_cvector(FILE *fp, char *id, char *pfx, unsigned char *v, int nc, int hb) {
	int i;
	fprintf(fp, "%sunsigned char %s[%d] = { ",pfx,id,nc);

	for (i = 0; i < nc; i++) {
		fprintf(fp, "%u%s",v[i], i < (nc-1) ? ", " : "");
		if ((i % hb) == (hb-1))
			fprintf(fp, "\n%s\t  ",pfx);
	}
	fprintf(fp, "%s};\n",pfx);
}

/* - - - - - - - - - - - - - - - - - - - - - - */

/* Print double matrix to g_log debug */
/* id identifies matrix */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void adump_dmatrix(a1log *log, char *id, char *pfx, double **a, int nr,  int nc) {
	int i, j;
	a1logd(g_log, 0, "%s%s[%d][%d]\n",pfx,id,nr,nc);

	for (j = 0; j < nr; j++) {
		a1logd(g_log, 0, "%s ",pfx);
		for (i = 0; i < nc; i++)
			a1logd(g_log, 0, "%f%s",a[j][i], i < (nc-1) ? ", " : "");
		a1logd(g_log, 0, "\n");
	}
}

/* Print double matrix to g_log debug with formatting */
/* id identifies matrix */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void adump_dmatrix_fmt(a1log *log, char *id, char *pfx, double **a, int nr, int nc, char *fmt) {
	int i, j;
	a1logd(g_log, 0, "%s%s[%d][%d]\n",pfx,id,nr,nc);

	for (j = 0; j < nr; j++) {
		a1logd(g_log, 0, "%s ",pfx);
		for (i = 0; i < nc; i++) {
			a1logd(g_log, 0, fmt, a[j][i]);
			if (i < (nc-1))
				a1logd(g_log, 0, "%s",", ");
		}
		a1logd(g_log, 0, "\n");
	}
}

/* Print float matrix to g_log debug */
/* id identifies matrix */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void adump_fmatrix(a1log *log, char *id, char *pfx, float **a, int nr,  int nc) {
	int i, j;
	a1logd(g_log, 0, "%s%s[%d][%d]\n",pfx,id,nr,nc);

	for (j = 0; j < nr; j++) {
		a1logd(g_log, 0, "%s ",pfx);
		for (i = 0; i < nc; i++)
			a1logd(g_log, 0, "%f%s",a[j][i], i < (nc-1) ? ", " : "");
		a1logd(g_log, 0, "\n");
	}
}

/* Print int matrix to g_log debug */
/* id identifies matrix */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void adump_imatrix(a1log *log, char *id, char *pfx, int **a, int nr,  int nc) {
	int i, j;
	a1logd(g_log, 0, "%s%s[%d][%d]\n",pfx,id,nr,nc);

	for (j = 0; j < nr; j++) {
		a1logd(g_log, 0, "%s ",pfx);
		for (i = 0; i < nc; i++)
			a1logd(g_log, 0, "%d%s",a[j][i], i < (nc-1) ? ", " : "");
		a1logd(g_log, 0, "\n");
	}
}

/* Print short matrix to g_log debug */
/* id identifies matrix */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void adump_smatrix(a1log *log, char *id, char *pfx, short **a, int nr,  int nc) {
	int i, j;
	a1logd(g_log, 0, "%s%s[%d][%d]\n",pfx,id,nr,nc);

	for (j = 0; j < nr; j++) {
		a1logd(g_log, 0, "%s ",pfx);
		for (i = 0; i < nc; i++)
			a1logd(g_log, 0, "%d%s",a[j][i], i < (nc-1) ? ", " : "");
		a1logd(g_log, 0, "\n");
	}
}

/* Print double vector to g_log debug */
/* id identifies vector */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void adump_dvector(a1log *log, char *id, char *pfx, double *a, int nc) {
	int i;
	a1logd(g_log, 0, "%s%s[%d]\n",pfx,id,nc);
	a1logd(g_log, 0, "%s ",pfx);
	for (i = 0; i < nc; i++)
		a1logd(g_log, 0, "%f%s",a[i], i < (nc-1) ? ", " : "");
	a1logd(g_log, 0, "\n");
}

/* Print double vector to g_log debug with formatting */
/* id identifies vector */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void adump_dvector_fmt(a1log *log, char *id, char *pfx, double *a, int nc, char *fmt) {
	int i;
	a1logd(g_log, 0, "%s%s[%d]\n",pfx,id,nc);
	a1logd(g_log, 0, "%s ",pfx);
	for (i = 0; i < nc; i++) {
		a1logd(g_log, 0, fmt, a[i]);
		if (i < (nc-1))
			a1logd(g_log, 0, "%s",", ");
	}
	a1logd(g_log, 0, "\n");
}

/* Print float vector to g_log debug */
/* id identifies vector */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void adump_fvector(a1log *log, char *id, char *pfx, float *a, int nc) {
	int i;
	a1logd(g_log, 0, "%s%s[%d]\n",pfx,id,nc);
	a1logd(g_log, 0, "%s ",pfx);
	for (i = 0; i < nc; i++)
		a1logd(g_log, 0, "%f%s",a[i], i < (nc-1) ? ", " : "");
	a1logd(g_log, 0, "\n");
}

/* Print int vector to g_log debug */
/* id identifies vector */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void adump_ivector(a1log *log, char *id, char *pfx, int *a, int nc) {
	int i;
	a1logd(g_log, 0, "%s%s[%d]\n",pfx,id,nc);
	a1logd(g_log, 0, "%s ",pfx);
	for (i = 0; i < nc; i++)
		a1logd(g_log, 0, "%d%s",a[i], i < (nc-1) ? ", " : "");
	a1logd(g_log, 0, "\n");
}

/* Print short vector to g_log debug */
/* id identifies vector */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void adump_svector(a1log *log, char *id, char *pfx, short *a, int nc) {
	int i;
	a1logd(g_log, 0, "%s%s[%d]\n",pfx,id,nc);
	a1logd(g_log, 0, "%s ",pfx);
	for (i = 0; i < nc; i++)
		a1logd(g_log, 0, "%d%s",a[i], i < (nc-1) ? ", " : "");
	a1logd(g_log, 0, "\n");
}

/* Print C double matrix to g_log debug */
/* id identifies matrix */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void adump_C_dmatrix(a1log *log, char *id, char *pfx, double *a, int nr, int nc) {
	int i, j;
	a1logd(g_log, 0, "%s%s[%d][%d]\n",pfx,id,nr,nc);

	for (j = 0; j < nr; j++, a += nc) {
		a1logd(g_log, 0, "%s ",pfx);
		for (i = 0; i < nc; i++)
			a1logd(g_log, 0, "%f%s",a[i], i < (nc-1) ? ", " : "");
		a1logd(g_log, 0, "\n");
	}
}

/* ============================================================================ */
/* C matrix support */

/* Clip a vector to the range 0.0 .. 1.0 */
/* and return any clipping margine */
double vect_ClipNmarg(int n, double *out, double *in) {
	int j;
	double tt, marg = 0.0;
	for (j = 0; j < n; j++) {
		out[j] = in[j];
		if (out[j] < 0.0) {
			tt = 0.0 - out[j];
			out[j] = 0.0;
			if (tt > marg)
				marg = tt;
		} else if (out[j] > 1.0) {
			tt = out[j] - 1.0;
			out[j] = 1.0;
			if (tt > marg)
				marg = tt;
		}
	}
	return marg;
}

/* 

  mat     in    out

[     ]   []    []
[     ]   []    []
[     ] * [] => []
[     ]   []    []
[     ]   []    []

 */

/* Multiply N vector by NxN transform matrix */
/* Organization is mat[out][in] */
void vect_MulByNxN(int n, double *out, double *mat, double *in) {
	int i, j;
	double _tt[20], *tt = _tt;

	if (n > 20)
		tt = dvector(0, n-1);

	for (i = 0; i < n; i++) {
		tt[i] = 0.0;
		for (j = 0; j < n; j++)
			tt[i] += mat[i * n + j] * in[j];
	}

	for (i = 0; i < n; i++)
		out[i] = tt[i];

	if (n > 20)
		free_dvector(tt, 0, n-1);
}

/* 

  mat         in    out
     N       
              []    
  [     ]     []    []
M [     ] * N [] => [] M
  [     ]     []    []
              []    

 */

/* Multiply N vector by MxN transform matrix to make M vector */
/* Organization is mat[out=M][in=N] */
void vect_MulByMxN(int n, int m, double *out, double *mat, double *in) {
	int i, j;
	double _tt[20], *tt = _tt;

	if (m > 20)
		tt = dvector(0, m-1);

	for (i = 0; i < m; i++) {
		tt[i] = 0.0;
		for (j = 0; j < n; j++)
			tt[i] += mat[i * n + j] * in[j];
	}

	for (i = 0; i < m; i++)
		out[i] = tt[i];

	if (m > 20)
		free_dvector(tt, 0, m-1);
}

/*
   in         mat       out
               M   
            [     ]    
   N        [     ]      M
[     ] * N [     ] => [   ]
            [     ]  
            [     ]    

 */

/* Multiply N vector by transposed NxM transform matrix to make M vector */
/* Organization is mat[in=N][out=M] */
void vect_MulByNxM(int n, int m, double *out, double *mat, double *in) {
	int i, j;
	double _tt[20], *tt = _tt;

	if (m > 20)
		tt = dvector(0, m-1);

	for (i = 0; i < m; i++) {
		tt[i] = 0.0;
		for (j = 0; j < n; j++)
			tt[i] += mat[j * m + i] * in[j];
	}

	for (i = 0; i < m; i++)
		out[i] = tt[i];

	if (m > 20)
		free_dvector(tt, 0, m-1);
}


/* Transpose an NxN matrix */
void matrix_TransposeNxN(int n, double *out, double *in) {
	int i, j;

	if (in != out) {
		for (i = 0; i < n; i++)
			for (j = 0; j < n; j++)
				out[i * n + j] = in[j * n + i];
	} else {
		for (i = 0; i < n; i++) {
			for (j = i+1; j < n; j++) {
				double tt;
				tt = out[i * n + j];
				out[i * n + j] = out[j * n + i];
				out[j * n + i] = tt;
			}
		}
	}
}

/*******************************************/
/* Platform independent IEE754 conversions */
/*******************************************/

/* Convert a native double to an IEEE754 encoded single precision value, */
/* in a platform independent fashion. (ie. This works even */
/* on the rare platforms that don't use IEEE 754 floating */
/* point for their C implementation) */
ORD32 doubletoIEEE754(double d) {
	ORD32 sn = 0, ep = 0, ma;
	ORD32 id;

	/* Convert double to IEEE754 single precision. */
	/* This would be easy if we're running on an IEEE754 architecture, */
	/* but isn't generally portable, so we use ugly code: */

	if (d < 0.0) {
		sn = 1;
		d = -d;
	}
	if (d != 0.0) {
		int ee;
		ee = (int)floor(log(d)/log(2.0));
		if (ee < -126)			/* Allow for denormalized */
			ee = -126;
		d *= pow(0.5, (double)(ee - 23));
		ee += 127;
		if (ee < 1)				/* Too small */
			ee = 0;				/* Zero or denormalised */
		else if (ee > 254) {	/* Too large */
			ee = 255;			/* Infinity */
			d = 0.0;
		}
		ep = ee;
	} else {
		ep = 0;					/* Zero */
	}
	ma = ((ORD32)d) & ((1 << 23)-1);
	id = (sn << 31) | (ep << 23) | ma;

	return id;
}

/* Convert a an IEEE754 encoded single precision value to a native double, */
/* in a platform independent fashion. (ie. This works even */
/* on the rare platforms that don't use IEEE 754 floating */
/* point for their C implementation) */
double IEEE754todouble(ORD32 ip) {
	double op;
	ORD32 sn = 0, ep = 0, ma;

	sn = (ip >> 31) & 0x1;
	ep = (ip >> 23) & 0xff;
	ma = ip & 0x7fffff;

	if (ep == 0) { 		/* Zero or denormalised */
		op = (double)ma/(double)(1 << 23);
		op *= pow(2.0, (-126.0));
	} else {
		op = (double)(ma | (1 << 23))/(double)(1 << 23);
		op *= pow(2.0, (((int)ep)-127.0));
	}
	if (sn)
		op = -op;
	return op;
}

/* Convert a native double to an IEEE754 encoded double precision value, */
/* in a platform independent fashion. (ie. This works even */
/* on the rare platforms that don't use IEEE 754 floating */
/* point for their C implementation) */
/* (Does this clip to range ?) */
ORD64 doubletoIEEE754_64(double d) {
	ORD32 sn = 0, ep = 0;
	ORD64 ma, id;

	/* Convert double to IEEE754 double precision. */
	/* This would be easy if we know we're running on an IEEE754 architecture, */
	/* but isn't generally portable, so we use ugly code: */

	if (d < 0.0) {
		sn = 1;
		d = -d;
	}
	if (d != 0.0) {
		int ee;
		ee = (int)floor(log(d)/log(2.0));
		if (ee < -1022)			/* Allow for denormalized */
			ee = -1022;
		d *= pow(0.5, (double)(ee - 52));
		ee += 1023;				/* Exponent bias */
		if (ee < 1)				/* Too small */
			ee = 0;				/* Zero or denormalised */
		else if (ee > 2046) {	/* Too large */
			ee = 2047;			/* Infinity */
			d = 0.0;
		}
		ep = ee;
	} else {
		ep = 0;					/* Zero */
	}
	ma = ((ORD64)d) & (((ORD64)1 << 52)-1);
	id = ((ORD64)sn << 63) | ((ORD64)ep << 52) | ma;

	return id;
}

/* Convert a an IEEE754 encode double precision value to a native double, */
/* in a platform independent fashion. (ie. This works even */
/* on the rare platforms that don't use IEEE 754 floating */
/* point for their C implementation) */
double IEEE754_64todouble(ORD64 ip) {
	double op;
	ORD32 sn = 0, ep = 0;
	INR64 ma;

	sn = (ip >> 63) & 0x1;
	ep = (ip >> 52) & 0x7ff;
	ma = ip & (((INR64)1 << 52)-1);

	if (ep == 0) { 		/* Zero or denormalised */
		op = (double)ma/(double)((INR64)1 << 52);
		op *= pow(2.0, -1022.0);
	} else {
		op = (double)(ma | ((INR64)1 << 52))/(double)((INR64)1 << 52);
		op *= pow(2.0, (((int)ep)-1023.0));
	}
	if (sn)
		op = -op;
	return op;
}

/* Return a string representation of a 32 bit ctime. */
/* A static buffer is used. There is no \n at the end */
char *ctime_32(const INR32 *timer) {
	char *rv;
#if defined(_MSC_VER) && __MSVCRT_VERSION__ >= 0x0601
	rv = _ctime32((const __time32_t *)timer);
#else
	time_t timerv = (time_t) *timer;		/* May case to 64 bit */
	rv = ctime(&timerv);
#endif

	if (rv != NULL)
		rv[strlen(rv)-1] = '\000';

	return rv;
}

/* Return a string representation of a 64 bit ctime. */
/* A static buffer is used. There is no \n at the end */
char *ctime_64(const INR64 *timer) {
	char *rv;
#if defined(_MSC_VER) && __MSVCRT_VERSION__ >= 0x0601
	rv = _ctime64((const __time64_t *)timer);
#else
	time_t timerv;

	if (sizeof(time_t) == 4 && *timer > 0x7fffffff)
		return NULL;
	timerv = (time_t) *timer;			/* May truncate to 32 bits */
	rv = ctime(&timerv);
#endif

	if (rv != NULL)
		rv[strlen(rv)-1] = '\000';

	return rv;
}

/*******************************************/
/* Native to/from byte buffer functions    */
/*******************************************/

/* No overflow detection is done - */
/* numbers are clipped or truncated. */

/* be = Big Endian */
/* le = Little Endian */

/* - - - - - - - - */
/* Unsigned 8 bit */

unsigned int read_ORD8(ORD8 *p) {
	unsigned int rv;
	rv = ((unsigned int)p[0]);
	return rv;
}

void write_ORD8(ORD8 *p, unsigned int d) {
	if (d > 0xff)
		d = 0xff;
	p[0] = (ORD8)(d);
}

/* - - - - - - - - */
/* Signed 8 bit */

int read_INR8(ORD8 *p) {
	int rv;
	rv = (int)(INR8)p[0];
	return rv;
}

void write_INR8(ORD8 *p, int d) {
	if (d > 0x7f)
		d = 0x7f;
	else if (d < -0x80)
		d = -0x80;
	p[0] = (ORD8)(d);
}

/* - - - - - - - - */
/* Unsigned 16 bit */

unsigned int read_ORD16_be(ORD8 *p) {
	unsigned int rv;
	rv = (((unsigned int)p[0]) << 8)
	   + (((unsigned int)p[1]));
	return rv;
}

unsigned int read_ORD16_le(ORD8 *p) {
	unsigned int rv;
	rv = (((unsigned int)p[0]))
	   + (((unsigned int)p[1]) << 8);
	return rv;
}

void write_ORD16_be(ORD8 *p, unsigned int d) {
	if (d > 0xffff)
		d = 0xffff;
	p[0] = (ORD8)(d >> 8);
	p[1] = (ORD8)(d);
}

void write_ORD16_le(ORD8 *p, unsigned int d) {
	if (d > 0xffff)
		d = 0xffff;
	p[0] = (ORD8)(d);
	p[1] = (ORD8)(d >> 8);
}

/* - - - - - - - - */
/* Signed 16 bit */

int read_INR16_be(ORD8 *p) {
	int rv;
	rv = (((int)(INR8)p[0]) << 8)
	   + (((int)p[1]));
	return rv;
}

int read_INR16_le(ORD8 *p) {
	int rv;
	rv = (((int)p[0]))
	   + (((int)(INR8)p[1]) << 8);
	return rv;
}

void write_INR16_be(ORD8 *p, int d) {
	if (d > 0x7fff)
		d = 0x7fff;
	else if (d < -0x8000)
		d = -0x8000;
	p[0] = (ORD8)(d >> 8);
	p[1] = (ORD8)(d);
}

void write_INR16_le(ORD8 *p, int d) {
	if (d > 0x7fff)
		d = 0x7fff;
	else if (d < -0x8000)
		d = -0x8000;
	p[0] = (ORD8)(d);
	p[1] = (ORD8)(d >> 8);
}

/* - - - - - - - - */
/* Unsigned 32 bit */

unsigned int read_ORD32_be(ORD8 *p) {
	unsigned int rv;
	rv = (((unsigned int)p[0]) << 24)
	   + (((unsigned int)p[1]) << 16)
	   + (((unsigned int)p[2]) << 8)
	   + (((unsigned int)p[3]));
	return rv;
}

unsigned int read_ORD32_le(ORD8 *p) {
	unsigned int rv;
	rv = (((unsigned int)p[0]))
	   + (((unsigned int)p[1]) << 8)
	   + (((unsigned int)p[2]) << 16)
	   + (((unsigned int)p[3]) << 24);
	return rv;
}

void write_ORD32_be(ORD8 *p, unsigned int d) {
	p[0] = (ORD8)(d >> 24);
	p[1] = (ORD8)(d >> 16);
	p[2] = (ORD8)(d >> 8);
	p[3] = (ORD8)(d);
}

void write_ORD32_le(ORD8 *p, unsigned int d) {
	p[0] = (ORD8)(d);
	p[1] = (ORD8)(d >> 8);
	p[2] = (ORD8)(d >> 16);
	p[3] = (ORD8)(d >> 24);
}

/* - - - - - - - - */
/* Signed 32 bit */

int read_INR32_be(ORD8 *p) {
	int rv;
	rv = (((int)(INR8)p[0]) << 24)
	   + (((int)p[1]) << 16)
	   + (((int)p[2]) << 8)
	   + (((int)p[3]));
	return rv;
}

int read_INR32_le(ORD8 *p) {
	int rv;
	rv = (((int)p[0]))
	   + (((int)p[1]) << 8)
	   + (((int)p[2]) << 16)
	   + (((int)(INR8)p[3]) << 24);
	return rv;
}

void write_INR32_be(ORD8 *p, int d) {
	p[0] = (ORD8)(d >> 24);
	p[1] = (ORD8)(d >> 16);
	p[2] = (ORD8)(d >> 8);
	p[3] = (ORD8)(d);
}

void write_INR32_le(ORD8 *p, int d) {
	p[0] = (ORD8)(d);
	p[1] = (ORD8)(d >> 8);
	p[2] = (ORD8)(d >> 16);
	p[3] = (ORD8)(d >> 24);
}

/* - - - - - - - - */
/* Unsigned 64 bit */

ORD64 read_ORD64_be(ORD8 *p) {
	ORD64 rv;
	rv = (((ORD64)p[0]) << 56)
	   + (((ORD64)p[1]) << 48)
	   + (((ORD64)p[2]) << 40)
	   + (((ORD64)p[3]) << 32)
	   + (((ORD64)p[4]) << 24)
	   + (((ORD64)p[5]) << 16)
	   + (((ORD64)p[6]) << 8)
	   + (((ORD64)p[7]));
	return rv;
}

ORD64 read_ORD64_le(ORD8 *p) {
	ORD64 rv;
	rv = (((ORD64)p[0]))
	   + (((ORD64)p[1]) << 8)
	   + (((ORD64)p[2]) << 16)
	   + (((ORD64)p[3]) << 24)
	   + (((ORD64)p[4]) << 32)
	   + (((ORD64)p[5]) << 40)
	   + (((ORD64)p[6]) << 48)
	   + (((ORD64)p[7]) << 56);
	return rv;
}

void write_ORD64_be(ORD8 *p, ORD64 d) {
	p[0] = (ORD8)(d >> 56);
	p[1] = (ORD8)(d >> 48);
	p[2] = (ORD8)(d >> 40);
	p[3] = (ORD8)(d >> 32);
	p[4] = (ORD8)(d >> 24);
	p[5] = (ORD8)(d >> 16);
	p[6] = (ORD8)(d >> 8);
	p[7] = (ORD8)(d);
}

void write_ORD64_le(ORD8 *p, ORD64 d) {
	p[0] = (ORD8)(d);
	p[1] = (ORD8)(d >> 8);
	p[2] = (ORD8)(d >> 16);
	p[3] = (ORD8)(d >> 24);
	p[4] = (ORD8)(d >> 32);
	p[5] = (ORD8)(d >> 40);
	p[6] = (ORD8)(d >> 48);
	p[7] = (ORD8)(d >> 56);
}

/* - - - - - - - - */
/* Signed 64 bit */

INR64 read_INR64_be(ORD8 *p) {
	INR64 rv;
	rv = (((INR64)(INR8)p[0]) << 56)
	   + (((INR64)p[1]) << 48)
	   + (((INR64)p[2]) << 40)
	   + (((INR64)p[3]) << 32)
	   + (((INR64)p[4]) << 24)
	   + (((INR64)p[5]) << 16)
	   + (((INR64)p[6]) << 8)
	   + (((INR64)p[7]));
	return rv;
}

INR64 read_INR64_le(ORD8 *p) {
	INR64 rv;
	rv = (((INR64)p[0]))
	   + (((INR64)p[1]) << 8)
	   + (((INR64)p[2]) << 16)
	   + (((INR64)p[3]) << 24)
	   + (((INR64)p[4]) << 32)
	   + (((INR64)p[5]) << 40)
	   + (((INR64)p[6]) << 48)
	   + (((INR64)(INR8)p[7]) << 56);
	return rv;
}

void write_INR64_be(ORD8 *p, INR64 d) {
	p[0] = (ORD8)(d >> 56);
	p[1] = (ORD8)(d >> 48);
	p[2] = (ORD8)(d >> 40);
	p[3] = (ORD8)(d >> 32);
	p[4] = (ORD8)(d >> 24);
	p[5] = (ORD8)(d >> 16);
	p[6] = (ORD8)(d >> 8);
	p[7] = (ORD8)(d);
}

void write_INR64_le(ORD8 *p, INR64 d) {
	p[0] = (ORD8)(d);
	p[1] = (ORD8)(d >> 8);
	p[2] = (ORD8)(d >> 16);
	p[3] = (ORD8)(d >> 24);
	p[4] = (ORD8)(d >> 32);
	p[5] = (ORD8)(d >> 40);
	p[6] = (ORD8)(d >> 48);
	p[7] = (ORD8)(d >> 56);
}

/* - - - - - - - - */

double read_FLT32_be(ORD8 *p);
double read_FLT32_le(ORD8 *p);
void write_FLT32_be(ORD8 *p, double d);
void write_FLT32_le(ORD8 *p, double d);

double read_FLT64_be(ORD8 *p);
double read_FLT64_le(ORD8 *p);
void write_FLT64_be(ORD8 *p, double d);
void write_FLT64_le(ORD8 *p, double d);

/* - - - - - - - - */
/* IEEE 32 bit float */

double read_FLT32_be(ORD8 *p) {
	ORD32 val;
	val = (((ORD32)p[0]) << 24)
	    + (((ORD32)p[1]) << 16)
	    + (((ORD32)p[2]) << 8)
	    + (((ORD32)p[3]));
	return IEEE754todouble(val);
}

double read_FLT32_le(ORD8 *p) {
	ORD32 val;
	val = (((ORD32)p[0]))
	    + (((ORD32)p[1]) << 8)
	    + (((ORD32)p[2]) << 16)
	    + (((ORD32)p[3]) << 24);
	return IEEE754todouble(val);
}

void write_FLT32_be(ORD8 *p, double d) {
	ORD32 val = doubletoIEEE754(d);
	p[0] = (ORD8)(val >> 24);
	p[1] = (ORD8)(val >> 16);
	p[2] = (ORD8)(val >> 8);
	p[3] = (ORD8)(val);
}

void write_FLT32_le(ORD8 *p, double d) {
	ORD32 val = doubletoIEEE754(d);
	p[0] = (ORD8)(val);
	p[1] = (ORD8)(val >> 8);
	p[2] = (ORD8)(val >> 16);
	p[3] = (ORD8)(val >> 24);
}

/* - - - - - - - - */
/* IEEE 64 bit float */

double read_FLT64_be(ORD8 *p) {
	ORD64 val;
	val = (((ORD64)p[0]) << 56)
	    + (((ORD64)p[1]) << 48)
	    + (((ORD64)p[2]) << 40)
	    + (((ORD64)p[3]) << 32)
	    + (((ORD64)p[4]) << 24)
	    + (((ORD64)p[5]) << 16)
	    + (((ORD64)p[6]) << 8)
	    + (((ORD64)p[7]));
	return IEEE754_64todouble(val);
}

double read_FLT64_le(ORD8 *p) {
	ORD64 val;
	val = (((ORD64)p[0]))
	    + (((ORD64)p[1]) << 8)
	    + (((ORD64)p[2]) << 16)
	    + (((ORD64)p[3]) << 24)
	    + (((ORD64)p[4]) << 32)
	    + (((ORD64)p[5]) << 40)
	    + (((ORD64)p[6]) << 48)
	    + (((ORD64)p[7]) << 56);
	return IEEE754_64todouble(val);
}

void write_FLT64_be(ORD8 *p, double d) {
	ORD64 val = doubletoIEEE754_64(d);
	p[0] = (ORD8)(val >> 56);
	p[1] = (ORD8)(val >> 48);
	p[2] = (ORD8)(val >> 40);
	p[3] = (ORD8)(val >> 32);
	p[4] = (ORD8)(val >> 24);
	p[5] = (ORD8)(val >> 16);
	p[6] = (ORD8)(val >> 8);
	p[7] = (ORD8)(val);
}

void write_FLT64_le(ORD8 *p, double d) {
	ORD64 val = doubletoIEEE754_64(d);
	p[0] = (ORD8)(val);
	p[1] = (ORD8)(val >> 8);
	p[2] = (ORD8)(val >> 16);
	p[3] = (ORD8)(val >> 24);
	p[4] = (ORD8)(val >> 32);
	p[5] = (ORD8)(val >> 40);
	p[6] = (ORD8)(val >> 48);
	p[7] = (ORD8)(val >> 56);
}


/*******************************************/
/* Some bit functions */

/* Return number of set bits */
int count_set_bits(unsigned int val) {
    int c = 0;
    while (val) {
        val &= (val - 1);
        c++;
    }
    return c;
}

/*******************************/
/* System independent timing */

#ifdef NT

/* Sleep for the given number of msec */
void msec_sleep(unsigned int msec) {
	Sleep(msec);
}

/* Return the current time in msec since */
/* the first invokation of msec_time() */
/* (Is this based on timeGetTime() ? ) */
unsigned int msec_time() {
	unsigned int rv;
	static unsigned int startup = 0;

	rv =  GetTickCount();
	if (startup == 0)
		startup = rv;

	return rv - startup;
}

/* Return the current time in usec */
/* since the first invokation of usec_time() */
/* Return -1.0 if not available */
double usec_time() {
	double rv;
	LARGE_INTEGER val;
	static double scale = 0.0;
	static LARGE_INTEGER startup;

	if (scale == 0.0) {
		if (QueryPerformanceFrequency(&val) == 0)
			return -1.0;
		scale = 1000000.0/val.QuadPart;
		QueryPerformanceCounter(&val);
		startup.QuadPart = val.QuadPart;

	} else {
		QueryPerformanceCounter(&val);
	}
	val.QuadPart -= startup.QuadPart;

	rv = val.QuadPart * scale;
		
	return rv;
}

#endif /* NT */

#if defined(UNIX)

/* Sleep for the given number of msec */
/* (Note that OS X 10.9+ App Nap can wreck this, unless */
/*  it is turned off.) */
void msec_sleep(unsigned int msec) {
#ifdef NEVER
	if (msec > 1000) {
		unsigned int secs;
		secs = msec / 1000;
		msec = msec % 1000;
		sleep(secs);
	}
	usleep(msec * 1000);
#else
	struct timespec ts;

	ts.tv_sec = msec / 1000;
	ts.tv_nsec = (msec % 1000) * 1000000;
	nanosleep(&ts, NULL);
#endif
}


#if defined(__APPLE__) /* && !defined(CLOCK_MONOTONIC) */

#include <mach/mach_time.h>

/* Return the current time in msec */
/* since the first invokation of msec_time() */
unsigned int msec_time() {
    mach_timebase_info_data_t timebase;
    static uint64_t startup = 0;
    uint64_t time;
	double msec;

    time = mach_absolute_time();
	if (startup == 0)
		startup = time;

    mach_timebase_info(&timebase);
	time -= startup;
    msec = ((double)time * (double)timebase.numer)/((double)timebase.denom * 1e6);

    return (unsigned int)floor(msec + 0.5);
}

/* Return the current time in usec */
/* since the first invokation of usec_time() */
double usec_time() {
    mach_timebase_info_data_t timebase;
    static uint64_t startup = 0;
    uint64_t time;
	double usec;

    time = mach_absolute_time();
	if (startup == 0)
		startup = time;

    mach_timebase_info(&timebase);
	time -= startup;
    usec = ((double)time * (double)timebase.numer)/((double)timebase.denom * 1e3);

    return usec;
}

#else

/* Return the current time in msec */
/* since the first invokation of msec_time() */
unsigned int msec_time() {
	unsigned int rv;
	static struct timespec startup = { 0, 0 };
	struct timespec cv;

	clock_gettime(CLOCK_MONOTONIC, &cv);

	/* Set time to 0 on first invocation */
	if (startup.tv_sec == 0 && startup.tv_nsec == 0)
		startup = cv;

	/* Subtract, taking care of carry */
	cv.tv_sec -= startup.tv_sec;
	if (startup.tv_nsec > cv.tv_nsec) {
		cv.tv_sec--;
		cv.tv_nsec += 1000000000;
	}
	cv.tv_nsec -= startup.tv_nsec;

	/* Convert nsec to msec */
	rv = cv.tv_sec * 1000 + cv.tv_nsec / 1000000;

	return rv;
}

/* Return the current time in usec */
/* since the first invokation of usec_time() */
double usec_time() {
	double rv;
	static struct timespec startup = { 0, 0 };
	struct timespec cv;

	clock_gettime(CLOCK_MONOTONIC, &cv);

	/* Set time to 0 on first invocation */
	if (startup.tv_sec == 0 && startup.tv_nsec == 0)
		startup = cv;

	/* Subtract, taking care of carry */
	cv.tv_sec -= startup.tv_sec;
	if (startup.tv_nsec > cv.tv_nsec) {
		cv.tv_sec--;
		cv.tv_nsec += 1000000000;
	}
	cv.tv_nsec -= startup.tv_nsec;

	/* Convert to usec */
	rv = cv.tv_sec * 1000000.0 + cv.tv_nsec/1000;

	return rv;
}

#endif

#endif /* UNIX */

/*******************************/
/* Debug convenience functions */
/*******************************/

#define DEB_MAX_CHAN 24
#define DEB_NO_BUFS 10

/* The buffer re-use arrangement isn't thread safe, but will */
/* work a alot of the time */ 

/* Print an int vector to a string. */
/* Returned static buffer is re-used every DEB_NO_BUFS calls. */
char *debPiv(int di, int *p) {
	static char buf[DEB_NO_BUFS][DEB_MAX_CHAN * 16];
	static int ix = 0;
	int e;
	char *bp;

	if (p == NULL)
		return "(null)";

	if (++ix >= DEB_NO_BUFS)
		ix = 0;
	bp = buf[ix];

	if (di > DEB_MAX_CHAN)
		di = DEB_MAX_CHAN;		/* Make sure that buf isn't overrun */

	for (e = 0; e < di; e++) {
		if (e > 0)
			*bp++ = ' ';
		sprintf(bp, "%d", p[e]); bp += strlen(bp);
	}
	return buf[ix];
}

/* Print a double color vector to a string with format. */
/* Returned static buffer is re-used every 5 calls. */
char *debPdvf(int di, char *fmt, double *p) {
	static char buf[DEB_NO_BUFS][DEB_MAX_CHAN * 50];
	static int ix = 0;
	int e;
	char *bp;

	if (p == NULL)
		return "(null)";

	if (fmt == NULL)
		fmt = "%.8f";

	if (++ix >= DEB_NO_BUFS)
		ix = 0;
	bp = buf[ix];

	if (di > DEB_MAX_CHAN)
		di = DEB_MAX_CHAN;		/* Make sure that buf isn't overrun */

	for (e = 0; e < di; e++) {
		if (e > 0)
			*bp++ = ' ';
		sprintf(bp, fmt, p[e]); bp += strlen(bp);
	}
	return buf[ix];
}

/* Print a double color vector to a string. */
/* Returned static buffer is re-used every 5 calls. */
char *debPdv(int di, double *p) {
	return debPdvf(di, NULL, p);
}

/* Print a float color vector to a string. */
/* Returned static buffer is re-used every 5 calls. */
char *debPfv(int di, float *p) {
#   define BUFSZ (DEB_MAX_CHAN * 50)
	char *fmt = "%.8f";
	static char buf[DEB_NO_BUFS][BUFSZ];
	static int ix = 0;
	int brem = BUFSZ;
	int e;
	char *bp;

	if (p == NULL)
		return "(null)";

	if (++ix >= DEB_NO_BUFS)
		ix = 0;
	bp = buf[ix];

	for (e = 0; e < di && brem > 10; e++) {
		int tt;
		if (e > 0)
			*bp++ = ' ', brem--;
		tt = snprintf(bp, brem, fmt, p[e]);
		if (tt < 0 || tt >= brem)
			break;			/* Run out of room... */
		bp += tt;
		brem -= tt;
	}
	return buf[ix];
#   undef BUFSZ
}

#undef DEB_MAX_CHAN

/*******************************************/
/* In case system doesn't have an implementation */

double gamma_func(double x) {
	static double cvals[12] = {
	  2.5066282746310002, 198580.06271387736, -696538.00715380255, 984524.69720040925, 
	  -719481.38054635737, 290262.7541092608, -64035.016015929359, 7201.8644207650395, 
	  -354.97463894564885, 5.6610056376747284, -0.01474384952133102, 7.4908560087605962e-007 };
	double rv;
	int i;
 
	rv = cvals[0];
	for(i = 1; i < 12; i++)
		rv += cvals[i]/(x + i);
	rv *= exp(-(x + 12)) * pow(x + 12, x + 0.5);

	return rv/x;
}

/*******************************************/
/* Dev. diagnostic logging to a file. */

#ifdef NT
# define LOGFILE "C:/Users/Public/log.txt"
#else
# define LOGFILE "~/log.txt"
#endif

FILE *a_diag_fp = NULL;

void a_diag_log(char *fmt, ...) {
	va_list args;

	if (a_diag_fp == NULL)
		a_diag_fp = fopen(LOGFILE, "w");

	if (a_diag_fp == NULL)
		return;

	va_start(args, fmt);
	vfprintf(a_diag_fp, fmt, args);
	va_end(args);
	fflush(a_diag_fp);
}

/*******************************************/

