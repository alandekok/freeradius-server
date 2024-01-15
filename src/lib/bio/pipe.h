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
 * @file lib/bio/pipe.h
 * @brief BIO pipe handlers.
 *
 * @copyright 2024 Network RADIUS SAS (legal@networkradius.com)
 */
RCSIDH(lib_bio_pipe_h, "$Id$")

/** Say a pipe is readable
 *
 *  Called from the readers thread, but with the writers data.
 *
 *  This function should be async-safe, as it is called with the pipe
 *  mutex held.  It should do as little as possible.
 */
typedef void (*fr_bio_pipe_readable_t)(fr_bio_t *bio, void *uctx);

fr_bio_t	*fr_bio_pipe_alloc(TALLOC_CTX *ctx, size_t buffer_size, fr_bio_pipe_readable_t readable, void *uctx);

void		fr_bio_pipe_set_eof(fr_bio_t *bio);
