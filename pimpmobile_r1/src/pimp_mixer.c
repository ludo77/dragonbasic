/* pimp_mixer.c -- High level mixer code
 * Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#include "pimp_mixer.h"
#include "pimp_debug.h"

void pimp_mixer_reset(struct pimp_mixer *mixer)
{
	u32 c;
	ASSERT(mixer != NULL);
	
	for (c = 0; c < PIMP_CHANNEL_COUNT; ++c)
	{
		mixer->channels[c].sample_data = 0;
		mixer->channels[c].sample_cursor = 0;
		mixer->channels[c].sample_cursor_delta = 0;
	}
}

#define CALC_LOOP_EVENT

#ifndef CALC_LOOP_EVENT

PURE int pimp_linear_search_loop_event(int event_cursor, int event_delta, const int max_samples)
{
	int i;
	for (i = 0; i < max_samples; ++i)
	{
		event_cursor -= event_delta;
		if (event_cursor <= 0)
		{
			return i + 1;
		}
	}
	return -1;
}

#else /* CALC_LOOP_EVENT */

PURE int pimp_calc_loop_event(int event_cursor, int event_delta, const int max_samples)
{
	int result;
	if (event_cursor == 0) return 1;
	if ((event_cursor - event_delta * max_samples) > 0) return -1;
	
#ifdef TARGET_GBA
	{
		int number = event_cursor + event_delta - 1;
		int denom = event_delta;
		
		/* call BIOS-function to divide, as it's fast enough for our needs. */
		asm (
			"mov r0, %[number] \n"
			"mov r1, %[denom]  \n"
#ifdef __thumb__
			"swi 0x6           \n"
#else
			"swi 0x6 << 16     \n"
#endif
			"mov %[result], r0 \n"
			: [result] "=r" (result) /* output */
			: /* inputs */
				[number] "r" (number),
				[denom] "r" (denom)
			: "r0", "r1", "r2", "r3" /* clobbers */
		);
	}
#else
	result = (event_cursor + event_delta - 1) / event_delta;
#endif
	
	ASSERT(result <= max_samples);
	return result;
}

#endif

/* returns the number of samples that can be mixed before a loop event occurs */
PURE int pimp_mixer_detect_loop_event(const struct pimp_mixer_channel_state *chan, const int max_samples)
{
	int event_delta, event_cursor;
	
	ASSERT(max_samples > 0);
	ASSERT(NULL != chan);
	
	switch (chan->loop_type)
	{
		case LOOP_TYPE_NONE:
			/* event is end of sample */
			event_delta = chan->sample_cursor_delta;
			event_cursor = (chan->sample_length << 12) - chan->sample_cursor;
		break;
		
		case LOOP_TYPE_FORWARD:
			/* event is loop end */
			event_delta = chan->sample_cursor_delta;
			event_cursor = (chan->loop_end << 12) - chan->sample_cursor;
		break;
		
		case LOOP_TYPE_PINGPONG:
			if (chan->sample_cursor_delta >= 0)
			{
				/* moving forwards through the sample */
				/* event is loop end */
				event_delta = chan->sample_cursor_delta;
				event_cursor = (chan->loop_end << 12) - chan->sample_cursor;
			}
			else
			{
				/* moving backwards through the sample */
				/* event is loop start */
				event_delta = -chan->sample_cursor_delta;
				event_cursor = -((chan->loop_start << 12) - chan->sample_cursor - 1);
			}
		break;
		default:
			/* should never happen */
			ASSERT(FALSE);
			return 0;
	}
	
	ASSERT((event_cursor > 0) && (event_delta > 0));
	
#ifndef CALC_LOOP_EVENT
	return pimp_linear_search_loop_event(event_cursor, event_delta, max_samples);
#else
	return pimp_calc_loop_event(event_cursor, event_delta, max_samples);
#endif
}

