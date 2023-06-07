/* x9_example_1.c:
 *
 *  One producer
 *  One consumer
 *  One message type
 *
 *  ┌────────┐       ┏━━━━━━━━┓       ┌────────┐
 *  │Producer│──────▷┃ inbox  ┃◁ ─ ─ ─│Consumer│
 *  └────────┘       ┗━━━━━━━━┛       └────────┘
 *
 *  This example showcases the simplest (multi-threading) pattern.
 *
 *  Data structures used:
 *   - x9_inbox
 *
 *  Functions used:
 *   - x9_create_inbox
 *   - x9_inbox_is_valid
 *   - x9_write_to_inbox_spin
 *   - x9_read_from_inbox_spin
 *   - x9_free_inbox
 *
 *  Test is considered passed iff:
 *   - None of the threads stall and exit cleanly after doing the work.
 *   - All messages sent by the producer(s) are received and asserted to be
 *   valid by the consumer(s).
 */

#include <assert.h>  /* assert */
#include <pthread.h> /* pthread_t, pthread functions */
#include <stdio.h>   /* printf */
#include <stdlib.h>  /* rand, RAND_MAX */

#include "../x9.h"

/* Both producer and consumer loops, would commonly be infinite loops, but for
 * the purpose of testing a reasonable NUMBER_OF_MESSAGES is defined. */
#define NUMBER_OF_MESSAGES 1000000

typedef struct {
  x9_inbox* inbox;
} th_struct;

typedef struct {
  int a;
  int b;
  int sum;
} msg;

static int random_int(int const min, int const max) {
  return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}

static inline void fill_msg_1(msg* const msg) {
  msg->a   = random_int(0, 10);
  msg->b   = random_int(0, 10);
  msg->sum = msg->a + msg->b;
}

static void* producer_fn(void* args) {
  th_struct* data = (th_struct*)args;

  msg m = {0};
  for (uint64_t k = 0; k != NUMBER_OF_MESSAGES; ++k) {
    fill_msg_1(&m);
    x9_write_to_inbox_spin(data->inbox, sizeof(msg), &m);
  }
  return 0;
}

static void* consumer_fn(void* args) {
  th_struct* data = (th_struct*)args;

  msg m = {0};
  for (uint64_t k = 0; k != NUMBER_OF_MESSAGES; ++k) {
    x9_read_from_inbox_spin(data->inbox, sizeof(msg), &m);
    assert(m.sum == (m.a + m.b));
  }
  return 0;
}

int main(void) {
  /* Seed random generator */
  srand((uint32_t)time(0));

  /* Create inbox */
  x9_inbox* const inbox = x9_create_inbox(4, "ibx_1", sizeof(msg));

  /* Using assert to simplify code for presentation purpose. */
  assert(x9_inbox_is_valid(inbox));

  /* Producer */
  pthread_t producer_th     = {0};
  th_struct producer_struct = {.inbox = inbox};

  /* Consumer */
  pthread_t consumer_th     = {0};
  th_struct consumer_struct = {.inbox = inbox};

  /* Launch threads */
  pthread_create(&producer_th, NULL, producer_fn, &producer_struct);
  pthread_create(&consumer_th, NULL, consumer_fn, &consumer_struct);

  /* Join them */
  pthread_join(consumer_th, NULL);
  pthread_join(producer_th, NULL);

  /* Cleanup */
  x9_free_inbox(inbox);

  printf("TEST PASSED: x9_example_1.c\n");
  return EXIT_SUCCESS;
}
