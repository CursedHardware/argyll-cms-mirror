
 /* Platform isolation convenience functions */

/* 
 * Argyll Color Management System
 *
 * Author: Graeme W. Gill
 * Date:   28/9/97
 *
 * Copyright 1997 - 2013 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

#define USE_BEGINTHREAD		/* [def] hyper-threading doesn't work on VC++6 otherwise. */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>

#ifdef NT
/* Set minimum OS target as XP */
# if !defined(WINVER) || WINVER < 0x0501
#  if defined(WINVER) 
#   undef WINVER
#  endif
#  define WINVER 0x0501
# endif
# if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0501
#  if defined(_WIN32_WINNT) 
#   undef _WIN32_WINNT
#  endif
#  define _WIN32_WINNT 0x0501
# endif
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#include <conio.h>
#include <tlhelp32.h>
#include <direct.h>
#include <process.h>
#endif

#if defined(UNIX)
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <pwd.h>

/* select() defined, but not poll(), so emulate poll() */
#if defined(FD_CLR) && !defined(POLLIN)
#include "pollem.h"
#define poll_x pollem
#else
#include <sys/poll.h>	/* Else assume poll() is native */
#define poll_x poll
#endif
#endif

#ifndef SALONEINSTLIB
#include "copyright.h"
#include "aconfig.h"
#else
#include "sa_config.h"
#endif
#include "numsup.h"
#include "cgats.h"
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"

#ifdef UNIX_APPLE
//#include <stdbool.h>
#include <sys/sysctl.h>
#include <sys/param.h>
#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/IOBSD.h>
#include <mach/mach_init.h>
#include <mach/task_policy.h>
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1050
# include <AudioToolbox/AudioServices.h>
#endif
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
# include <objc/objc-auto.h>
#endif
#endif /* UNIX_APPLE */

#undef DEBUG

#ifdef DEBUG
# define DBG(xx)	a1logd(g_log, 0, xx )
# define DBGA g_log, 0 		/* First argument to DBGF() */
# define DBGF(xx)	a1logd xx
#else
# define DBG(xx)
# define DBGF(xx)
#endif

#ifdef __BORLANDC__
#define _kbhit kbhit
#endif

/* ============================================================= */
/*                           MS WINDOWS                          */  
/* ============================================================= */
#ifdef NT

/* Get the next console character. Return 0 if none is available. */
/* Wait for one if wait is nz */
/* (If not_interactive set, return next stdin character if available, but discard cr or lf) */
static int con_char(int wait) {

//fprintf(stderr,"~1 con_char not_interactive %d wait %d\n",not_interactive,wait);

	if (not_interactive) {		/* Can't assume that it's the console */
		HANDLE stdinh;
		char buf[10] = { 0 };
		DWORD bread;
		int i, rv = 0;

		if ((stdinh = GetStdHandle(STD_INPUT_HANDLE)) == INVALID_HANDLE_VALUE) {
			return 0;		
		}

		if (stdin_type == FILE_TYPE_CHAR) {
//fprintf(stderr,"~1 wait %d console:\n",wait);

			if (wait || _kbhit() != 0) {
				for (bread = 0; bread < 10;)  {
					int c = _getch();
					buf[bread++] = c;
//fprintf(stderr,"~1  read 0x%x\n" ,c);
					if (c == '\n' || c == '\r' || c == 0x3) {
						break;
					}
				}
//fprintf(stderr,"~1  read %d: ",bread);
//for (i = 0; i < bread; i++)
//fprintf(stderr," 0x%x",buf[i]);
//fprintf(stderr,"\n");
				if (bread > 0) {
					rv = buf[0];
				}
			}

		/* We assume pipe has been set to NOWAIT mode. */
		} else if (stdin_type == FILE_TYPE_PIPE) {
			int i, bib;
//fprintf(stderr,"~1  top of pipe\n");

			for (bib = 0; bib < 10;) {
//fprintf(stderr,"~1  got %d in buf\n",bib);
				if ((!ReadFile(stdinh, buf + bib, 10 - bib, &bread, NULL) || bread == 0)
				 && !wait) {
//fprintf(stderr,"~1  no chars waiting\n");
					break;
				}
				bib += bread;

				for (i = 0; i < bib; i++) {
					if (buf[i] == '\n' || buf[i] == '\r' || buf[i] == 0x3) {
//fprintf(stderr,"~1  found lf at ix %d\n",i);
						break;
					}
				}
				if (i < bib) {
//fprintf(stderr,"~1  found lf\n");
					break;		/* Found '\n' */
				}
				Sleep(100);		/* Wait for a line ending in '\n' */
			}

//if (bread > 0) {
//fprintf(stderr,"~1  read %d: ",bread);
//for (i = 0; i < bread; i++)
//fprintf(stderr," 0x%x",buf[i]);
//fprintf(stderr,"\n//");
//}
			rv = buf[0];

		/* Assume a file. This will have very limited functionality. */
		/* We assume that we can't poll for console input, but will */
		/* read from the file on blocking calls. */
		} else {
			if (wait) {		/* Only attempt a read if this is blocking */
				if (ReadFile(stdinh, buf, 10, &bread, NULL) && bread > 0)
					rv = buf[0];
			}
		}

//fprintf(stderr,"~1 returning 0x%x\n",rv);
		return rv;
	}

	/* Assume it's the interactive console */
	if (wait || _kbhit() != 0) {
		int c = _getch();
		return c;
	}
	return 0; 
}

/* wait for and then return the next character from the keyboard */
/* (If not_interactive set, wait for next stdin character but discard cr or lf) */
int next_con_char(void) {
	return con_char(1);
}

