/* x9_example_2.c
 *
 *  Two producers.
 *  One consumer and producer simultaneously.
 *  One consumer.
 *
 * ┌────────┐      ┏━━━━━━━━┓                      ┏━━━━━━━━┓
 * │Producer│─────▷┃        ┃      ┌────────┐      ┃        ┃
 * └────────┘      ┃        ┃      │Consumer│      ┃        ┃      ┌────────┐
 *                 ┃inbox 1 ┃◁ ─ ─ │  and   │─────▷┃inbox 2 ┃◁ ─ ─ │Consumer│
 * ┌────────┐      ┃        ┃      │Producer│      ┃        ┃      └────────┘
 * │Producer│─────▷┃        ┃      └────────┘      ┃        ┃
 * └────────┘      ┗━━━━━━━━┛                      ┗━━━━━━━━┛
 *
 *  This example showcase using multiple threads to write to the same inbox,
 *  using multiple message types, the 'x9_node' abstraction, and
 *  respective create/free and select functions.
 *
 *  Data structures used:
 *   - x9_inbox
 *   - x9_node
 *
 *  Functions used:
 *   - x9_create_inbox
 *   - x9_inbox_is_valid
 *   - x9_create_node
 *   - x9_node_is_valid
 *   - x9_select_inbox_from_node
 *   - x9_write_to_inbox_spin
 *   - x9_read_from_inbox_spin
 *   - x9_free_node_and_attached_inboxes
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

#define NUMBER_OF_PRODUCER_THREADS 2

typedef struct {
  x9_node* node;
} th_struct;

typedef struct {
  int a;
  int b;
  int sum;

} msg_type_1;

typedef struct {
  int x;
  int y;
  int sum;
  int product;
} msg_type_2;

static inline int random_int(int const min, int const max) {
  return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}

static inline void fill_msg_type_1(msg_type_1 msg[const static 1]) {
  msg->a   = random_int(0, 10);
  msg->b   = random_int(0, 10);
  msg->sum = msg->a + msg->b;
}

static inline void fill_msg_type_2(msg_type_2       to[const static 1],
                                   msg_type_1 const from[const static 1]) {
  to->x       = from->a;
  to->y       = from->b;
  to->sum     = from->sum;
  to->product = (from->a * from->b);
}

static void* producer_fn(void* args) {
  th_struct* data = (th_struct*)args;

  x9_inbox* const destination = x9_select_inbox_from_node(data->node, "ibx_1");
  assert(x9_inbox_is_valid(destination));

  msg_type_1 msg = {0};
  for (uint64_t k = 0; k != NUMBER_OF_MESSAGES; ++k) {
    fill_msg_type_1(&msg);
    x9_write_to_inbox_spin(destination, sizeof(msg_type_1), &msg);
  }
  return 0;
}

static void* producer_consumer_fn(void* args) {
  th_struct* data = (th_struct*)args;

  x9_inbox* const inbox = x9_select_inbox_from_node(data->node, "ibx_1");

  assert(x9_inbox_is_valid(inbox));

  x9_inbox* const destination = x9_select_inbox_from_node(data->node, "ibx_2");

  assert(x9_inbox_is_valid(destination));

  msg_type_1 incoming_msg = {0};
  msg_type_2 outgoing_msg = {0};
  for (uint64_t k = 0; k != (NUMBER_OF_MESSAGES * NUMBER_OF_PRODUCER_THREADS);
       ++k) {
    x9_read_from_inbox_spin(inbox, sizeof(msg_type_1), &incoming_msg);
    assert(incoming_msg.sum == (incoming_msg.a + incoming_msg.b));
    fill_msg_type_2(&outgoing_msg, &incoming_msg);
    x9_write_to_inbox_spin(destination, sizeof(msg_type_2), &outgoing_msg);
  }
  return 0;
}

static void* consumer_fn(void* args) {
  th_struct* data = (th_struct*)args;

  x9_inbox* const inbox = x9_select_inbox_from_node(data->node, "ibx_2");

  assert(x9_inbox_is_valid(inbox));

  msg_type_2 msg = {0};
  for (uint64_t k = 0; k != (NUMBER_OF_MESSAGES * NUMBER_OF_PRODUCER_THREADS);
       ++k) {
    x9_read_from_inbox_spin(inbox, sizeof(msg_type_2), &msg);
    assert(msg.sum == (msg.x + msg.y));
    assert(msg.product == (msg.x * msg.y));
  }
  return 0;
}

int main(void) {
  /* Seed random generator */
  srand((uint32_t)time(0));

  /* Create inboxes */
  x9_inbox* const inbox_msg_type_1 =
      x9_create_inbox(4, "ibx_1", sizeof(msg_type_1));

  x9_inbox* const inbox_msg_type_2 =
      x9_create_inbox(4, "ibx_2", sizeof(msg_type_2));

  /* Using asserts to simplify code for presentation purpose. */
  assert(x9_inbox_is_valid(inbox_msg_type_1));
  assert(x9_inbox_is_valid(inbox_msg_type_2));

  /* Create node */
  x9_node* const node =
      x9_create_node("my_node", 2, inbox_msg_type_1, inbox_msg_type_2);

  /* Assert - Same reason as above. */
  assert(x9_node_is_valid(node));

  /* Producers */
  pthread_t producer_th_1     = {0};
  th_struct producer_1_struct = {.node = node};

  pthread_t producer_th_2     = {0};
  th_struct producer_2_struct = {.node = node};

  /* Producer/Consumer */
  pthread_t producer_consumer_th = {0};
  th_struct prod_cons_struct     = {.node = node};

  /* Consumer */
  pthread_t consumer_th     = {0};
  th_struct consumer_struct = {.node = node};

  /* Launch threads */
  pthread_create(&producer_th_1, NULL, producer_fn, &producer_1_struct);
  pthread_create(&producer_th_2, NULL, producer_fn, &producer_2_struct);
  pthread_create(&consumer_th, NULL, consumer_fn, &consumer_struct);
  pthread_create(&producer_consumer_th, NULL, &producer_consumer_fn,
                 &prod_cons_struct);

  /* Join them */
  pthread_join(consumer_th, NULL);
  pthread_join(producer_consumer_th, NULL);
  pthread_join(producer_th_1, NULL);
  pthread_join(producer_th_2, NULL);

  /* Cleanup */
  x9_free_node_and_attached_inboxes(node);

  printf("TEST PASSED: x9_example_2.c\n");
  return EXIT_SUCCESS;
}
