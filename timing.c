/* Timing routines for computing elapsed wall time */
/* Michael D. Black, Computer Science Innovations */
/* Melbourne, FL.  mblack@csihq.com */

#include "config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "timing.h"

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#if HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef TIME_WITH_SYS_TIME
#  include <time.h>
#  include <sys/time.h>
#elif defined(HAVE_SYS_TIME_H)
#  include <sys/time.h>
#else
   /* can't use gettimeofday without struct timeval */
#  undef HAVE_GETTIMEOFDAY
#  include <time.h>
#endif

/* Prefer gettimeofday to ftime to times.  */
#if defined(HAVE_GETTIMEOFDAY)
#  undef HAVE_FTIME
#  undef HAVE_TIMES
#elif defined(HAVE_FTIME)
#  undef HAVE_TIMES
#endif

#ifdef HAVE_FTIME
#  include <sys/timeb.h>
#endif

#ifdef HAVE_TIMES
#  if HAVE_SYS_TIMES_H
#    include <sys/times.h>
#  endif
#  ifdef _SC_CLK_TCK
#    define HAVE_SC_CLK_TCK 1
#  else
#    define HAVE_SC_CLK_TCK 0
#  endif
/* TIMES_TICK may have been set in policy.h, or we may be able to get
   it using sysconf.  If neither is the case, try to find a useful
   definition from the system header files.  */
#  if !defined(TIMES_TICK) && (!defined(HAVE_SYSCONF) || !defined(HAVE_SC_CLK_TCK))
#    ifdef CLK_TCK
#      define TIMES_TICK CLK_TCK
#    else /* ! defined (CLK_TCK) */
#      ifdef HZ
#        define TIMES_TICK HZ
#      endif /* defined (HZ) */
#    endif /* ! defined (CLK_TCK) */
#else
#  endif /* TIMES_TICK == 0 && (! HAVE_SYSCONF || ! HAVE_SC_CLK_TCK) */
#  ifndef TIMES_TICK
#    define TIMES_TICK 0
#  endif
#endif /* HAVE_TIMES */

#ifdef HAVE_GETTIMEOFDAY
int gettimeofday (struct timeval *tv, struct timezone *tz);
#endif

double 
timing (int reset)
{
  static double elaptime, starttime, stoptime;
  double yet;
#define NEED_TIME
#ifdef HAVE_GETTIMEOFDAY
  struct timeval tv;
  struct timezone tz;

  tz.tz_dsttime = DST_NONE;
  gettimeofday (&tv, &tz);
  yet=tv.tv_sec + tv.tv_usec/1000000.0;
#undef NEED_TIME
#endif
#ifdef HAVE_FTIME
	static int fbad=0;

	if (! fbad)
	{
		struct timeb stime;
		static struct timeb slast;

		(void) ftime (&stime);

		/* On some systems, such as SCO 3.2.2, ftime can go backwards in
		   time.  If we detect this, we switch to using time.  */
		if (slast.time != 0
			&& (stime.time < slast.time
			|| (stime.time == slast.time && stime.millitm < slast.millitm)))
			fbad = 1;
		else
		{
			yet = stime.millitm / 1000.0  + stime.time;
			slast = stime;
			goto doit;
		}
	}
#endif

#ifdef HAVE_TIMES
  struct tms s;
  long i;
  static int itick;

  if (itick == 0)
    {
#if TIMES_TICK == 0
#if HAVE_SYSCONF && HAVE_SC_CLK_TCK
      itick = (int) sysconf (_SC_CLK_TCK);
#else /* ! HAVE_SYSCONF || ! HAVE_SC_CLK_TCK */
      const char *z;

      z = getenv ("HZ");
      if (z != NULL)
        itick = (int) strtol (z, (char **) NULL, 10);

      /* If we really couldn't get anything, just use 60.  */
      if (itick == 0)
        itick = 60;
#endif /* ! HAVE_SYSCONF || ! HAVE_SC_CLK_TCK */
#else /* TIMES_TICK != 0 */
      itick = TIMES_TICK;
#endif /* TIMES_TICK == 0 */
    }
  yet = ((double) times (&s)) / itick;
#undef NEED_TIME
#endif

#ifdef NEED_TIME
	yet=(double) time(NULL);
#endif
doit:
  if (reset) {
    starttime = yet;
    return starttime;
  }
  else {
    stoptime = yet;
    elaptime = stoptime - starttime;
    return elaptime;
  }
}

/*#define TEST*/
#ifdef TEST
main()
{
	int i;
	printf("timing %g\n",timing(1));
	printf("timing %g\n",timing(0));
	for(i=0;i<20;i++){
	sleep(1);
	printf("timing %g\n",timing(0));
	}
}
#endif
