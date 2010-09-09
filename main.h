/**************************************************************************
 *                                                                        *
 *  statustran                                                            *
 *                                                                        *
 *  StatusNet translator for the GNU operating system (requires curl)     *
 *                                                                        *
 *  Copyright 1995, 1996, 1997, 1998, 1999, 2001, 2002, 2007              *
 *            Free Software Foundation, Inc.                              *
 *                                                                        *
 *  Copyright 2010  Matt Windsor <hayashi@archhurd.org>                   *
 *                                                                        *
 *  Based on work by Gordon Matzigkeit <gord@fig.org>                     *
 *               and Miles Bader       <miles@gnu.org>                    *
 *                                                                        *
 *  This file is part of statustran.                                      *
 *                                                                        *
 *  statustran is free software: you can redistribute it and/or modify    *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation, either version 3 of the License, or     *
 *  (at your option) any later version.                                   *
 *                                                                        *
 *  statustran is distributed in the hope that it will be useful,         *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *  GNU General Public License for more details.                          *
 *                                                                        *
 *  You should have received a copy of the GNU General Public License     *
 *  along with statustran.  If not, see <http://www.gnu.org/licenses/>.   *
 *                                                                        *
 **************************************************************************/

enum
  {
    MESSAGE_SIZE = 140,  /**< The size of a message on StatusNet. */
    OUTBUF_SIZE  = 1000  /**< Size of the buffer to store the URL-encoded 
			    version of the message. It should probably be 
			    at least three times MESSAGE_SIZE with more 
			    room for the status= prefix. */
  };

extern const char URL[];           /**< The URL to send the message to. */
extern const char ORIGIN_STANZA[]; /**< The POST datum referring to the 
				      origin of the message post. */

#define MIN(x,y) ((x) < (y) ? (x) : (y))
