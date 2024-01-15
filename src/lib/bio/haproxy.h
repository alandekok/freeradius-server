#pragma once
/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/**
 * $Id$
 * @file lib/bio/haproxy.h
 * @brief Binary IO abstractions for HA proxy protocol interceptors
 *
 * @copyright 2024 Network RADIUS SAS (legal@networkradius.com)
 */
RCSIDH(lib_bio_fd_h, "$Id$")

#include <freeradius-devel/util/socket.h>

typedef struct {
	fr_socket_t	socket;
} fr_bio_haproxy_info_t;

fr_bio_t	*fr_bio_haproxy_alloc(TALLOC_CTX *ctx, fr_bio_t *next) CC_HINT(nonnull);

fr_bio_haproxy_info_t const *fr_bio_haproxy_info(fr_bio_t *bio) CC_HINT(nonnull);
