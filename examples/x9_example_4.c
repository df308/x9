/* x9_example_4.c
 *
 *  One producer broadcasts the same message to three inboxes.
 *  Three consumers read from each inbox.
 *  One message type.
 *
 *                   ┏━━━━━━━━┓      ┌────────┐
 *               ┌──▷┃ inbox  ┃◁─ ─ ─│Consumer│
 *               │   ┗━━━━━━━━┛      └────────┘
 *  ┌────────┐   │   ┏━━━━━━━━┓      ┌────────┐
 *  │Producer│───┼──▷┃ inbox  ┃◁─ ─ ─│Consumer│
 *  └────────┘   │   ┗━━━━━━━━┛      └────────┘
 *               │   ┏━━━━━━━━┓      ┌────────┐
 *               └──▷┃ inbox  ┃◁─ ─ ─│Consumer│
 *                   ┗━━━━━━━━┛      └────────┘
 *
 *
 *  This example showcases the use of 'x9_broadcast_msg_to_all_node_inboxes'.
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
 *   - x9_broadcast_msg_to_all_node_inboxes
 *   - x9_read_from_inbox_spin
 *   - x9_free_node_and_attached_inboxes
 *
 *  IMPORTANT:
 *   - All inboxes  must receive messages of the same type (or at least of the
 *   same size) that its being broadcasted.
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
  x9_node* node;
  char*    inbox_to_consume_from;
} th_struct;

typedef struct {
  int a;
  int b;
  int sum;
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
    x9_broadcast_msg_to_all_node_inboxes(data->node, sizeof(msg), &m);
  }
  return 0;
}

static void* consumer_fn(void* args) {
  th_struct* data = (th_struct*)args;

  x9_inbox* const inbox =
      x9_select_inbox_from_node(data->node, data->inbox_to_consume_from);
  assert(x9_inbox_is_valid(inbox));

  msg m = {0};

  for (uint64_t k = 0; k != NUMBER_OF_MESSAGES; ++k) {
    x9_read_from_inbox_spin(inbox, sizeof(msg), &m);
    assert(m.sum == (m.a + m.b));
  }
  return 0;
}

int main(void) {
  /* Seed random generator */
  srand((uint32_t)time(0));

  /* Create inboxes */
  x9_inbox* const inbox_consumer_1 = x9_create_inbox(4, "ibx_1", sizeof(msg));
  x9_inbox* const inbox_consumer_2 = x9_create_inbox(4, "ibx_2", sizeof(msg));
  x9_inbox* const inbox_consumer_3 = x9_create_inbox(4, "ibx_3", sizeof(msg));

  /* Using asserts to simplify code for presentation purpose. */
  assert(x9_inbox_is_valid(inbox_consumer_1));
  assert(x9_inbox_is_valid(inbox_consumer_2));
  assert(x9_inbox_is_valid(inbox_consumer_3));

  /* Create node */
  x9_node* const node = x9_create_node("my_node", 3, inbox_consumer_1,
                                       inbox_consumer_2, inbox_consumer_3);

  /* Assert - Same reason as above. */
  assert(x9_node_is_valid(node));

  /* Producer */
  pthread_t producer_th     = {0};
  th_struct producer_struct = {.node = node};

  /* Consumer 1 */
  pthread_t consumer_1_th     = {0};
  th_struct consumer_1_struct = {.node                  = node,
                                 .inbox_to_consume_from = "ibx_1"};
  /* Consumer 2 */
  pthread_t consumer_2_th     = {0};
  th_struct consumer_2_struct = {.node                  = node,
                                 .inbox_to_consume_from = "ibx_2"};

  /* Consumer 3 */
  pthread_t consumer_3_th     = {0};
  th_struct consumer_3_struct = {.node                  = node,
                                 .inbox_to_consume_from = "ibx_3"};

  /* Launch threads */
  pthread_create(&producer_th, NULL, producer_fn, &producer_struct);
  pthread_create(&consumer_1_th, NULL, consumer_fn, &consumer_1_struct);
  pthread_create(&consumer_2_th, NULL, consumer_fn, &consumer_2_struct);
  pthread_create(&consumer_3_th, NULL, consumer_fn, &consumer_3_struct);

  /* Join them */
  pthread_join(producer_th, NULL);
  pthread_join(consumer_1_th, NULL);
  pthread_join(consumer_2_th, NULL);
  pthread_join(consumer_3_th, NULL);

  /* Cleanup */
  x9_free_node_and_attached_inboxes(node);

  printf("TEST PASSED: x9_example_4.c\n");
  return EXIT_SUCCESS;
}
