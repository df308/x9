/* x9_example_6.c
 *
 *  One producer
 *  Two consumers reading from the same inbox concurrently (busy loop).
 *  One message type.
 *
 *                   ┏━━━━━━━━┓       ┌────────┐
 *                   ┃        ┃    ─ ─│Consumer│
 *  ┌────────┐       ┃        ┃   │   └────────┘
 *  │Producer│──────▷┃ inbox  ┃◁ ─
 *  └────────┘       ┃        ┃   │   ┌────────┐
 *                   ┃        ┃    ─ ─│Consumer│
 *                   ┗━━━━━━━━┛       └────────┘
 *
 *  This example showcases the use of 'x9_read_from_shared_inbox_spin'.
 *
 *  Data structures used:
 *   - x9_inbox
 *
 *  Functions used:
 *   - x9_create_inbox
 *   - x9_inbox_is_valid
 *   - x9_write_to_inbox_spin
 *   - x9_read_from_shared_inbox_spin
 *   - x9_free_inbox
 *
 *  Test is considered passed iff:
 *   - None of the threads stall and exit cleanly after doing the work.
 *   - All messages sent by the producer(s) are received and asserted to be
 *   valid by the consumer(s).
 *   - Each consumer processes at least one message.
 */

#include <assert.h>    /* assert */
#include <pthread.h>   /* pthread_t, pthread functions */
#include <stdatomic.h> /* atomic_* */
#include <stdbool.h>   /* bool */
#include <stdint.h>    /* uint64_t */
#include <stdio.h>     /* printf */
#include <stdlib.h>    /* rand, RAND_MAX */

#include "../x9.h"

/* Both producer and consumer loops, would commonly be infinite loops, but for
 * the purpose of testing a reasonable NUMBER_OF_MESSAGES is defined. */
#define NUMBER_OF_MESSAGES 1000000

typedef struct {
  x9_inbox* inbox;
  uint64_t  msgs_read;
} th_struct;

typedef struct {
  int  a;
  int  b;
  int  sum;
  bool last_message;
  char pad[3];
} msg;

static inline int random_int(int const min, int const max) {
  return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}

static inline void fill_msg_type(msg* const m) {
  m->a   = random_int(0, 10);
  m->b   = random_int(0, 10);
  m->sum = m->a + m->b;
}

static void* producer_fn(void* args) {
  th_struct* data = (th_struct*)args;

  msg m = {0};
  for (uint_fast64_t k = 0; k != (NUMBER_OF_MESSAGES); ++k) {
    fill_msg_type(&m);
    if (k == (NUMBER_OF_MESSAGES - 1)) { m.last_message = true; }
    x9_write_to_inbox_spin(data->inbox, sizeof(msg), &m);
  }
  return 0;
}

static void* consumer_fn(void* args) {
  th_struct* data = (th_struct*)args;
  msg        m    = {0};
  for (;;) {
    x9_read_from_shared_inbox_spin(data->inbox, sizeof(msg), &m);
    assert(m.sum == (m.a + m.b));

    /* The First thread to read the 'last message' writes the same message back
     * to the inbox, so in case the second thread had already entered
     * 'x9_read_from_shared_inbox_spin' function, it will be able to read the
     * message and do a clean exit.
     * If the second thread, at the time that the first thread set
     * 'g_read_last= true' to hasn't entered 'x9_write_to_inbox_spin', then it
     * will check the if(g_read_last) condition above and exit.
     * The first case would equal NUMBER_OF_MESSAGES + 1 read.
     * The second case, NUMBER_OF_MESSAGES.
     * Given the implementation of 'x9_read_from_shared_inbox_spin', this is
     * the only way to get a clean exit without killing the second thread. */
    if (m.last_message) {
      x9_write_to_inbox_spin(data->inbox, sizeof(msg), &m);
      ++data->msgs_read;
      return 0;
    }
    ++data->msgs_read;
  }
}

int main(void) {
  /* Seed random generator */
  srand((uint32_t)time(0));

  /* Create inbox */
  x9_inbox* const inbox = x9_create_inbox(4, "ibx", sizeof(msg));

  /* Using assert to simplify code for presentation purpose */
  assert(x9_inbox_is_valid(inbox));

  /* Producer */
  pthread_t producer_th     = {0};
  th_struct producer_struct = {.inbox = inbox};

  /* Consumers */
  pthread_t consumer_1_th     = {0};
  th_struct consumer_1_struct = {.inbox = inbox};

  pthread_t consumer_2_th     = {0};
  th_struct consumer_2_struct = {.inbox = inbox};

  /* Launch threads */
  pthread_create(&producer_th, NULL, producer_fn, &producer_struct);
  pthread_create(&consumer_1_th, NULL, consumer_fn, &consumer_1_struct);
  pthread_create(&consumer_2_th, NULL, consumer_fn, &consumer_2_struct);

  /* Join them */
  pthread_join(producer_th, NULL);
  pthread_join(consumer_1_th, NULL);
  pthread_join(consumer_2_th, NULL);

  /* Assert that all of the consumers read from the shared inbox. */
  assert(consumer_1_struct.msgs_read > 0);
  assert(consumer_2_struct.msgs_read > 0);

  /* Assert that the total number of messages read == NUMBER_OF_MESSAGES or
   * NUMBER_OF_MESSAGES + 1 */
  assert((NUMBER_OF_MESSAGES + 1) ==
             (consumer_1_struct.msgs_read + consumer_2_struct.msgs_read) ||
         (NUMBER_OF_MESSAGES) ==
             (consumer_1_struct.msgs_read + consumer_2_struct.msgs_read)

  );

  /* Cleanup */
  x9_free_inbox(inbox);

  printf("TEST PASSED: x9_example_6.c\n");
  return EXIT_SUCCESS;
}
