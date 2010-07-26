/*
 *  Linux specific stuff
 *  Copyright (C) 2010 Andreas Öman
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "linux.h"
#include "misc/callout.h"
#include "prop/prop.h"



static int isvalid;
static int64_t last_idle[17];
static int64_t last_tot[17];
static callout_t timer;
static prop_t *p_cpuroot;
static prop_t *p_cpu[17];
static prop_t *p_load[16];

static int
cpu_monitor_do(void)
{
  int ret = 0, id;
  char data[1000];
  uint64_t v1, v2, v3, v4, v5, v6, v7, di, tot, dt;
  char buf[100];
  char s1[22];
  FILE *f;

  f = fopen("/proc/stat", "r");
  if(f == NULL)
    return 0;

  while(fgets(data, sizeof(data), f) != NULL) {
    if(sscanf(data, "%20s %"PRId64" %"PRId64" %"PRId64" "
	      "%"PRId64" %"PRId64" %"PRId64" %"PRId64, 
	      s1, &v1, &v2, &v3, &v4, &v5, &v6, &v7) != 8)
      continue;
    if(strncmp(s1, "cpu", 3))
      continue;

    tot = v1+v2+v3+v4+v5+v6+v7;


    if(!strcmp(s1, "cpu")) {
      id = 16;
    } else {
      id = atoi(s1 + 3);
      if(id < 0 || id > 16)
	continue;
    }
    
    if(isvalid) {
      di = v4  - last_idle[id];
      dt = tot - last_tot[id];

      if(id < 16) {

	if(p_cpu[id] == NULL) {
	  p_cpu[id] = prop_create(p_cpuroot, NULL);

	  snprintf(buf, sizeof(buf), "CPU%d", id);
	  prop_set_string(prop_create(p_cpu[id], "name"), buf);
	  p_load[id] = prop_create(p_cpu[id], "load");
	}
	float v = 1.0 - ((float)di / (float)dt);
	if(v < 0) v = 0;
	else if(v > 1) v = 1;

	prop_set_float(p_load[id], v);
      }
    }
    last_idle[id] = v4;
    last_tot[id] = tot;
  }
  isvalid = 1;
  prop_set_int(prop_create(prop_create(prop_get_global(), "cpuinfo"),
			   "available"), 1);
  fclose(f);
  return ret;
}


static void 
timercb(callout_t *c, void *aux)
{
  callout_arm(&timer, timercb, NULL, 1);
  cpu_monitor_do();
}

void
linux_init_cpu_monitor(void)
{
  p_cpuroot = prop_create(prop_create(prop_get_global(), "cpuinfo"), "cpus");

  cpu_monitor_do();
  callout_arm(&timer, timercb, NULL, 1);
}
