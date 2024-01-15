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
 * @file lib/bio/fd.h
 * @brief Binary IO abstractions for file descriptors
 *
 * Allow reads and writes from file descriptors.
 *
 * @copyright 2024 Network RADIUS SAS (legal@networkradius.com)
 */
RCSIDH(lib_bio_fd_h, "$Id$")

#include <freeradius-devel/bio/base.h>
#include <freeradius-devel/util/socket.h>

/*
 *	For reading packets src_ip is *their* IP, and dst_ip is *our* IP.
 *
 *	For writing packets, src_ip is *our* IP, and dst_ip is *their* IP.
 */
typedef struct {
	fr_time_t	when;		//!< when the packet was received
	fr_socket_t	socket;
} fr_bio_fd_packet_ctx_t;

typedef enum {
	FR_BIO_FD_STATE_INVALID = 0,
	FR_BIO_FD_STATE_CLOSED,
	FR_BIO_FD_STATE_OPEN,		//!< error states must be before this
	FR_BIO_FD_STATE_CONNECTING,
} fr_bio_fd_state_t;

typedef enum {
	FR_BIO_FD_UNCONNECTED,		//!< unconnected UDP / datagram only
					// uses #fr_bio_fd_packet_ctx_t for writes and reads
	FR_BIO_FD_CONNECTED,		//!< connected client sockets
	FR_BIO_FD_ACCEPT,		//!< returns new fd in buffer on read()
					// uses #fr_bio_fd_packet_ctx_t for read
} fr_bio_fd_type_t;

typedef struct {
	fr_socket_t	socket;

	fr_bio_fd_type_t type;

	bool		read_blocked;
	bool		write_blocked;
	bool		eof;

	uint8_t		state;
} fr_bio_fd_info_t;

fr_bio_t	*fr_bio_fd_alloc(TALLOC_CTX *ctx, fr_bio_cb_funcs_t *cb, fr_socket_t const *sock, fr_bio_fd_type_t type, size_t offset) CC_HINT(nonnull(1));

int		fr_bio_fd_close(fr_bio_t *bio) CC_HINT(nonnull);

int		fr_bio_fd_init(fr_bio_t *bio, fr_socket_t const *sock) CC_HINT(nonnull);

int		fr_bio_fd_connect(fr_bio_t *bio) CC_HINT(nonnull);

fr_bio_fd_info_t const *fr_bio_fd_info(fr_bio_t *bio) CC_HINT(nonnull);

int		fr_bio_fd_socket_open(fr_bio_t *bio, fr_socket_t const *sock, fr_bio_fd_type_t type, bool async) CC_HINT(nonnull);
