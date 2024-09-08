/* pimp_effects.h -- Implementations of the actual effects
 * Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#ifndef PIMP_EFFECTS_H
#define PIMP_EFFECTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pimp_internal.h"

static void porta_up(struct pimp_channel_state *chan, s32 period_low_clamp)
{
	ASSERT(chan != 0);
	
	chan->period -= chan->porta_speed;
	if (chan->period < period_low_clamp) chan->period = period_low_clamp;
}

static void porta_down(struct pimp_channel_state *chan, s32 period_high_clamp)
{
	ASSERT(chan != 0);
	
	chan->period += chan->porta_speed;
	if (chan->period > period_high_clamp) chan->period = period_high_clamp;
}

static void porta_note(struct pimp_channel_state *chan)
{
	ASSERT(chan != 0);
	
	if (chan->period > chan->porta_target)
	{
		chan->period -= chan->porta_speed;
		if (chan->period < chan->porta_target) chan->period = chan->porta_target;
	}
	else if (chan->period < chan->porta_target)
	{
		chan->period += chan->porta_speed;
		if (chan->period > chan->porta_target) chan->period = chan->porta_target;
	}
}

static void volume_slide(struct pimp_channel_state *chan, int speed)
{
	chan->volume += speed;
	if (chan->volume > 64) chan->volume = 64;
	if (chan->volume < 0)  chan->volume = 0;
}

static void vibrato(struct pimp_channel_state *chan, s32 period_low_clamp, s32 period_high_clamp)
{
	static const s16 sine_waveform[64] = {
		   0,   24,   49,   74,   97,  120,  141,  161,
		 180,  197,  212,  224,  235,  244,  250,  253,
		 255,  253,  250,  244,  235,  224,  212,  197,
		 180,  161,  141,  120,   97,   74,   49,   24,
		  -0,  -24,  -49,  -74,  -97, -120, -141, -161,
		-180, -197, -212, -224, -235, -244, -250, -253,
		-255, -253, -250, -244, -235, -224, -212, -197,
		-180, -161, -141, -120,  -97,  -74,  -49,  -24
	};
	
	chan->period = chan->note_period + (sine_waveform[chan->vibrato_counter & 63] * chan->vibrato_depth) / 32;
	
	if (chan->period < period_low_clamp) chan->period = period_low_clamp;
	if (chan->period > period_high_clamp) chan->period = period_high_clamp;
	
	chan->vibrato_counter += chan->vibrato_speed;
}

#ifdef __cplusplus
}
#endif

#endif /* PIMP_EFFECTS_H */
