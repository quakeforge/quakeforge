/*
	ringbuffer.h

	ring buffer

	Copyright (C) 2020  Bill Currie <bill@taniwha.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/

#ifndef __QF_ringbuffer_h
#define __QF_ringbuffer_h

/** Type declaration for a type-safe ring buffer.
 *
 * While not in itself thread-safe, the buffer is designed (and tested) to be
 * used in a threaded environment using sutable locking mechanisms.
 *
 * \param type		The type of data element stored in the ring buffer.
 * \param size		The number of objects in the ring buffer. Note that the
 * 					actual capacity of the buffer is `size - 1` due to the
 * 					way ring buffers work.
 */
#define RING_BUFFER(type, size) 	\
	struct {						\
		type        buffer[size];	\
		unsigned    head;			\
		unsigned    tail;			\
	}

#define RB_buffer_size(ring_buffer)						\
	({	__auto_type rb = (ring_buffer);					\
		sizeof (rb->buffer) / sizeof (rb->buffer[0]);	\
	})

/** Return the amount of space available for writing in the ring buffer.
 *
 * Use this to know how much data can be written (RB_WRITE_DATA()) or acquired
 * (RB_ACQUIRE()), or just to see if any space is available.
 *
 * \note Does NOT affect buffer state.
 *
 * \param ring_buffer	The ring buffer to check for available space.
 */
#define RB_SPACE_AVAILABLE(ring_buffer)										\
	({	__auto_type rb = &(ring_buffer);									\
		(rb->tail + RB_buffer_size(rb) - rb->head - 1) % RB_buffer_size(rb);\
	})

/** Return the number of objects available in the ring buffer.
 *
 * Use this to know how much data can be read (RB_READ_DATA()) or discarded
 * (RB_RELEASE()), or just to see if any data is available.
 *
 * \note Does NOT affect buffer state.
 *
 * \param ring_buffer	The ring buffer to check for available data.
 */
#define RB_DATA_AVAILABLE(ring_buffer)										\
	({	__auto_type rb = &(ring_buffer);									\
		(rb->head + RB_buffer_size (rb) - rb->tail) % RB_buffer_size (rb);	\
	})

/** Write \a count objects from \a data to the buffer, wrapping if necessary.
 *
 * \note Affects buffer state (advances the head).
 *
 * \note Does NOT check that the space is available. It is the caller's
 * responsitiblity to do so using RB_SPACE_AVAILABLE().
 *
 * \param ring_buffer	The ring buffer to which the data will be written.
 * \param data			Pointer to the data to be copied into the ring buffer.
 * \param count			unsigned int. The number of objects to copy from
 *						\a data
 */
#define RB_WRITE_DATA(ring_buffer, data, count)								\
	({	__auto_type rb = &(ring_buffer);									\
		const typeof (rb->buffer[0]) *d = (data);							\
		unsigned    c = (count);											\
		unsigned    h = rb->head;											\
		rb->head = (h + c) % RB_buffer_size (rb);							\
		if (c > RB_buffer_size (rb) - h) {									\
			memcpy (rb->buffer + h, d,										\
					(RB_buffer_size (rb) - h) * sizeof (rb->buffer[0]));	\
			c -= RB_buffer_size (rb) - h;									\
			d += RB_buffer_size (rb) - h;									\
			h = 0;															\
		}																	\
		memcpy (rb->buffer + h, d, c * sizeof (rb->buffer[0]));				\
	})

/** Acquire \a count objects from the buffer, wrapping if necessary.
 *
 * \note Affects buffer state (advances the head).
 *
 * \note Does NOT check that the space is available. It is the caller's
 * responsitiblity to do so using RB_SPACE_AVAILABLE().
 *
 * \param ring_buffer	The ring buffer to which the data will be written.
 * \param count			unsigned int. The number of objects to copy from
 *						\a data.
 * \return				Address of the first object acquired.
 */
#define RB_ACQUIRE(ring_buffer, count)										\
	({	__auto_type rb = &(ring_buffer);									\
		unsigned    c = (count);											\
		unsigned    h = rb->head;											\
		rb->head = (h + c) % RB_buffer_size (rb);							\
		&rb->buffer[h];														\
	})

/** Read \a count objects from the buffer to \a data, wrapping if necessary.
 *
 * \note Affects buffer state (advances the tail).
 *
 * \note Does NOT check that the data is available. It is the caller's
 * responsitiblity to do so using RB_DATA_AVAILABLE().
 *
 * \param ring_buffer	The ring buffer from which the data will be read.
 * \param data			Pointer to the location into which data will be copied
 * 						from the ring buffer.
 * \param count			unsigned int. The number of objects to copy from
 *						the buffer into \a data
 */
#define RB_READ_DATA(ring_buffer, data, count)								\
	({	__auto_type rb = &(ring_buffer);									\
		typeof (&rb->buffer[0]) d = (data);									\
		unsigned    c = (count);											\
		unsigned    oc = c;													\
		unsigned    t = rb->tail;											\
		if (c > RB_buffer_size (rb) - t) {									\
			memcpy (d, rb->buffer + t,										\
					(RB_buffer_size (rb) - t) * sizeof (rb->buffer[0]));	\
			c -= RB_buffer_size (rb) - t;									\
			d += RB_buffer_size (rb) - t;									\
			t = 0;															\
		}																	\
		memcpy (d, rb->buffer + t, c * sizeof (rb->buffer[0]));				\
		rb->tail = (t + oc) % RB_buffer_size (rb);							\
	})

/** Discard \a count objects from the ring buffer.
 *
 * \note Affects buffer state (advances the tail).
 *
 * \note Does NOT check that the data is available. It is the caller's
 * responsitiblity to do so using RB_DATA_AVAILABLE().
 *
 * \param ring_buffer	The ring buffer from which the objects will be
 * 						discarded.
 * \param count			The number of objects to discard.
 */
#define RB_RELEASE(ring_buffer, count)										\
	({	__auto_type rb = &(ring_buffer);									\
		unsigned    c = (count);											\
		unsigned    t = rb->tail;											\
		rb->tail = (t + c) % RB_buffer_size (rb);							\
	})

/** Access a single item from the buffer without affecting buffer state.
 *
 * \note Does NOT affect buffer state.
 *
 * \note Does NOT check that the data is available. It is the caller's
 * responsitiblity to do so using RB_DATA_AVAILABLE().
 *
 * \param ring_buffer	The ring buffer from which to access the object.
 * \param ahead			The tail-relative index of the object to access from
 *						the buffer. Valid range is 0 to
 *						`RB_DATA_AVAILABLE() - 1`
 * \return				Address of the accessed element
 */
#define RB_PEEK_DATA(ring_buffer, ahead)						\
	({	__auto_type rb = &(ring_buffer);						\
		&rb->buffer[(rb->tail + ahead) % RB_buffer_size (rb)];	\
	})

/** WRite a single item to the buffer without affecting buffer state.
 *
 * \note Does NOT affect buffer state.
 *
 * \note Does NOT check that the data is available. It is the caller's
 * responsitiblity to do so using RB_DATA_AVAILABLE().
 *
 * \param ring_buffer	The ring buffer from which to read the object.
 * \param ahead			The tail-relative index of the buffer object to write.
 *						Valid range is 0 to `RB_DATA_AVAILABLE() - 1`
 * \param data			The data to write to the buffer.
 */
#define RB_POKE_DATA(ring_buffer, ahead, data)							\
	({	__auto_type rb = &(ring_buffer);								\
		rb->buffer[(rb->tail + ahead) % RB_buffer_size (rb)] = (data);	\
	})

#endif//__QF_ringbuffer_h
