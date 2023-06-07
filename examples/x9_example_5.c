/* x9_example_5.c
 *
 *  Three producers.
 *  Three consumers reading concurrently from the same inbox.
 *  One message type.
 *
 *                                    ┌────────┐
 *  ┌────────┐       ┏━━━━━━━━┓    ─ ─│Consumer│
 *  │Producer│──────▷┃        ┃   │   └────────┘
 *  ├────────┤       ┃        ┃       ┌────────┐
 *  │Producer│──────▷┃ inbox  ┃◁──┤─ ─│Consumer│
 *  ├────────┤       ┃        ┃       └────────┘
 *  │Producer│──────▷┃        ┃   │   ┌────────┐
 *  └────────┘       ┗━━━━━━━━┛    ─ ─│Consumer│
 *                                    └────────┘
 *
 *  This example showcases the use of 'x9_read_from_shared_inbox'.
 *
 *  Data structures used:
 *   - x9_inbox
 *
 *  Functions used:
 *   - x9_create_inbox
 *   - x9_inbox_is_valid
 *   - x9_write_to_inbox_spin
 *   - x9_read_from_shared_inbox
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
#include <stdbool.h>   /* bool */
#include <stdint.h>    /* uint64_t */
#include <stdio.h>     /* printf */
#include <stdlib.h>    /* rand, RAND_MAX */

#include "../x9.h"

/* Both producer and consumer loops, would commonly be infinite loops, but for
 * the purpose of testing a reasonable NUMBER_OF_MESSAGES is defined. */
#define NUMBER_OF_MESSAGES 1000000

#define NUMBER_OF_PRODUCER_THREADS 3

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
  for (uint_fast64_t k = 0; k != NUMBER_OF_MESSAGES; ++k) {
    fill_msg_type(&m);
    if (k == (NUMBER_OF_MESSAGES - 1)) { m.last_message = true; }
    x9_write_to_inbox_spin(data->inbox, sizeof(msg), &m);
  }
  return 0;
}

static void* consumer_fn(void* args) {
  th_struct* data = (th_struct*)args;

  msg m = {0};
  for (;;) {
    if (x9_read_from_shared_inbox(data->inbox, sizeof(msg), &m)) {
      assert(m.sum == (m.a + m.b));
      ++data->msgs_read;
      if (m.last_message) { return 0; }
    }
  }
}

int main(void) {
  /* Seed random generator */
  srand((uint32_t)time(0));

  /* Create inbox */
  x9_inbox* const inbox = x9_create_inbox(4, "ibx", sizeof(msg));

  /* Using assert to simplify code for presentation purpose. */
  assert(x9_inbox_is_valid(inbox));

  /* Producers */
  pthread_t producer_1_th     = {0};
  th_struct producer_1_struct = {.inbox = inbox};

  pthread_t producer_2_th     = {0};
  th_struct producer_2_struct = {.inbox = inbox};

  pthread_t producer_3_th     = {0};
  th_struct producer_3_struct = {.inbox = inbox};

  /* Consumers */
  pthread_t consumer_1_th     = {0};
  th_struct consumer_1_struct = {.inbox = inbox};

  pthread_t consumer_2_th     = {0};
  th_struct consumer_2_struct = {.inbox = inbox};

  pthread_t consumer_3_th     = {0};
  th_struct consumer_3_struct = {.inbox = inbox};

  /* Launch threads */
  pthread_create(&producer_1_th, NULL, producer_fn, &producer_1_struct);
  pthread_create(&producer_2_th, NULL, producer_fn, &producer_2_struct);
  pthread_create(&producer_3_th, NULL, producer_fn, &producer_3_struct);
  pthread_create(&consumer_1_th, NULL, consumer_fn, &consumer_1_struct);
  pthread_create(&consumer_2_th, NULL, consumer_fn, &consumer_2_struct);
  pthread_create(&consumer_3_th, NULL, consumer_fn, &consumer_3_struct);

  /* Join them */
  pthread_join(producer_1_th, NULL);
  pthread_join(producer_2_th, NULL);
  pthread_join(producer_3_th, NULL);
  pthread_join(consumer_1_th, NULL);
  pthread_join(consumer_2_th, NULL);
  pthread_join(consumer_3_th, NULL);

  /* Assert that all of the consumers read from the shared inbox. */
  assert(consumer_1_struct.msgs_read > 0);
  assert(consumer_2_struct.msgs_read > 0);
  assert(consumer_3_struct.msgs_read > 0);

  /* Assert that the total number of messages read == (NUMBER_OF_MESSAGES *
   * NUMBER_OF_PRODUCER_THREADS) */
  assert((NUMBER_OF_MESSAGES * NUMBER_OF_PRODUCER_THREADS) ==
         (consumer_1_struct.msgs_read + consumer_2_struct.msgs_read +
          consumer_3_struct.msgs_read));

  /* Cleanup */
  x9_free_inbox(inbox);

  printf("TEST PASSED: x9_example_5.c\n");
  return EXIT_SUCCESS;
}