/* If there is one, return the next character from the keyboard, else return 0 */
/* (If not_interactive set, return next stdin character if available, but discard cr or lf) */
int poll_con_char(void) {
	return con_char(0); 
}

/* If interactive, suck all characters from the keyboard */
void empty_con_chars(void) {

	if (not_interactive)		/* Don't suck all the characters from stdin */
		return;

	Sleep(50);					/* _kbhit seems to have a bug */
	while (_kbhit()) {
		if (next_con_char() == 0x3)	/* ^C Safety */
			break;
	}
}

/* Do an fgets from stdin, taking account of possible interference from */
/* non-interactive mode. */
char *con_fgets(char *buf, int size) {

	if (not_interactive) {
		char *rv = NULL;
		HANDLE stdinh;
		int i, bib;
//fprintf(stderr,"~1  top of pipe\n");

		if ((stdinh = GetStdHandle(STD_INPUT_HANDLE)) == INVALID_HANDLE_VALUE) {
			return NULL;		
		}

		for (bib = 0; bib < size;) {
			DWORD bread;

			if (ReadFile(stdinh, buf + bib, size - bib, &bread, NULL) && bread > 0) {
				bib += bread;

				for (i = 0; i < bib; i++) {
					if (buf[i] == '\n' || buf[i] == '\r') {
						break;
					}
				}
				if (i < bib) {
					buf[i] = '\000';
					break;		/* Found '\n' */
				}
			}
			Sleep(100);		/* Wait for a line ending in '\n' */
	
		}
		return buf;
	}

	return fgets(buf, size, stdin);
}

/* Sleep for the given number of seconds */
void sleep(unsigned int secs) {
	Sleep(secs * 1000);
}

static athread *beep_thread = NULL;
static int beep_delay;
static int beep_freq;
static int beep_msec;

/* Delayed beep handler */
static int delayed_beep(void *pp) {
	msec_sleep(beep_delay);
	a1logd(g_log,8, "msec_beep activate\n");
	Beep(beep_freq, beep_msec);
	return 0;
}

/* Activate the system beeper */
void msec_beep(int delay, int freq, int msec) {
	a1logd(g_log,8, "msec_beep %d msec\n",msec);
	if (delay > 0) {
		if (beep_thread != NULL)
			beep_thread->del(beep_thread);
		beep_delay = delay;
		beep_freq = freq;
		beep_msec = msec;
		if ((beep_thread = new_athread(delayed_beep, NULL)) == NULL)
			a1logw(g_log, "msec_beep: Delayed beep failed to create thread\n");
	} else {
		a1logd(g_log,8, "msec_beep activate\n");
		Beep(freq, msec);
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Hack to deal with a possibly statically initilized amutex on MSWin. */
/* We statically initilize the CRITICAL_SECTION with a sentinel */
/* LockCount value, and if this value is set, use an atomic */
/* lock to do the initilization before any other operation on it. */
/* (A more robust trick may be to declare the amutex as a pointer */
/*  and create it if it is NULL using a InterlockedCompareExchange()) */
int amutex_chk(CRITICAL_SECTION *lock) {

	if (lock->LockCount == amutex_static_LockCount) {		/* Not initialized */
		static volatile LONG ilock = 0;

		/* Try ilock */
//		if (InterlockedCompareExchange((LONG **)&ilock, (LONG *)1, (LONG *)0) == 0)
		if (InterlockedCompareExchange(&ilock, (LONG)1, (LONG)0) == 0)
		{
			/* We locked it */
			if (lock->LockCount == amutex_static_LockCount) {	/* Still not inited */
				InitializeCriticalSection(lock);				/* So we init it */
			}
			ilock = 0;		/* We're done */
		} else {			/* Wait for other thread to finish init */
			while(ilock != 0) {
				msec_sleep(1);
			}
		}
	}
	return 0;
}


int acond_timedwait_imp(HANDLE cond, CRITICAL_SECTION *lock, int msec) {
	int rv;
	LeaveCriticalSection(lock);
	rv = WaitForSingleObject(cond, msec);
	EnterCriticalSection(lock);

	if (rv == WAIT_TIMEOUT)
		return 1;

	if (rv != WAIT_OBJECT_0)
		return 2;

	return 0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifdef NEVER    /* Not currently needed, or effective */

/* Set the current threads priority */
int set_interactive_priority() {
	if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST) == 0)
		return 1;
	return 0;
}

int set_normal_priority() {
	if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL) == 0)
		return 1;
	return 0;
}

#endif /* NEVER */


/* If reusable, start a stopped thread. NOP if not reusable */
static void athread_start(
athread *p
) {
	DBG("athread_start called\n");
	if (!p->reusable)
		return;

	/* Signal to the thread that it should start */
	amutex_lock(p->startm);
	p->startv = 1;
	acond_signal(p->startc);
	amutex_unlock(p->startm);
}

/* If reusable, wait for the thread to stop. Return the result. NOP if not reusable */
static int athread_wait_stop(
athread *p
) {
	DBG("athread_wait_stop called\n");
	if (!p->reusable)
		return p->result;

	/* Wait for thread to stop */
	amutex_lock(p->stopm);
	while (p->stopv == 0)
		acond_wait(p->stopc, p->stopm);
	p->stopv = 0;
	amutex_unlock(p->stopm);

	return p->result;
}

