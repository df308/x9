/* x9_example_3.c
 *
 *  Two producers and simultaneously consumers.
 *
 *  ┌────────┐       ┏━━━━━━━━┓       ┌────────┐
 *  │Producer│──────▷┃inbox 1 ┃◁ ─ ─ ─│Producer│
 *  │        │       ┗━━━━━━━━┛       │        │
 *  │  and   │                        │  and   │
 *  │        │       ┏━━━━━━━━┓       │        │
 *  │Consumer│─ ─ ─ ▷┃inbox 2 ┃◁──────│Consumer│
 *  └────────┘       ┗━━━━━━━━┛       └────────┘
 *
 *  This example showcases the use of: 'x9_write_to_inbox'
 *  and 'x9_read_from_inbox', which, unlike 'x9_write_to_inbox_spin' and
 *  'x9_read_from_inbox_spin' do not block until are able to write/read a msg.
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
 *   - x9_write_to_inbox
 *   - x9_read_from_inbox
 *   - x9_free_node_and_attached_inboxes
 *
 *  Test is considered passed iff:
 *   - None of the threads stall and exit cleanly after doing the work.
 *   - All messages sent by the producer(s) are received and asserted to be
 *   valid by the consumer(s).
 */

#include <assert.h>  /* assert */
#include <math.h>    /* fabs */
#include <pthread.h> /* pthread_t, pthread functions */
#include <stdio.h>   /* printf */
#include <stdlib.h>  /* rand, RAND_MAX */

#include "../x9.h"

/* Both producer and consumer loops, would commonly be infinite loops, but for
 * the purpose of testing a reasonable NUMBER_OF_MESSAGES is defined. */
#define NUMBER_OF_MESSAGES 1000000

typedef struct {
  x9_node* node;
} th_struct;

typedef struct {
  int a;
  int b;
  int sum;
} msg_type_1;

typedef struct {
  double x;
  double y;
  double product;
} msg_type_2;

static inline int random_int(int const min, int const max) {
  return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}

static inline double random_double(int const min, int const max) {
  return ((double)(random_int(min, max) * 1.0));
}

static inline void fill_msg_type_1(msg_type_1* const msg) {
  msg->a   = random_int(0, 10);
  msg->b   = random_int(0, 10);
  msg->sum = msg->a + msg->b;
}

static inline void fill_msg_type_2(msg_type_2* const msg) {
  msg->x       = random_double(0, 10);
  msg->y       = random_double(0, 10);
  msg->product = msg->x * msg->y;
}

static void* producer_1_fn(void* args) {
  th_struct* data = (th_struct*)args;

  x9_inbox* const write_inbox = x9_select_inbox_from_node(data->node, "ibx_1");
  assert(write_inbox);

  x9_inbox* const read_inbox = x9_select_inbox_from_node(data->node, "ibx_2");
  assert(read_inbox);

  uint64_t msgs_read = {0};
  uint64_t msgs_sent = {0};

  msg_type_2 incoming_msg = {0};
  msg_type_1 outgoing_msg = {0};

  for (;;) {
    if ((msgs_read == NUMBER_OF_MESSAGES) &&
        (msgs_sent == NUMBER_OF_MESSAGES)) {
      break;
    }

    if (msgs_sent != NUMBER_OF_MESSAGES) {
      fill_msg_type_1(&outgoing_msg);
      if (x9_write_to_inbox(write_inbox, sizeof(msg_type_1), &outgoing_msg)) {
        ++msgs_sent;
      }
    }

    if (msgs_read != NUMBER_OF_MESSAGES) {
      if (x9_read_from_inbox(read_inbox, sizeof(msg_type_2), &incoming_msg)) {
        ++msgs_read;
        assert(fabs(incoming_msg.product - (incoming_msg.x * incoming_msg.y)) <
               0.1);
      }
    }
  }

  return 0;
}

static void* producer_2_fn(void* args) {
  th_struct* data = (th_struct*)args;

  x9_inbox* const write_inbox = x9_select_inbox_from_node(data->node, "ibx_2");
  assert(write_inbox);

  x9_inbox* const read_inbox = x9_select_inbox_from_node(data->node, "ibx_1");
  assert(read_inbox);

  uint64_t msgs_read = {0};
  uint64_t msgs_sent = {0};

  msg_type_1 incoming_msg = {0};
  msg_type_2 outgoing_msg = {0};

  for (;;) {
    if ((msgs_read == NUMBER_OF_MESSAGES) &&
        (msgs_sent == NUMBER_OF_MESSAGES)) {
      break;
    }

    if (msgs_read != NUMBER_OF_MESSAGES) {
      if (x9_read_from_inbox(read_inbox, sizeof(msg_type_1), &incoming_msg)) {
        ++msgs_read;
        assert(incoming_msg.sum == (incoming_msg.a + incoming_msg.b));
      }
    }

    if (msgs_sent != NUMBER_OF_MESSAGES) {
      fill_msg_type_2(&outgoing_msg);
      if (x9_write_to_inbox(write_inbox, sizeof(msg_type_2), &outgoing_msg)) {
        ++msgs_sent;
      }
    }
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

  /* Asserts - Same reason as above.*/
  assert(x9_node_is_valid(node));

  /* Producer 1 (left on diagram) */
  pthread_t producer_1_th     = {0};
  th_struct producer_1_struct = {.node = node};

  /* Producer 2 (right on diagram) */
  pthread_t producer_2_th     = {0};
  th_struct producer_2_struct = {.node = node};

  /* Launch threads */
  pthread_create(&producer_1_th, NULL, producer_1_fn, &producer_1_struct);
  pthread_create(&producer_2_th, NULL, producer_2_fn, &producer_2_struct);

  /* Join them */
  pthread_join(producer_1_th, NULL);
  pthread_join(producer_2_th, NULL);

  /* Cleanup */
  x9_free_node_and_attached_inboxes(node);

  printf("TEST PASSED: x9_example_3.c\n");
  return EXIT_SUCCESS;
}
