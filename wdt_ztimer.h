/*
 * Copyright (C) 2020-2022 Universit√© Grenoble Alpes
 */

/*
 * Author: Didier Donsez, Universit√© Grenoble Alpes
 */


#ifndef _WDT_TIMER_H
#define _WDT_TIMER_H

int start_wdt_ztimer(void);
#if WDT_HAS_STOP
int wdt_stop_cmd(int argc, char *argv[]);
#endif
int abort_cmd(int argc, char *argv[]);

#endif
