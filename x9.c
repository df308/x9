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

#pragma GCC diagnostic ignored "-Wpadded"

#include "x9.h"

#include <assert.h>    /* assert */
#include <immintrin.h> /* _mm_pause */
#include <stdarg.h>    /* va_* */
#include <stdatomic.h> /* atomic_* */
#include <stdbool.h>   /* bool */
#include <stdio.h>     /* printf */
#include <stdlib.h>    /* aligned_alloc, calloc */
#include <string.h>    /* strcmp */
#include <string.h>    /* memcpy */

/* CPU cache line size */
#define X9_CL_SIZE       64
#define X9_ALIGN_TO_CL() __attribute__((__aligned__(X9_CL_SIZE)))

#ifdef X9_DEBUG
static void x9_print_error_msg(char const* const error_msg) {
  printf("X9_ERROR: %s\n", error_msg);
  fflush(stdout);
  return;
}
#endif

/* --- Internal types --- */

typedef struct {
  _Atomic(bool) slot_has_data;
  _Atomic(bool) msg_written;
  _Atomic(bool) shared;
  char const    pad[5];
} x9_msg_header;

typedef struct x9_inbox_internal {
  _Atomic(uint64_t) read_idx  X9_ALIGN_TO_CL();
  _Atomic(uint64_t) write_idx X9_ALIGN_TO_CL();
  uint64_t sz                 X9_ALIGN_TO_CL();
  uint64_t                    msg_sz;
  uint64_t                    constant;
  void*                       msgs;
  char*                       name;
  char                        pad[24];
} x9_inbox;

typedef struct x9_node_internal {
  x9_inbox** inboxes;
  uint64_t   n_inboxes;
  char*      name;
} x9_node;

/* --- Internal functions --- */

static inline uint64_t x9_load_idx(x9_inbox* const inbox,
                                   bool const      read_idx) {
  /* From paper: Faster Remainder by Direct Computation, Lemire et al */
  register uint64_t const low_bits =
      inbox->constant *
      (read_idx ? atomic_load_explicit(&inbox->read_idx, __ATOMIC_RELAXED)
                : atomic_load_explicit(&inbox->write_idx, __ATOMIC_RELAXED));
  return ((__uint128_t)low_bits * inbox->sz) >> 64;
}

static inline uint64_t x9_increment_idx(x9_inbox* const inbox,
                                        bool const      read_idx) {
  /* From paper: Faster Remainder by Direct Computation, Lemire et al */
  register uint64_t const low_bits =
      inbox->constant *
      (read_idx
           ? atomic_fetch_add_explicit(&inbox->read_idx, 1, __ATOMIC_RELAXED)
           : atomic_fetch_add_explicit(&inbox->write_idx, 1,
                                       __ATOMIC_RELAXED));
  return ((__uint128_t)low_bits * inbox->sz) >> 64;
}

static inline void* x9_header_ptr(x9_inbox const* const inbox,
                                  uint64_t const        idx) {
  return &((char*)inbox->msgs)[idx * (inbox->msg_sz + sizeof(x9_msg_header))];
}

/* --- Public functions --- */

x9_inbox* x9_create_inbox(uint64_t const sz,
                          char const* restrict const name,
                          uint64_t const msg_sz) {
  if (!((sz > 0) && !(sz % 2))) { goto inbox_incorrect_size; }

  x9_inbox* inbox = aligned_alloc(X9_CL_SIZE, sizeof(x9_inbox));
  if (NULL == inbox) { goto inbox_allocation_failed; }
  memset(inbox, 0, sizeof(x9_inbox));

  uint64_t const name_len = strlen(name);
  char*          ibx_name = calloc(name_len + 1, sizeof(char));
  if (NULL == ibx_name) { goto inbox_name_allocation_failed; }
  memcpy(ibx_name, name, name_len);

  void* msgs = calloc(sz, msg_sz + sizeof(x9_msg_header));
  if (NULL == msgs) { goto inbox_msgs_allocation_failed; }

  inbox->constant = UINT64_C(0xFFFFFFFFFFFFFFFF) / sz + 1;
  inbox->name     = ibx_name;
  inbox->msgs     = msgs;
  inbox->sz       = sz;
  inbox->msg_sz   = msg_sz;
  return inbox;

inbox_incorrect_size:
#ifdef X9_DEBUG
  x9_print_error_msg("INBOX_INCORRECT_SIZE");
#endif
  return NULL;

inbox_allocation_failed:
#ifdef X9_DEBUG
  x9_print_error_msg("INBOX_ALLOCATION_FAILED");
#endif
  return NULL;

inbox_name_allocation_failed:
#ifdef X9_DEBUG
  x9_print_error_msg("INBOX_NAME_ALLOCATION_FAILED");
#endif
  free(inbox);
  return NULL;

inbox_msgs_allocation_failed:
#ifdef X9_DEBUG
  x9_print_error_msg("INBOX_MSGS_ALLOCATION_FAILED");
#endif
  free(ibx_name);
  free(inbox);
  return NULL;
}