/* Wait for the thread to exit. Return the result */
static int athread_wait(struct _athread *p) {

	if (p->reusable) {
		p->dofinish = 1;
		athread_start(p);	/* Wake thread up to cause it to exit */
	}

	if (p->joined)
		return p->result;

	WaitForSingleObject(p->th, INFINITE);
	p->joined = 1;

	return p->result;
}

/* Forcefully terminate the thread */
static void athread_terminate(
athread *p
) {
	DBG("athread_terminate called\n");

	if (p == NULL || p->joined)
		return;

	if (p->th != NULL) {
		DBG("athread_del calling TerminateThread()\n");
		TerminateThread(p->th, (DWORD)-1);
//		WaitForSingleObject(p->th, INFINITE);	// Don't join in case it doesn't terminate	
	}
	p->joined = 1;		/* Skip any join afterwards */
}

/* Free the thread */
static void athread_del(
athread *p
) {
	DBG("athread_del called\n");

	if (p == NULL)
		return;

	if (p->th != NULL) {
		if (!p->joined)
			WaitForSingleObject(p->th, INFINITE);
		CloseHandle(p->th);
	}

	if (p->reusable) {
		acond_del(p->startc);
		amutex_del(p->startm);
		acond_del(p->stopc);
		amutex_del(p->stopm);
	}

	free(p);
}