/* returns false if we hit sample-end */
STATIC BOOL pimp_process_loop_event(struct pimp_mixer_channel_state *chan)
{
	ASSERT(NULL != chan);
	switch (chan->loop_type)
	{
		case LOOP_TYPE_NONE: return FALSE;
		
		case LOOP_TYPE_FORWARD:
			do
			{
				ASSERT(chan->sample_cursor >= (chan->loop_end << 12)); /* we should be positioned AT or BEYOND event cursor when responding to event */
				chan->sample_cursor -= (chan->loop_end - chan->loop_start) << 12;
			}
			while (chan->sample_cursor >= (chan->loop_end << 12));
		break;
		
		case LOOP_TYPE_PINGPONG:
			do
			{
				if (chan->sample_cursor_delta >= 0)
				{
					ASSERT(chan->sample_cursor >= (chan->loop_end << 12)); /* we should be positioned AT or BEYOND event cursor when responding to event */
					chan->sample_cursor -=  chan->loop_end << 12;
					chan->sample_cursor  = -chan->sample_cursor;
					chan->sample_cursor +=  chan->loop_end << 12;
					ASSERT(chan->sample_cursor > 0);
					chan->sample_cursor -= 1;
				}
				else
				{
					ASSERT(chan->sample_cursor >= (chan->loop_start << 12)); /* we should be positioned AT or BEYOND event cursor when responding to event */
					chan->sample_cursor -=  (chan->loop_start) << 12;
					chan->sample_cursor  = -chan->sample_cursor;
					chan->sample_cursor +=  (chan->loop_start) << 12;
					ASSERT(chan->sample_cursor > (chan->loop_start << 12));
					chan->sample_cursor -= 1;
				}
				chan->sample_cursor_delta = -chan->sample_cursor_delta;
			}
			while ((chan->sample_cursor > (chan->loop_end << 12)) || (chan->sample_cursor < (chan->loop_start << 12)));
		break;
		default: ASSERT(FALSE); /* should never happen */
	}
	
	return TRUE;
}

void pimp_mixer_mix_channel(struct pimp_mixer_channel_state *chan, s32 *target, u32 samples)
{
	ASSERT(NULL != target);
	ASSERT(NULL != chan);
	
	while (samples > 0)
	{
		int safe_samples = pimp_mixer_detect_loop_event(chan, samples);
		int mix_samples = safe_samples;
		
		ASSERT(samples >= safe_samples);
		
		if (safe_samples < 0) mix_samples = samples;
		
		pimp_mixer_mix_samples(target, mix_samples, chan->sample_data, chan->volume, chan->sample_cursor, chan->sample_cursor_delta);
		chan->sample_cursor = chan->sample_cursor + chan->sample_cursor_delta * mix_samples;
		
		if (safe_samples < 0) break; /* done. */
		
		target  += safe_samples; /* move target pointer */
		samples -= safe_samples;
		
		if (FALSE == pimp_process_loop_event(chan))
		{
			/* the sample has stopped, we need to fill the rest of the buffer with the dc-offset, so it doesn't ruin our unsigned mixing-thing */
			while (samples--)
			{
				*target++ += chan->volume * 128;
			}
			
			/* terminate sample */
			chan->sample_data = 0;
			
			return;
		}
		
		/* check that process_loop_event() didn't put us outside the sample */
		ASSERT((chan->sample_cursor >> 12) < chan->sample_length);
		ASSERT((chan->sample_cursor >> 12) >= 0);
	}
}

void pimp_mixer_mix(struct pimp_mixer *mixer, s8 *target, int samples)
{
	u32 c;
	int dc_offs;
	
	ASSERT(NULL != mixer);
	ASSERT(NULL != mixer->mix_buffer);
	ASSERT(NULL != target);
	ASSERT(samples >= 0);

	pimp_mixer_clear(mixer->mix_buffer, samples);
	
	dc_offs = 0;
	for (c = 0; c < PIMP_CHANNEL_COUNT; ++c)
	{
		struct pimp_mixer_channel_state *chan = &mixer->channels[c];
		if ((NULL != chan->sample_data) && (chan->volume > 0))
		{
			pimp_mixer_mix_channel(chan, mixer->mix_buffer, samples);
			dc_offs += chan->volume * 128;
		}
	}
	
	dc_offs >>= 8;
	
	pimp_mixer_clip_samples(target, mixer->mix_buffer, samples, dc_offs);
}
