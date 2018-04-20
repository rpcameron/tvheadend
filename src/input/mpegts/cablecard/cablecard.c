/* Tvheadend - CableCARD Subsystem
 *
 * Copyright (C) 2018 Robert Cameron
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "input.h"

#include "cablecard_private.h"

void
cablecard_init(void)
{
	/* Logical network init */
	cablecard_network_init();

	/* Physical network init */
	cablecard_device_init();
}

void
cablecard_done(void)
{
	/* Physical network teardown */
	cablecard_device_done();

	/* Network teardown */
	cablecard_network_done();
}