/* _beginthreadex inits the CRT properly, wheras CreateThread doesn't on VC++6 */ 
#ifdef USE_BEGINTHREAD
/* Thread function */
static unsigned int __stdcall threadproc(
	void *lpParameter
) {
#else
DWORD WINAPI threadproc(
	LPVOID lpParameter
) {
#endif
	athread *p = (athread *)lpParameter;

	if (p->reusable) {		/* Run over and over */
		for (;;) {

			/* Wait for client to start us */
			amutex_lock(p->startm);
			while (p->startv == 0)
				acond_wait(p->startc, p->startm);
			p->startv = 0;
			amutex_unlock(p->startm);

			if (p->dofinish)
				break;

			p->result = p->function(p->context);

			if (p->dofinish)
				break;

			/* Signal to the client that we've stopped */
			amutex_lock(p->stopm);
			p->stopv = 1;
			acond_signal(p->stopc);
			amutex_unlock(p->stopm);
		}

	} else {
		p->result = p->function(p->context);
	}
//	p->finished = 1;
	return 0;
}
 
athread *new_athread_reusable(
	int (*function)(void *context),
	void *context,
	int reusable
) {
	athread *p = NULL;

	DBG("new_athread called\n");

	if ((p = (athread *)calloc(sizeof(athread), 1)) == NULL) {
		a1loge(g_log, 1, "new_athread: calloc failed\n");
		return NULL;
	}

	p->reusable = reusable;
	if (p->reusable) {
		amutex_init(p->startm);
		p->startv = 0;
		acond_init(p->startc);

		amutex_init(p->stopm);
		p->stopv = 0;
		acond_init(p->stopc);
	}

	p->function = function;
	p->context = context;
	p->start = athread_start;
	p->wait_stop = athread_wait_stop;
	p->wait = athread_wait;
	p->terminate = athread_terminate;
	p->del = athread_del;

	/* Create a thread */
#ifdef USE_BEGINTHREAD
	p->th = (HANDLE)_beginthreadex(NULL, 0, threadproc, (void *)p, 0, NULL);
	if (p->th == (HANDLE)-1L) {
#else
	p->th = CreateThread(NULL, 0, threadproc, (void *)p, 0, NULL);
	if (p->th == NULL) {
#endif
		a1loge(g_log, 1, "new_athread: CreateThread failed with %d\n",GetLastError());
		p->th = NULL;
		athread_del(p);		/* Cleanup any resources */
		return NULL;
	}

	DBG("new_athread returning OK\n");
	return p;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Return the login $HOME directory. */
/* (Useful if we might be running sudo) */
/* No NT equivalent ?? */
char *login_HOME() {
	return getenv("HOME");
}

/* Delete a file */
void delete_file(char *fname) {
	_unlink(fname);
}

/* Given the path to a file, ensure that all the parent directories */
/* are created. return nz on error */
int create_parent_directories(char *path) {
	struct _stat sbuf;
	char *pp = path;

	if (*pp != '\000'		/* Skip drive number */
		&& ((*pp >= 'a' && *pp <= 'z') || (*pp >= 'A' && *pp <= 'Z'))
	    && pp[1] == ':')
		pp += 2;
	if (*pp == '/')
		pp++;			/* Skip root directory */
	for (;pp != NULL && *pp != '\000';) {
		if ((pp = strchr(pp, '/')) != NULL) {
			*pp = '\000';
			if (_stat(path,&sbuf) != 0)
			{
				if (_mkdir(path) != 0)
					return 1;
			}
			*pp = '/';
			pp++;
		}
	}
	return 0;
}

/* return the number of processors */
int system_processors() {
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;
}

#endif /* NT */

/* Do a string copy while replacing all '\' characters with '/' */
void copynorm_dirsep(char *d, char *s) { 
#ifdef NT
	for (;;) {
		*d = *s;
		if (*s == '\000')
			break;
		if (*d == '\\')
			*d = '/';
		s++;
		d++;
	}
#endif
}

/* Allocate and create a path to the given filename that is */
/* in the same directory as the given file. */
/* Returns normalized separator '/' path. */
/* Free after use */
/* Return NULL on malloc error */
char *path_to_file_in_same_dir(char *inpath, char *infile) {
	size_t alen = 0;
	char *rv = NULL, *cp;

	/* Be very conservative */
	alen = strlen(inpath) + strlen(infile) + 1;

	if ((rv = malloc(alen)) == NULL) {
		return NULL;
	}

	copynorm_dirsep(rv, inpath);
	
	/* Locate the base filename in path */
	if ((cp = strrchr(rv, '/')) == NULL) {
		strcpy(rv, infile);
	} else {
		cp++;
		strcpy(cp, infile);
	}

	return rv;
}


/* ============================================================= */
/*                          UNIX/OS X                            */
/* ============================================================= */

#if defined(UNIX)

/* Wait for and return the next character from the keyboard */
/* (If not_interactive set, wait for next stdin character but discard cr or lf) */
int next_con_char(void) {
	struct pollfd pa[1];		/* Poll array to monitor stdin */
	struct termios origs, news;
	char rv = 0;

	/* Configure stdin to be ready with just one character if interactive */
	if (!not_interactive) {
		if (tcgetattr(STDIN_FILENO, &origs) < 0)
			a1logw(g_log, "next_con_char: tcgetattr failed with '%s' on stdin", strerror(errno));
		news = origs;
		news.c_lflag &= ~(ICANON | ECHO);
		news.c_cc[VTIME] = 0;
		news.c_cc[VMIN] = 1;
		if (tcsetattr(STDIN_FILENO,TCSANOW, &news) < 0)
			a1logw(g_log, "next_con_char: tcsetattr failed with '%s' on stdin", strerror(errno));
	}

	/* Wait for stdin to have at least one character. */
	/* If not_interactive set then it may be a character followed by cr and/or lf to flush it */
	pa[0].fd = STDIN_FILENO;
	pa[0].events = POLLIN | POLLPRI;
	pa[0].revents = 0;

	if (poll_x(pa, 1, -1) > 0					/* wait until there is something */
	 && (pa[0].revents == POLLIN
		 || pa[0].revents == POLLPRI)) {		/* Something there */
		char tb[10];
		if (read(STDIN_FILENO, tb, 3) > 0) {	/* get it and any return */
			rv = tb[0];
		}
	} else {
		a1logw(g_log, "next_con_char: poll on stdin returned unexpected value 0x%x",pa[0].revents);
	}

	/* Restore stdin if interactive */
	if (!not_interactive && tcsetattr(STDIN_FILENO, TCSANOW, &origs) < 0) {
		a1logw(g_log, "next_con_char: tcsetattr failed with '%s' on stdin", strerror(errno));
	}

	return rv;
}

/* If here is one, return the next character from the keyboard, else return 0 */
/* (If not_interactive set, return next stdin character if available, but discard cr or lf) */
int poll_con_char(void) {
	struct pollfd pa[1];		/* Poll array to monitor stdin */
	struct termios origs, news;
	char rv = 0;

	/* Configure stdin to be ready with just one character if interactive */
	if (!not_interactive) {
		if (tcgetattr(STDIN_FILENO, &origs) < 0)
			a1logw(g_log, "poll_con_char: tcgetattr failed with '%s' on stdin", strerror(errno));
		news = origs;
		news.c_lflag &= ~(ICANON | ECHO);
		news.c_cc[VTIME] = 0;
		news.c_cc[VMIN] = 1;
		if (tcsetattr(STDIN_FILENO,TCSANOW, &news) < 0)
			a1logw(g_log, "poll_con_char: tcsetattr failed with '%s' on stdin", strerror(errno));
	}

	/* See if stdin has a character */
	/* If not_interactive set then it may be a character followed by cr and/or lf to flush it */
	pa[0].fd = STDIN_FILENO;
	pa[0].events = POLLIN | POLLPRI;
	pa[0].revents = 0;

	if (poll_x(pa, 1, 0) > 0					/* don't wait if there is nothing */
	 && (pa[0].revents == POLLIN
		 || pa[0].revents == POLLPRI)) {		/* Something is there */
		char tb[10];
		if (read(STDIN_FILENO, tb, 3) > 0) {	/* Get it and any return */
			rv = tb[0];
		}
	}

	/* Restore stdin if interactive */
	if (!not_interactive && tcsetattr(STDIN_FILENO, TCSANOW, &origs) < 0)
		a1logw(g_log, "poll_con_char: tcsetattr failed with '%s' on stdin", strerror(errno));

	return rv;
}

/* Discard all pending characters from stdin */
void empty_con_chars(void) {

	if (not_interactive)		/* Don't suck all the characters from stdin */
		return;

	tcflush(STDIN_FILENO, TCIFLUSH);
}

/* Do an fgets from stdin, taking account of possible interference from */
/* non-Interactive mode. */
char *con_fgets(char *s, int size) {
	return fgets(s, size, stdin);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

int acond_timedwait_imp(pthread_cond_t *cond, pthread_mutex_t *lock, int msec) {
	struct timeval tv;
	struct timespec ts;
	int rv;

	// this is unduly complicated...
	gettimeofday(&tv, NULL);
	ts.tv_sec = tv.tv_sec + msec/1000;
	ts.tv_nsec = (tv.tv_usec + (msec % 1000) * 1000) * 1000L;
	if (ts.tv_nsec > 1000000000L) {
		ts.tv_nsec -= 1000000000L;
		ts.tv_sec++;
	}

	rv = pthread_cond_timedwait(cond, lock, &ts);

	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

#ifdef NEVER    /* Not currently needed, or effective */

/* Set the current threads priority */
int set_interactive_priority() {
#ifdef UNIX_APPLE
#ifdef NEVER
    int rv = 0;
    struct task_category_policy tcatpolicy;

    tcatpolicy.role = TASK_FOREGROUND_APPLICATION;

    if (task_policy_set(mach_task_self(),
        TASK_CATEGORY_POLICY, (thread_policy_t)&tcatpolicy,
        TASK_CATEGORY_POLICY_COUNT) != KERN_SUCCESS)
		rv = 1;
//		a1logd(g_log, 8, "set_interactive_priority: set to forground got %d\n",rv);
	return rv;
#else
    int rv = 0;
    struct thread_precedence_policy tppolicy;

    tppolicy.importance = 500;

    if (thread_policy_set(mach_thread_self(),
        THREAD_PRECEDENCE_POLICY, (thread_policy_t)&tppolicy,
        THREAD_PRECEDENCE_POLICY_COUNT) != KERN_SUCCESS)
		rv = 1;
//		a1logd(g_log, 8, "set_interactive_priority: set to important got %d\n",rv);
	return rv;
#endif /* NEVER */
#else /* !UNIX_APPLE */
	int rv;
	struct sched_param param;
	param.sched_priority = 32;

	/* This doesn't work unless we're running as su :-( */
	rv = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
//		a1logd(g_log, 8, "set_interactive_priority: set got %d\n",rv);
	return rv;
#endif /* !UNIX_APPLE */
}

int set_normal_priority() {
#ifdef UNIX_APPLE
#ifdef NEVER
    int rv = 0;
    struct task_category_policy tcatpolicy;

    tcatpolicy.role = TASK_UNSPECIFIED;

    if (task_policy_set(mach_task_self(),
        TASK_CATEGORY_POLICY, (thread_policy_t)&tcatpolicy,
        TASK_CATEGORY_POLICY_COUNT) != KERN_SUCCESS)
		rev = 1;
//		a1logd(g_log, 8, "set_normal_priority: set to normal got %d\n",rv);
#else
    int rv = 0;
    struct thread_precedence_policy tppolicy;

    tppolicy.importance = 1;

    if (thread_policy_set(mach_thread_self(),
        THREAD_STANDARD_POLICY, (thread_policy_t)&tppolicy,
        THREAD_STANDARD_POLICY_COUNT) != KERN_SUCCESS)
		rv = 1;
//		a1logd(g_log, 8, "set_normal_priority: set to standard got %d\n",rv);
	return rv;
#endif /* NEVER */
#else /* !UNIX_APPLE */
	struct sched_param param;
	param.sched_priority = 0;
	int rv;

	rv = pthread_setschedparam(pthread_self(), SCHED_OTHER, &param);
//		a1logd(g_log, 8, "set_normal_priority: reset got %d\n",rv);
	return rv;
#endif /* !UNIX_APPLE */
}

#endif /* NEVER */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static athread *beep_thread = NULL;
static int beep_delay;
static int beep_freq;
static int beep_msec;

/* Delayed beep handler */
static int delayed_beep(void *pp) {
	msec_sleep(beep_delay);
	a1logd(g_log,8, "msec_beep activate\n");
#ifdef UNIX_APPLE
# if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1050
	AudioServicesPlayAlertSound(kUserPreferredAlert);
# else
	SysBeep((beep_msec * 60)/1000);
# endif
#else	/* UNIX */
	/* Linux is pretty lame in this regard... */
	/* Maybe we could write an 8Khz 8 bit sample to /dev/dsp, or /dev/audio ? */
	/* The ALSA system is the modern way for audio output. */
	/* Also check out what sox does: <http://sox.sourceforge.net/> */
	fprintf(stdout, "\a"); fflush(stdout);
#endif 
	return 0;
}

/* Activate the system beeper */
void msec_beep(int delay, int freq, int msec) {
	a1logd(g_log,8, "msec_beep %d msec\n",msec);
	if (delay > 0) {
		if (beep_thread != NULL)
			beep_thread->del(beep_thread);
		beep_delay = delay;
		beep_freq = freq;
		beep_msec = msec;
		if ((beep_thread = new_athread(delayed_beep, NULL)) == NULL)
			a1logw(g_log, "msec_beep: Delayed beep failed to create thread\n");
	} else {
		a1logd(g_log,8, "msec_beep activate\n");
#ifdef UNIX_APPLE
# if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1050
			AudioServicesPlayAlertSound(kUserPreferredAlert);
# else
			SysBeep((msec * 60)/1000);
# endif
#else	/* UNIX */
		fprintf(stdout, "\a"); fflush(stdout);
#endif
	}
}


#ifdef NEVER
/* If we're UNIX and running on X11, we could do this */
/* sort of thing (from xset) to sound a beep, */
/* IF we were linking with X11: */

static void
set_bell_vol(Display *dpy, int percent) {
	XKeyboardControl values;
	XKeyboardState kbstate;
	values.bell_percent = percent;
	if (percent == DEFAULT_ON)
	  values.bell_percent = SERVER_DEFAULT;
	XChangeKeyboardControl(dpy, KBBellPercent, &values);
	if (percent == DEFAULT_ON) {
	  XGetKeyboardControl(dpy, &kbstate);
	  if (!kbstate.bell_percent) {
	    values.bell_percent = -percent;
	    XChangeKeyboardControl(dpy, KBBellPercent, &values);
	  }
	}
	return;
}

static void
set_bell_pitch(Display *dpy, int pitch) {
	XKeyboardControl values;
	values.bell_pitch = pitch;
	XChangeKeyboardControl(dpy, KBBellPitch, &values);
	return;
}

static void
set_bell_dur(Display *dpy, int duration) {
	XKeyboardControl values;
	values.bell_duration = duration;
	XChangeKeyboardControl(dpy, KBBellDuration, &values);
	return;
}

XBell(..);

#endif /* NEVER */

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* If reusable, start a stopped thread. NOP if not reusable */
static void athread_start(
athread *p
) {
	DBG("athread_start called\n");
	if (!p->reusable)
		return;

	/* Signal to the thread that it should start */
	amutex_lock(p->startm);
	p->startv = 1;
	acond_signal(p->startc);
	amutex_unlock(p->startm);
}

/* If reusable, wait for the thread to stop. Return the result. NOP if not reusable */
static int athread_wait_stop(
athread *p
) {
	DBG("athread_wait_stop called\n");
	if (!p->reusable)
		return p->result;

	/* Wait for thread to stop */
	amutex_lock(p->stopm);
	while (p->stopv == 0)
		acond_wait(p->stopc, p->stopm);
	p->stopv = 0;
	amutex_unlock(p->stopm);

	return p->result;
}

/* Wait for the thread to exit. Return the result */
static int athread_wait(struct _athread *p) {
	int rv;

	if (p->reusable) {
		p->dofinish = 1;
		athread_start(p);	/* Wake thread up to cause it to exit */
	}

	if (p->joined)
		return p->result;

    if ((rv = pthread_join(p->thid, NULL)) != 0) 
		warning("pthread_join of thid %d failed with %d",p->thid,rv);
	p->joined = 1;

	return p->result;
}

/* Forcefully terminate the thread */
static void athread_terminate(
athread *p
) {
	DBG("athread_terminate called\n");

	if (p == NULL || p->joined)
		return;

	if (p->thid != (pthread_t)0) {
		DBG("athread_terminate calling pthread_cancel()\n");
		pthread_cancel(p->thid);
//	    pthread_join(p->thid, NULL);	// Don't join in case it doesn't terminate
	}
	p->joined = 1;		/* Skip any join afterwards */
}

/* Free up thread resources */
static void athread_del(
athread *p
) {
	DBG("athread_del called\n");

	if (p == NULL)
		return;

	if (p->thid != 0 && !p->joined) {
		int rv;
	    if ((rv = pthread_join(p->thid, NULL)) != 0) 
			warning("pthread_join of thid %d failed with %d",p->thid,rv);
	}

	if (p->reusable) {
		acond_del(p->startc);
		amutex_del(p->startm);
		acond_del(p->stopc);
		amutex_del(p->stopm);
	}

	free(p);
}

static void *threadproc(
	void *param
) {
	athread *p = (athread *)param;

	/* Register this thread with the Objective-C garbage collector */
	/* (Hmm. Done by default in latter versions though, hence deprecated in them ?) */
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
	 objc_registerThreadWithCollector();
#endif

	if (p->reusable) {		/* Run over and over */
		for (;;) {

			/* Wait for client to start us */
			amutex_lock(p->startm);
			while (p->startv == 0)
				acond_wait(p->startc, p->startm);
			p->startv = 0;
			amutex_unlock(p->startm);

			if (p->dofinish)		/* If we've been woken up so we can exit */
				break;

			p->result = p->function(p->context);

			/* Signal to the client that we've stopped */
			amutex_lock(p->stopm);
			p->stopv = 1;
			acond_signal(p->stopc);
			amutex_unlock(p->stopm);
		}

	} else {
		p->result = p->function(p->context);
	}
//	p->finished = 1;

	return 0;
}
 

athread *new_athread_reusable(
	int (*function)(void *context),
	void *context,
	int reusable
) {
	int rv;
	athread *p = NULL;

	DBG("new_athread called\n");

	if ((p = (athread *)calloc(sizeof(athread), 1)) == NULL) {
		a1loge(g_log, 1, "new_athread: calloc failed\n");
		return NULL;
	}

	p->reusable = reusable;
	if (p->reusable) {
		amutex_init(p->startm);
		p->startv = 0;
		acond_init(p->startc);

		amutex_init(p->stopm);
		p->stopv = 0;
		acond_init(p->stopc);
	}

	p->function = function;
	p->context = context;
	p->start = athread_start;
	p->wait_stop = athread_wait_stop;
	p->wait = athread_wait;
	p->del = athread_del;

#if defined(UNIX_APPLE)
	{
		pthread_attr_t stackSzAtrbt;

		/* Default stack size is 512K - this is a bit small - raise it to 4MB */
		if ((rv = pthread_attr_init(&stackSzAtrbt)) != 0
		 || (rv = pthread_attr_setstacksize(&stackSzAtrbt, 4 * 1024 * 1024)) != 0) {
			fprintf(stderr,"new_athread: thread_attr_setstacksize failed with %d\n",rv);
			return NULL;
		}

		/* Create a thread */
		rv = pthread_create(&p->thid, &stackSzAtrbt, threadproc, (void *)p);
		if (rv != 0) {
			a1loge(g_log, 1, "new_athread: pthread_create failed with %d\n",rv);
			athread_del(p);
			return NULL;
		}
	}
#else	/* !APPLE */

	/* Create a thread */
	rv = pthread_create(&p->thid, NULL, threadproc, (void *)p);
	if (rv != 0) {
		a1loge(g_log, 1, "new_athread: pthread_create failed with %d\n",rv);
		p->thid = 0;
		athread_del(p);
		return NULL;
	}
#endif /* !APPLE */

	DBG("About to exit new_athread()\n");
	return p;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Return the login $HOME directory. */
/* (Useful if we might be running sudo) */
char *login_HOME() {

	if (getuid() == 0) {	/* If we are running as root */
		char *uids;

		if ((uids = getenv("SUDO_UID")) != NULL) {		/* And we sudo's to get it */
			int uid;
			struct passwd *pwd;

			uid = atoi(uids);

			if ((pwd = getpwuid(uid)) != NULL) {
				return pwd->pw_dir;
			}
		}
	}

	return getenv("HOME");
}


/* Delete a file */
void delete_file(char *fname) {
	unlink(fname);
}

/* Given the path to a file, ensure that all the parent directories */
/* are created. return nz on error */
int create_parent_directories(char *path) {
	struct stat sbuf;
	char *pp = path;
	mode_t mode = 0700;		/* Default directory mode */

	if (*pp == '/')
		pp++;			/* Skip root directory */
	for (;pp != NULL && *pp != '\000';) {
		if ((pp = strchr(pp, '/')) != NULL) {
			*pp = '\000';
			if (stat(path,&sbuf) != 0)
			{
				if (mkdir(path, mode) != 0)
					return 1;
			} else
				mode = sbuf.st_mode;
			*pp = '/';
			pp++;
		}
	}
	return 0;
}

#if !defined(UNIX_APPLE) || __MAC_OS_X_VERSION_MAX_ALLOWED >= 1040

/* return the number of processors */
int system_processors() {
	return sysconf(_SC_NPROCESSORS_ONLN);
}

#else	/* OS X < 10.4 code */

/* return the number of processors using BSD code */
int system_processors() {
	int mib[4];
	int numCPU;
	size_t len = sizeof(numCPU); 

	mib[0] = CTL_HW;
	mib[1] = HW_AVAILCPU; 

	sysctl(mib, 2, &numCPU, &len, NULL, 0);

	if (numCPU < 1) {
	    mib[1] = HW_NCPU;
		sysctl(mib, 2, &numCPU, &len, NULL, 0);
		if (numCPU < 1)
			numCPU = 1;
	}

	return numCPU;
}

#endif

#endif /* defined(UNIX) */

/* - - - - - - - - - - - - - - - - - - - - - - - - */
#if defined(UNIX_APPLE) || defined(NT)

/* Thread to monitor and kill the named processes */
static int th_kkill_nprocess(void *pp) {
	kkill_nproc_ctx *ctx = (kkill_nproc_ctx *)pp;

	/* set result to 0 if it ever succeeds or there was no such process */
	ctx->th->result = -1;
	while(ctx->stop == 0) {
		if (kill_nprocess(ctx->pname, ctx->log) >= 0)
			ctx->th->result = 0;
		msec_sleep(20);			/* Don't hog the CPU */
	}
	ctx->done = 1;

	return 0;
}

static void kkill_nprocess_del(kkill_nproc_ctx *p) {
	int i;

	p->stop = 1;

	DBG("kkill_nprocess del called\n");
	for (i = 0; p->done == 0 && i < 100; i++) {
		msec_sleep(50);
	}

	if (p->done == 0) {			/* Hmm */
		a1logw(p->log,"kkill_nprocess del failed to stop - killing thread\n");
		p->th->del(p->th);
	}

	del_a1log(p->log);
	free(p);

	DBG("kkill_nprocess del done\n");
}

/* Start a thread to constantly kill a process. */
/* Call ctx->del() when done */
kkill_nproc_ctx *kkill_nprocess(char **pname, a1log *log) {
	kkill_nproc_ctx *p;

	DBG("kkill_nprocess called\n");
	if (log != NULL && log->debug >= 8) {
		int i;
		a1logv(log, 8, "kkill_nprocess called with");
		for (i = 0; pname[i] != NULL; i++)
			a1logv(log, 8, " '%s'",pname[i]);
		a1logv(log, 8, "\n");
	}

	if ((p = (kkill_nproc_ctx *)calloc(sizeof(kkill_nproc_ctx), 1)) == NULL) {
		a1loge(log, 1, "kkill_nprocess: calloc failed\n");
		return NULL;
	}

	p->pname = pname;
	p->log = new_a1log_d(log);
	p->del = kkill_nprocess_del;

	if ((p->th = new_athread(th_kkill_nprocess, p)) == NULL) {
		del_a1log(p->log);
		free(p);
		return NULL;
	}
	return p;
}

#ifdef NT

/* Kill a list of named process. */
/* Kill the first process found, then return */
/* return < 0 if this fails. */
/* return 0 if there is no such process */
/* return 1 if a process was killed */
int kill_nprocess(char **pname, a1log *log) {
	PROCESSENTRY32 entry;
	HANDLE snapshot;
	int j;

	/* Get a snapshot of the current processes */
	if ((snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0)) == NULL) {
		return -1;
	}

	while(Process32Next(snapshot,&entry) != FALSE) {
		HANDLE proc;

		if (strcmp(entry.szExeFile, "spotread.exe") == 0
		 && (proc = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID)) != NULL) {
			if (TerminateProcess(proc,0) == 0) {
				a1logv(log, 8, "kill_nprocess: Failed to kill '%s'\n",entry.szExeFile);
			} else {
				a1logv(log, 8, "kill_nprocess: Killed '%s'\n",entry.szExeFile);
			}
			CloseHandle(proc);
		}
		for (j = 0;; j++) {
			if (pname[j] == NULL)	/* End of list */
				break;
			a1logv(log, 8, "kill_nprocess: Checking process '%s' against list '%s'\n",
			                                                  entry.szExeFile,pname[j]);
			if (strcmp(entry.szExeFile,pname[j]) == 0) {
				a1logv(log, 1, "kill_nprocess: killing process '%s' pid %d\n",
				                            entry.szExeFile,entry.th32ProcessID);

				if ((proc = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID)) == NULL
				 || TerminateProcess(proc,0) == 0) {
					a1logv(log, 1, "kill_nprocess: kill process '%s' failed with %d\n",
					                                             pname[j],GetLastError());
					CloseHandle(proc);
					CloseHandle(snapshot);
					return -1;
				}
				CloseHandle(proc);
				/* Stop on first one found ? */
				CloseHandle(snapshot);
				return 1;
			}
		}
	}
	CloseHandle(snapshot);
	return 0;
}

#endif /* NT */

#if defined(UNIX_APPLE)

/* Kill a list of named process. */
/* Kill the first process found, then return */
/* return < 0 if this fails. */
/* return 0 if there is no such process */
/* return 1 if a process was killed */
int kill_nprocess(char **pname, a1log *log) {
	struct kinfo_proc *procList = NULL;
	size_t procCount = 0;
	static int name[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
	size_t length;
	int rv = 0;
	int i, j;

	procList = NULL;
	for (;;) {
		int err;

		/* Establish the amount of memory needed */
		length = 0;
		if (sysctl(name, (sizeof(name) / sizeof(*name)) - 1,
                      NULL, &length,
                      NULL, 0) == -1) {
			DBGF((DBGA,"sysctl #1 failed with %d\n", errno));
			return -1;
		}
	
		/* Add some more entries in case the number of processors changed */
		length += 10 * sizeof(struct kinfo_proc);
		if ((procList = malloc(length)) == NULL) {
			DBGF((DBGA,"malloc failed for %d bytes\n", length));
			return -1;
		}

		/* Call again with memory */
		if ((err = sysctl(name, (sizeof(name) / sizeof(*name)) - 1,
                          procList, &length,
                          NULL, 0)) == -1) {
			DBGF((DBGA,"sysctl #1 failed with %d\n", errno));
			free(procList);
			return -1;
		}
		if (err == 0) {
			break;
		} else if (err == ENOMEM) {
			free(procList);
			procList = NULL;
		}
	}

	procCount = length / sizeof(struct kinfo_proc);

	/* Locate the processes */
	for (i = 0; i < procCount; i++) {
		for (j = 0;; j++) {
			if (pname[j] == NULL)	/* End of list */
				break;
			a1logv(log, 8, "kill_nprocess: Checking process '%s' against list '%s'\n",
			                                         procList[i].kp_proc.p_comm,pname[j]);
			if (strncmp(procList[i].kp_proc.p_comm,pname[j],MAXCOMLEN) == 0) {
				a1logv(log, 1, "kill_nprocess: killing process '%s' pid %d\n",
				                            pname[j],procList[i].kp_proc.p_pid);
				if (kill(procList[i].kp_proc.p_pid, SIGTERM) != 0) {
					a1logv(log, 1, "kill_nprocess: kill process '%s' failed with %d\n",
					                                                    pname[j],errno);
					free(procList);
					return -1;
				}
				/* Stop on first one found ? */
				free(procList);
				return 1;
			}
		}
	}
	free(procList);
	return rv;
}

#endif /* UNIX_APPLE */

#endif /* UNIX_APPLE || NT */

/* ===================================================================== */
/* Some web support */

static char nib2hex(int nib) {
	nib &= 0xf;
	if (nib < 10)
		return '0' + nib;
	else
		return 'A' + nib - 10;
}

/* Destination should be strlen(s) * 3 + 1 */
void encodeurl(char *d, char *s) {
	char *dd = d;
	DBGF((DBGA," encodeurl s = '%s'\n",s));
	for (; *s != '\000'; s++) {
		char c = *s; 

		if (isalnum(c)
		 || c == '~'
		 || c == '-'
		 || c == '.'
		 || c == '_')
			*d++ = c;
		else {
			*d++ = '%';
			*d++ = nib2hex(c >> 4);
			*d++ = nib2hex(c);
		}
	}
	*d++ = '\000';
	DBGF((DBGA," encodeurl d = '%s'\n",dd));
}
 
static int hex2nib(char c) {
	int nib = 0;
	if (c >= '0' && c <= '9')
		nib = c - '0';
	else if (c >= 'A' && c <= 'F')
		nib = c - 'A' + 10;
	else if (c >= 'a' && c <= 'f')
		nib = c - 'a' + 10;
	return nib;
}

/* Destination is smaller than src */
void decodeurl(char *d, char *s) {
	char *dd = d;
	DBGF((DBGA," decodeurl s = '%s'\n",s));
	for (; *s != '\000'; s++) {
//		if (s[0] == '+')
//			*d++ = ' ';
//		else
		if (s[0] == '%' && s[1] != '\000' && s[2] != '\000') {
			*d++ = (hex2nib(s[1]) << 4) + hex2nib(s[2]); 
			s += 2;
		} else
			*d++ = s[0];
	}
	*d++ = '\000';
	DBGF((DBGA," decodeurl d = '%s'\n",dd));
}

/* ===================================================================== */
/* Some compatibility functions */

#if defined(UNIX_APPLE) 

size_t osx_strnlen(const char *string, size_t maxlen) {
  const char *end = memchr(string, '\0', maxlen);
  return end ? end - string : maxlen;
}

char *osx_strndup(const char *s, size_t n) {
	size_t len = osx_strnlen(s, n);
	char *buf = malloc(len + 1);

	if (buf == NULL)
		return NULL;
	buf[len] = '\0';
  	return memcpy(buf, s, len);
}

#endif // APPLE && < 1040