bool x9_inbox_is_valid(x9_inbox const* const inbox) {
  return !(NULL == inbox);
}

bool x9_inbox_name_is(x9_inbox const* const inbox,
                      char const* restrict const cmp) {
  return !strcmp(inbox->name, cmp) ? true : false;
}

void x9_free_inbox(x9_inbox* const inbox) {
  free(inbox->msgs);
  free(inbox->name);
  free(inbox);
}

x9_inbox* x9_select_inbox_from_node(x9_node const* const node,
                                    char const* restrict const name) {
  for (uint64_t k = 0; k != node->n_inboxes; ++k) {
    if (x9_inbox_name_is(node->inboxes[k], name)) { return node->inboxes[k]; }
  }
#ifdef X9_DEBUG
  x9_print_error_msg("NODE_DOES_NOT_CONTAIN_INBOX");
#endif
  return NULL;
}

x9_node* x9_create_node(char* restrict const name,
                        uint64_t const n_inboxes,
                        ...) {
  if (!(n_inboxes > 0)) { goto node_incorrect_definition; }

  x9_node* node = calloc(1, sizeof(x9_node));
  if (NULL == node) { goto node_allocation_failed; }

  uint64_t const name_len  = strlen(name);
  char*          node_name = calloc(name_len + 1, sizeof(char));
  if (NULL == node_name) { goto node_name_allocation_failed; }
  memcpy(node_name, name, name_len);
  node->name = node_name;

  va_list argp = {0};
  va_start(argp, n_inboxes);

  x9_inbox** inboxes = calloc(n_inboxes, sizeof(x9_inbox));
  if (NULL == inboxes) { goto node_inboxes_allocation_failed; }
  node->inboxes   = inboxes;
  node->n_inboxes = n_inboxes;

  for (uint64_t k = 0; k != n_inboxes; ++k) {
    x9_inbox* inbox = va_arg(argp, x9_inbox*);
    if (k) {
      for (uint64_t j = 0; j != k; ++j) {
        if (inbox == node->inboxes[j]) {
          goto node_multiple_equal_inboxes;
        } else {
          node->inboxes[k] = inbox;
        }
      }
    } else {
      node->inboxes[k] = inbox;
    }
  }
  va_end(argp);

  return node;

node_incorrect_definition:
#ifdef X9_DEBUG
  x9_print_error_msg("NODE_INCORRECT_DEFINITION");
#endif
  return NULL;

node_allocation_failed:
#ifdef X9_DEBUG
  x9_print_error_msg("NODE_ALLOCATION_FAILED");
#endif
  return NULL;

node_name_allocation_failed:
#ifdef X9_DEBUG
  x9_print_error_msg("NODE_NAME_ALLOCATION_FAILED");
#endif
  free(node);
  return NULL;

node_inboxes_allocation_failed:
#ifdef X9_DEBUG
  x9_print_error_msg("NODE_INBOXES_ALLOCATION_FAILED");
#endif
  va_end(argp);
  free(node_name);
  free(node);
  return NULL;

node_multiple_equal_inboxes:
#ifdef X9_DEBUG
  x9_print_error_msg("NODE_MULTIPLE_EQUAL_INBOXES");
#endif
  va_end(argp);
  free(node_name);
  free(node->inboxes);
  free(node);
  return NULL;
}

bool x9_node_is_valid(x9_node const* const node) { return !(NULL == node); }

bool x9_node_name_is(x9_node const* const node,
                     char const* restrict const cmp) {
  return !strcmp(node->name, cmp) ? true : false;
}

void x9_free_node(x9_node* const node) {
  free(node->name);
  free(node->inboxes);
  free(node);
}

void x9_free_node_and_attached_inboxes(x9_node* const node) {
  for (uint64_t k = 0; k != node->n_inboxes; ++k) {
    if (NULL != node->inboxes[k]) {
      x9_free_inbox(node->inboxes[k]);
      node->inboxes[k] = NULL;
    }
  }
  x9_free_node(node);
}

bool x9_write_to_inbox(x9_inbox* const inbox,
                       uint64_t const  msg_sz,
                       void const* restrict const msg) {
  bool                          f      = false;
  register uint64_t const       idx    = x9_load_idx(inbox, false);
  register x9_msg_header* const header = x9_header_ptr(inbox, idx);

  if (atomic_compare_exchange_strong_explicit(&header->slot_has_data, &f, true,
                                              __ATOMIC_ACQUIRE,
                                              __ATOMIC_RELAXED)) {
    memcpy((char*)header + sizeof(x9_msg_header), msg, msg_sz);
    atomic_fetch_add_explicit(&inbox->write_idx, 1, __ATOMIC_RELEASE);
    atomic_store_explicit(&header->msg_written, true, __ATOMIC_RELEASE);
    return true;
  }
  return false;
}

