/*  
 * Copyright (c) 2012  University of Padua, Department of Mathematics  
 *  
 *  This file is free software: you may copy, redistribute and/or modify it  
 *  under the terms of the GNU General Public License as published by the  
 *  Free Software Foundation, either version 2 of the License, or (at your  
 *  option) any later version.  
 *  
 *  This file is distributed in the hope that it will be useful, but  
 *  WITHOUT ANY WARRANTY; without even the implied warranty of  
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU  
 *  General Public License for more details.  
 *  
 *  You should have received a copy of the GNU General Public License  
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.  
 *  
 * This file incorporates work covered by the following copyright and  
 * permission notice:  
 *  
 * 		Copyright (c) 2007-2009 POK team
 *
 *		Redistribution and use in source and binary forms, with or without
 *		modification, are permitted provided that the following conditions
 *		are met:
 *
 *			* Redistributions of source code must retain the above copyright
 *			  notice, this list of conditions and the following disclaimer.
 *			* Redistributions in binary form must reproduce the above
 *			  copyright notice, this list of conditions and the following
 *			  disclaimer in the documentation and/or other materials
 *			  provided with the distribution.
 *			* Neither the name of the POK Team nor the names of its main
 *			  author (Julien Delange) or its contributors may be used to
 *			  endorse or promote products derived from this software
 *			  without specific prior written permission.
 *
 *		THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *		AS IS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *		LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *		FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *		COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *		INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *		BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *		LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *		CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *		LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *		ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *		POSSIBILITY OF SUCH DAMAGE.
 */



#include <errno.h>
#include <core/time.h>
#include <core/sched.h>
#include <dependencies.h>
#include <libc.h>
#include <bsp.h>

#include "rtc.h"

/**
 * From "kernel/include/core/time.h"
 * The rate of the clock in POK
 * #define POK_TIMER_FREQUENCY 1000
 */

/* Last time when decr was set.  */
static unsigned long long time_last;

/* Decrementer optimal value.  */
/*static*/ unsigned int time_inter;

unsigned int next_timer;

unsigned long long time_new;

#ifdef POK_SKIP_IDLE

void set_patmos_tb (unsigned long long time_warp)
{

	__PATMOS_RTC_WR_CYCLE_LOW((unsigned int)time_warp)
	__PATMOS_RTC_WR_CYCLE_UP((unsigned int)(time_warp >> 32))

}

#endif

unsigned long long get_patmos_tb(void)
{
	unsigned int u;
	unsigned int l;

	__PATMOS_RTC_RD_CYCLE_LOW(l)
	__PATMOS_RTC_RD_CYCLE_UP(u)

	return (((unsigned long long)u) << 32) | l;
}

uint64_t last_patmos_tb;

bool_t dec_updated;

#ifdef POK_NEEDS_SCHED_O1
uint64_t pok_update_tick()
{
	uint64_t current_time = get_patmos_tb();
	uint64_t the_counter;
	if(!dec_updated)
		the_counter = (current_time - time_last)/((POK_BUS_FREQ_HZ /POK_FREQ_DIV) / POK_TIMER_FREQUENCY);
	else
		the_counter = (current_time - last_patmos_tb)/((POK_BUS_FREQ_HZ /POK_FREQ_DIV) / POK_TIMER_FREQUENCY);

	return pok_tick_counter+the_counter;
}
#endif

/* Compute new value for the decrementer.  If the value is in the future,
	sets the decrementer else returns an error.  */
int pok_arch_set_decr (unsigned int timer)
{
	time_new = time_last + timer;
	unsigned long long time_cur = get_patmos_tb();
	int delta = time_new - time_cur;
	
	#ifdef POK_NEEDS_DEBUG
	printf("TIME LAST: ");
	print_long(time_last);
	printf(", TIMER: %u, TIME NEW: ",timer);
	print_long(time_new);printf(", TIME CUR: ");
	print_long(time_cur);printf(", DELTA: %d\n",delta);
	#endif
	last_patmos_tb = time_last;
	time_last = time_new;
	dec_updated=TRUE;
	if (delta < 0)
	{
		return POK_ERRNO_EINVAL;
	}
	else
	{
		__PATMOS_RTC_WR_INTERVAL(delta)
		return POK_ERRNO_OK;
	}
}

/* Called by the interrupt handled.  */
void pok_arch_decr_int (void)
{
	dec_updated=FALSE;
#ifndef POK_NEEDS_SCHED_O1
	int err;
	do
	{
		err = pok_arch_set_decr(next_timer);
#endif
		//O1 scheduler: count ticks which can be also non-multiple of frequency - FORMERLY: +=1

		pok_tick_counter += next_timer/((POK_BUS_FREQ_HZ /POK_FREQ_DIV) / POK_TIMER_FREQUENCY);

#ifndef POK_NEEDS_SCHED_O1
	} while (err != POK_ERRNO_OK);
#endif

#ifdef POK_NEEDS_DEBUG
	printf ( "DEC interrupt:%u\n" , (unsigned int) pok_tick_counter );
#endif
	pok_sched ();
}

pok_ret_t pok_bsp_time_init ()
{
	int err;

#ifdef POK_NEEDS_DEBUG
	printf ("TIMER_SETUP: Freq:%d MHZ, Div:%d, Shift:%d\n", POK_BUS_FREQ_MHZ, POK_FREQ_DIV, POK_FREQ_SHIFT);
#endif

	time_inter = (POK_BUS_FREQ_HZ /POK_FREQ_DIV) / POK_TIMER_FREQUENCY;

	next_timer = time_inter;
	time_last = get_patmos_tb ();
	last_patmos_tb = time_last;


	err = pok_arch_set_decr(next_timer);

	return (POK_ERRNO_OK);
}