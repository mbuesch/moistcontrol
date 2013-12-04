/*
 * Output extender
 *
 * Copyright (c) 2013 Michael Buesch <m@bues.ch>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "ioext.h"


struct ioext_context ioext_ctx;


void ioext_commit(void)
{
	struct ioext_context *ctx = &ioext_ctx;
	uint8_t chip;

	for (chip = 0; chip < EXTOUT_NR_CHIPS; chip++) {
		if (ctx->states[chip] != ctx->old_states[chip]) {
			ctx->old_states[chip] = ctx->states[chip];
			pcf8574_write(&ctx->chips[chip],
				      ctx->states[chip]);
		}
	}
}

void ioext_init(void)
{
	struct ioext_context *ctx = &ioext_ctx;
	uint8_t chip;

	memset(ctx, 0, sizeof(*ctx));
	for (chip = 0; chip < EXTOUT_NR_CHIPS; chip++)
		pcf8574_init(&ctx->chips[chip], chip, 1);
}