void x9_write_to_inbox_spin(x9_inbox* const inbox,
                            uint64_t const  msg_sz,
                            void const* restrict const msg) {
  for (;;) {
    bool                          f      = false;
    register uint64_t const       idx    = x9_increment_idx(inbox, false);
    register x9_msg_header* const header = x9_header_ptr(inbox, idx);
    if (atomic_compare_exchange_weak_explicit(&header->slot_has_data, &f, true,
                                              __ATOMIC_ACQUIRE,
                                              __ATOMIC_RELAXED)) {
      memcpy((char*)header + sizeof(x9_msg_header), msg, msg_sz);
      atomic_store_explicit(&header->msg_written, true, __ATOMIC_RELEASE);
      break;
    }
  }
}

void x9_broadcast_msg_to_all_node_inboxes(x9_node const* const node,
                                          uint64_t const       msg_sz,
                                          void const* restrict const msg) {
  for (uint64_t k = 0; k != node->n_inboxes; ++k) {
    x9_write_to_inbox_spin(node->inboxes[k], msg_sz, msg);
  }
}

bool x9_read_from_inbox(x9_inbox* const inbox,
                        uint64_t const  msg_sz,
                        void* restrict const outparam) {
  register uint64_t const       idx    = x9_load_idx(inbox, true);
  register x9_msg_header* const header = x9_header_ptr(inbox, idx);

  if (atomic_load_explicit(&header->slot_has_data, __ATOMIC_RELAXED)) {
    if (atomic_load_explicit(&header->msg_written, __ATOMIC_ACQUIRE)) {
      memcpy(outparam, (char*)header + sizeof(x9_msg_header), msg_sz);
      atomic_store_explicit(&header->msg_written, false, __ATOMIC_RELAXED);
      atomic_store_explicit(&header->slot_has_data, false, __ATOMIC_RELEASE);
      atomic_fetch_add_explicit(&inbox->read_idx, 1, __ATOMIC_RELEASE);
      return true;
    }
  }
  return false;
}

void x9_read_from_inbox_spin(x9_inbox* const inbox,
                             uint64_t const  msg_sz,
                             void* restrict const outparam) {
  register uint64_t const       idx    = x9_increment_idx(inbox, true);
  register x9_msg_header* const header = x9_header_ptr(inbox, idx);

  for (;;) {
    _mm_pause();
    if (atomic_load_explicit(&header->slot_has_data, __ATOMIC_RELAXED)) {
      if (atomic_load_explicit(&header->msg_written, __ATOMIC_ACQUIRE)) {
        memcpy(outparam, (char*)header + sizeof(x9_msg_header), msg_sz);
        atomic_store_explicit(&header->msg_written, false, __ATOMIC_RELAXED);
        atomic_store_explicit(&header->slot_has_data, false, __ATOMIC_RELEASE);
        return;
      }
    }
  }
}

bool x9_read_from_shared_inbox(x9_inbox* const inbox,
                               uint64_t const  msg_sz,
                               void* restrict const outparam) {
  bool                          f      = false;
  register uint64_t const       idx    = x9_load_idx(inbox, true);
  register x9_msg_header* const header = x9_header_ptr(inbox, idx);

  if (atomic_compare_exchange_strong_explicit(
          &header->shared, &f, true, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
    if (atomic_load_explicit(&header->slot_has_data, __ATOMIC_RELAXED)) {
      if (atomic_load_explicit(&header->msg_written, __ATOMIC_ACQUIRE)) {
        memcpy(outparam, (char*)header + sizeof(x9_msg_header), msg_sz);
        atomic_fetch_add_explicit(&inbox->read_idx, 1, __ATOMIC_RELEASE);
        atomic_store_explicit(&header->msg_written, false, __ATOMIC_RELAXED);
        atomic_store_explicit(&header->slot_has_data, false, __ATOMIC_RELEASE);
        atomic_store_explicit(&header->shared, false, __ATOMIC_RELEASE);
        return true;
      }
    }
    atomic_store_explicit(&header->shared, false, __ATOMIC_RELEASE);
  }
  return false;
}

void x9_read_from_shared_inbox_spin(x9_inbox* const inbox,
                                    uint64_t const  msg_sz,
                                    void* restrict const outparam) {
  for (;;) {
    bool                          f      = false;
    register uint64_t const       idx    = x9_increment_idx(inbox, true);
    register x9_msg_header* const header = x9_header_ptr(inbox, idx);

    if (atomic_compare_exchange_strong_explicit(
            &header->shared, &f, true, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
      if (atomic_load_explicit(&header->slot_has_data, __ATOMIC_RELAXED)) {
        if (atomic_load_explicit(&header->msg_written, __ATOMIC_ACQUIRE)) {
          memcpy(outparam, (char*)header + sizeof(x9_msg_header), msg_sz);
          atomic_store_explicit(&header->msg_written, false, __ATOMIC_RELAXED);
          atomic_store_explicit(&header->slot_has_data, false,
                                __ATOMIC_RELEASE);
          atomic_store_explicit(&header->shared, false, __ATOMIC_RELEASE);
          return;
        }
      }
      atomic_store_explicit(&header->shared, false, __ATOMIC_RELEASE);
    }
  }
}

