/**********************************************************************
  timer_util.c
 **********************************************************************

  timer_util - Useful timer routines with S-Lang intrinsics.
  Copyright ©2002-2003, Stewart Adcock <stewart@linux-domain.com>
  All rights reserved.

  The latest version of this program should be available at:
  http://www.stewart-adcock.co.uk/

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.  Alternatively, if your project
  is incompatible with the GPL, I will probably agree to requests
  for permission to use the terms of any other license.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY WHATSOEVER.

  A full copy of the GNU General Public License should be in the file
  "COPYING" provided with this distribution; if not, see:
  http://www.gnu.org/

 **********************************************************************

  Synopsis:	Timer routines with S-Lang intrinsics which are
		helpful for code analysis.

		These functions are thread-safe.

		Note that the user time will be incorrect after about
		72 minutes.

  Updated:	06 Jun 2003 SAA	Reduced number of calls to the clock() function.
		20 Mar 2002 SAA Replaced use of printf("%Zd", (size_t)) to printf("%lu", (unsigned long)).
		13 Mar 2002 SAA timer_diagnostics() added.
		07 Feb 2002 SAA	Rewrote S-Lang stuff.
		06 Feb 2002 SAA	First version.

 **********************************************************************/

#include "timer_util.h"

/**********************************************************************
  timer_diagnostics()
  synopsis:	Display diagnostic information.
  parameters:	none
  return:	none
  last updated:	13 Mar 2002
 **********************************************************************/

void timer_diagnostics(void)
  {
  printf("=== Table diagnostics ========================================\n");
  printf("Version:                   %s\n", VERSION_STRING);
  printf("Build date:                %s\n", BUILD_DATE_STRING);
  printf("Compilation machine characteristics:\n%s\n", UNAME_STRING);

  printf("--------------------------------------------------------------\n");
  printf("structure                  sizeof\n");
  printf("chrono_t                   %lu\n", (unsigned long) sizeof(chrono_t));
  printf("==============================================================\n");

  return;
  }


/**********************************************************************
  timer_start()
  synopsis:	Set or reset timer.
  parameters:	chrono_t	t	The timer
  return:	none
  last updated:	06 Feb 2002
 **********************************************************************/

void timer_start(chrono_t *t)
  {
  t->begin_clock = t->save_clock = clock();
  t->begin_time = t->save_time = time(NULL);

  plog(LOG_NORMAL, "Timer started: %d", t->begin_clock);

  return;
  }


/**********************************************************************
  timer_check()
  synopsis:	Read timer.
  parameters:	chrono_t	t	The timer
  return:	user time in seconds.
  last updated:	06 Jun 2003
 **********************************************************************/

double timer_check(chrono_t *t)
  {
  double        user_time, real_time;
  clock_t	c = clock();
  time_t	t = time();

  plog(LOG_NORMAL, "Timer checked: %d", c);

  user_time = (c - t->save_clock) / (double) CLOCKS_PER_SEC;
  real_time = difftime(t, t->save_time);
  t->save_clock = c;
  t->save_time = t;

  plog(LOG_NORMAL, "User time: %f seconds.", user_time);
  plog(LOG_NORMAL, "Real time: %f seconds.", real_time);

  return user_time;
  }


/**********************************************************************
  SLang intrinsic wrappers.
  We can't use pointers from S-Lang, and structures are a pain in the
  posterior, so we use a 
 **********************************************************************/

#if HAVE_SLANG==1

THREAD_LOCK_DEFINE_STATIC(chrono_table_lock);
static TableStruct *chrono_table=NULL;        /* Table of timer handles. */

/**********************************************************************
  timer_new_slang()
  synopsis:	Allocate a new timer timer.
  parameters:	none
  return:	timer handle
  last updated:	07 Feb 2002
 **********************************************************************/

int timer_new_slang(void)
  {
  chrono_t	*t=s_malloc(sizeof(chrono_t));
  int		t_handle;

  THREAD_LOCK(chrono_table_lock);
  if (!chrono_table) chrono_table=table_new();

  t_handle = table_add(chrono_table, (vpointer) t);
  THREAD_UNLOCK(chrono_table_lock);

  return (int) t_handle;
  }


/**********************************************************************
  timer_new_slang()
  synopsis:	Allocate a new timer timer.
  parameters:	none
  return:	timer handle
  last updated:	07 Feb 2002
 **********************************************************************/

void timer_free_slang(int *t_handle)
  {
  chrono_t       *t;

  THREAD_LOCK(chrono_table_lock);
  t = table_remove_index(chrono_table, *t_handle);
  THREAD_UNLOCK(chrono_table_lock);

  s_free(t);

  return;
  }


/**********************************************************************
  timer_start_slang()
  synopsis:	Set or reset timer.
  parameters:	chrono_t	t	The timer
  return:	none
  last updated:	07 Feb 2002
 **********************************************************************/

void timer_start_slang(int *t_handle)
  {
  chrono_t       *t;

  t = (chrono_t *) table_get_data(chrono_table, *t_handle);

  timer_start(t);

  return;
  }


/**********************************************************************
  timer_check_slang()
  synopsis:	Check timer.
  parameters:	chrono_t	t	The timer
  return:	user time in seconds.
  last updated:	07 Feb 2002
 **********************************************************************/

double timer_check_slang(int *t_handle)
  {
  chrono_t       *t;

  t = (chrono_t *) table_get_data(chrono_table, *t_handle);

  return timer_check(t);
  }

#endif	/* HAVE_SLANG==1 */

