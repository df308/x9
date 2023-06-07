/*
   X9 - high performance message passing library.
   Copyright (c) 2023, Diogo Flores

   BSD 2-Clause License (https://opensource.org/license/bsd-2-clause/)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at: diogoxflores@gmail.com
*/

#pragma once

#include <stdbool.h> /* bool */
#include <stdint.h>  /* uint64_t */

/* --- Opaque types --- */

typedef struct x9_node_internal  x9_node;
typedef struct x9_inbox_internal x9_inbox;

/* --- Public API --- */

/* Creates a x9_inbox with a buffer of size 'sz', which must be positive and
 * mod 2 == 0, a 'name', which can be used for comparison by calling
 * 'x9_inbox_name_is' or for selecting a specific x9_inbox from a x9_node by
 * calling 'x9_select_inbox_from_node', and a 'msg_sz' which the inbox is
 * expected to receive.
 *
 * Example:
 *   x9_inbox* inbox = x9_create_inbox(512, "ibx", sizeof(<some struct>));*/
__attribute__((nonnull)) x9_inbox* x9_create_inbox(
    uint64_t const sz, char const* restrict const name, uint64_t const msg_sz);

/* Variadic function that creates a 'x9_node', which is an abstraction that
 * unifies x9_inbox(es).
 * 'name' can be used for comparison by calling 'x9_node_name_is',
 * 'n_inboxes' must be > 0 and equal the number of inboxes passed to the
 * function to be attached to the x9_node.
 *
 * Example:
 *   x9_node* node = x9_create_node("my_node", 3, inbox_1, inbox_2, inbox_3);*/
__attribute__((nonnull)) x9_node* x9_create_node(char* restrict const name,
                                                 uint64_t const n_inboxes,
                                                 ...);

/* Returns 'true' if the 'inbox' is valid, 'false' otherwise.
 * Should always be called after 'x9_create_inbox' to validate the correct
 * initialization of the underlying data structure, and after calling
 * 'x9_select_inbox_from_node' to ensure that the user selected a valid inbox.
 * If the return value is 'false', and it's unclear why it is so, the user can
 * enable the 'X9_DEBUG' macro at compile time for precise error information.*/
bool x9_inbox_is_valid(x9_inbox const* const inbox);

/* Returns 'true' if the 'node' is valid, 'false' otherwise.
 * Should always be called after 'x9_create_node' to validate the correct
 * initialization of the underlying data structure.
 * If the return value is 'false', and it's unclear why it is so, the user can
 * enable the 'X9_DEBUG' macro at compile time for precise error information.*/
bool x9_node_is_valid(x9_node const* const node);

/* Selects a x9_inbox from a 'node' by its 'name'.
 * Returns NULL if the 'node' does not contain an x9_inbox with such 'name'.*/
__attribute__((nonnull)) x9_inbox* x9_select_inbox_from_node(
    x9_node const* const node, char const* restrict const name);

/* Returns 'true' if the 'inbox' name == 'cmp', 'false' otherwise.*/
__attribute__((nonnull)) bool x9_inbox_name_is(x9_inbox const* const inbox,
                                               char const* restrict const cmp);

/* Returns 'true' if the 'node' name == 'cmp', 'false' otherwise.*/
__attribute__((nonnull)) bool x9_node_name_is(x9_node const* const node,
                                              char const* restrict const cmp);

/* Frees the 'inbox' data structure and its internal components. */
__attribute__((nonnull)) void x9_free_inbox(x9_inbox* const inbox);

/* Frees the 'node' data structure and its internal components. */
__attribute__((nonnull)) void x9_free_node(x9_node* const node);

/* Frees the 'node' and all the x9_inbox(es) attached to it.
 * IMPORTANT: should only be used when the attached x9_inbox(es) are not shared
 * with other nodes.*/
__attribute__((nonnull)) void x9_free_node_and_attached_inboxes(
    x9_node* const node);

/* Returns 'true' if a message was read, 'false' otherwise.
 * If 'true', the message will be written to 'outparam'.
 * IMPORTANT: Can only be used to read from inboxes where the thread calling
 * this function is the only thread reading from said 'inbox'.*/
__attribute__((nonnull)) bool x9_read_from_inbox(
    x9_inbox* const inbox,
    uint64_t const  msg_sz,
    void* restrict const outparam);

/* Reads the next unread message in the 'inbox' to 'outparam'.
 * Uses spinning, that is, it wil not return until it has read a message, and
 * it will keep checking if a message was written and try to read it. */
__attribute__((nonnull)) void x9_read_from_inbox_spin(
    x9_inbox* const inbox,
    uint64_t const  msg_sz,
    void* restrict const outparam);

/* Returns 'true' if a message was read, 'false' otherwise.
 * If 'true', the msg contents will be written to the 'outparam'.
 * Use this function when multiple threads read from the same inbox. */
__attribute__((nonnull)) bool x9_read_from_shared_inbox(
    x9_inbox* const inbox,
    uint64_t const  msg_sz,
    void* restrict const outparam);

/* Reads the next unread message in the 'inbox' to 'outparam'.
 * Use this function when multiple threads read from the same inbox.
 * Uses spinning, that is, it wil not return until it has read a message, and
 * it will keep checking if a message was written and try to read it. */
__attribute__((nonnull)) void x9_read_from_shared_inbox_spin(
    x9_inbox* const inbox,
    uint64_t const  msg_sz,
    void* restrict const outparam);

/* Returns 'true' if the message was written to the 'inbox', 'false'
 * otherwise. */
__attribute__((nonnull)) bool x9_write_to_inbox(
    x9_inbox* const inbox,
    uint64_t const  msg_sz,
    void const* restrict const msg);

/* Writes the 'msg' to the 'inbox'.
 * Uses spinning, that is, it wil not not return until it has written the
 * 'msg', and it will keep checking if the destination inbox has a free
 * slot that it can write the 'msg' to. */
__attribute__((nonnull)) void x9_write_to_inbox_spin(
    x9_inbox* const inbox,
    uint64_t const  msg_sz,
    void const* restrict const msg);

/* Writes the same 'msg' to all 'node' inboxes.
 * Calls 'x9_write_to_inbox_spin' in the background.
 * Users must guarantee that all 'node' inboxes accept messages of the
 * same type (or at least of the same 'msg_sz') */
__attribute__((nonnull)) void x9_broadcast_msg_to_all_node_inboxes(
    x9_node const* const node,
    uint64_t const       msg_sz,
    void const* restrict const msg);

