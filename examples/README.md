Notes: 
--- 

- Inbox N is always associated with a message of type N, hence to inbox 1
producers will write messages of type 1 and consumers will read messages of
type 1, and so on.

- Each producer/consumer runs in a unique thread.

- All x9_inbox sizes equal 4. This is not ideal for performance and should not
be used as a guideline. The reason why I use such a small buffer is because
I wanted to saturate the inbox and make it much more likely to trigger a data 
race (which there is none).

- All examples/tests can be run by executing ./run_examples.sh

```
──────▷  write to
─ ─ ─ ▷  read from
```

Examples
---
```
x9_example_1.c:

 One producer
 One consumer
 One message type
 
 ┌────────┐       ┏━━━━━━━━┓       ┌────────┐
 │Producer│──────▷┃ inbox  ┃◁ ─ ─ ─│Consumer│
 └────────┘       ┗━━━━━━━━┛       └────────┘
 
 This example showcases the simplest (multi-threading) pattern.
 
 Data structures used:
  - x9_inbox
 
 Functions used:
  - x9_create_inbox
  - x9_inbox_is_valid
  - x9_write_to_inbox_spin
  - x9_read_from_inbox_spin
  - x9_free_inbox
 
 Test is considered passed iff:
  - None of the threads stall and exit cleanly after doing the work.
  - All messages sent by the producer(s) are received and asserted to be
  valid by the consumer(s).
 ```
-------------------------------------------------------------------------------
```
x9_example_2.c

 Two producers.
 One consumer and producer simultaneously.
 One consumer.

 ┌────────┐      ┏━━━━━━━━┓                      ┏━━━━━━━━┓
 │Producer│─────▷┃        ┃      ┌────────┐      ┃        ┃
 └────────┘      ┃        ┃      │Consumer│      ┃        ┃      ┌────────┐
                 ┃inbox 1 ┃◁ ─ ─ │  and   │─────▷┃inbox 2 ┃◁ ─ ─ │Consumer│
 ┌────────┐      ┃        ┃      │Producer│      ┃        ┃      └────────┘
 │Producer│─────▷┃        ┃      └────────┘      ┃        ┃
 └────────┘      ┗━━━━━━━━┛                      ┗━━━━━━━━┛

 This example showcase using multiple threads to write to the same inbox,
 using multiple message types, the 'x9_node' abstraction, and
 respective create/free and select functions.

 Data structures used:
  - x9_inbox
  - x9_node 

 Functions used:
  - x9_create_inbox
  - x9_inbox_is_valid
  - x9_create_node
  - x9_node_is_valid
  - x9_select_inbox_from_node
  - x9_write_to_inbox_spin
  - x9_read_from_inbox_spin
  - x9_free_node_and_attached_inboxes

 Test is considered passed iff:
  - None of the threads stall and exit cleanly after doing the work.
  - All messages sent by the producer(s) are received and asserted to be
  valid by the consumer(s).
```
-------------------------------------------------------------------------------
```
x9_example_3.c

 Two producers and simultaneously consumers.
 
 ┌────────┐       ┏━━━━━━━━┓       ┌────────┐
 │Producer│──────▷┃inbox 1 ┃◁ ─ ─ ─│Producer│
 │        │       ┗━━━━━━━━┛       │        │
 │  and   │                        │  and   │
 │        │       ┏━━━━━━━━┓       │        │
 │Consumer│─ ─ ─ ▷┃inbox 2 ┃◁──────│Consumer│
 └────────┘       ┗━━━━━━━━┛       └────────┘

 This example showcases the use of: 'x9_write_to_inbox'
 and 'x9_read_from_inbox', which, unlike 'x9_write_to_inbox_spin' and
 'x9_read_from_inbox_spin' do not block until are able to write/read a msg.

 Data structures used:
  - x9_inbox
  - x9_node

 Functions used:
  - x9_create_inbox
  - x9_inbox_is_valid
  - x9_create_node
  - x9_node_is_valid
  - x9_select_inbox_from_node
  - x9_write_to_inbox
  - x9_read_from_inbox
  - x9_free_node_and_attached_inboxes

 Test is considered passed iff:
  - None of the threads stall and exit cleanly after doing the work.
  - All messages sent by the producer(s) are received and asserted to be
  valid by the consumer(s).
```
-------------------------------------------------------------------------------
```
x9_example_4.c

 One producer broadcasts the same message to three inboxes.
 Three consumers read from each inbox.
 One message type.
 
                  ┏━━━━━━━━┓      ┌────────┐
              ┌──▷┃ inbox  ┃◁─ ─ ─│Consumer│
              │   ┗━━━━━━━━┛      └────────┘
 ┌────────┐   │   ┏━━━━━━━━┓      ┌────────┐
 │Producer│───┼──▷┃ inbox  ┃◁─ ─ ─│Consumer│
 └────────┘   │   ┗━━━━━━━━┛      └────────┘
              │   ┏━━━━━━━━┓      ┌────────┐
              └──▷┃ inbox  ┃◁─ ─ ─│Consumer│
                  ┗━━━━━━━━┛      └────────┘


 This example showcases the use of 'x9_broadcast_msg_to_all_node_inboxes'.

 Data structures used:
  - x9_inbox
  - x9_node

 Functions used:
  - x9_create_inbox
  - x9_inbox_is_valid
  - x9_create_node
  - x9_node_is_valid
  - x9_broadcast_msg_to_all_node_inboxes
  - x9_read_from_inbox_spin
  - x9_free_node_and_attached_inboxes

 IMPORTANT:
  - All inboxes  must receive messages of the same type (or at least of the
  same size) that its being broadcasted.

 Test is considered passed iff:
  - None of the threads stall and exit cleanly after doing the work.
  - All messages sent by the producer(s) are received and asserted to be
  valid by the consumer(s).
```
-------------------------------------------------------------------------------
```
x9_example_5.c

 Three producers.
 Three consumers reading concurrently from the same inbox.
 One message type.

                                   ┌────────┐
 ┌────────┐       ┏━━━━━━━━┓    ─ ─│Consumer│
 │Producer│──────▷┃        ┃   │   └────────┘
 ├────────┤       ┃        ┃       ┌────────┐
 │Producer│──────▷┃ inbox  ┃◁──┤─ ─│Consumer│
 ├────────┤       ┃        ┃       └────────┘
 │Producer│──────▷┃        ┃   │   ┌────────┐
 └────────┘       ┗━━━━━━━━┛    ─ ─│Consumer│
                                   └────────┘

 This example showcases the use of 'x9_read_from_shared_inbox'.

 Data structures used:
  - x9_inbox
 
 Functions used:
  - x9_create_inbox
  - x9_inbox_is_valid
  - x9_write_to_inbox_spin
  - x9_read_from_shared_inbox
  - x9_free_inbox

 Test is considered passed iff:
  - None of the threads stall and exit cleanly after doing the work.
  - All messages sent by the producer(s) are received and asserted to be
  valid by the consumer(s).
  - Each consumer processes at least one message.
```
-------------------------------------------------------------------------------
```
x9_example_6.c

 One producer
 Two consumers reading from the same inbox concurrently (busy loop).
 One message type.

                  ┏━━━━━━━━┓       ┌────────┐
                  ┃        ┃    ─ ─│Consumer│
 ┌────────┐       ┃        ┃   │   └────────┘
 │Producer│──────▷┃ inbox  ┃◁ ─
 └────────┘       ┃        ┃   │   ┌────────┐
                  ┃        ┃    ─ ─│Consumer│
                  ┗━━━━━━━━┛       └────────┘

 This example showcases the use of 'x9_read_from_shared_inbox_spin'.

 Data structures used:
  - x9_inbox

 Functions used:
  - x9_create_inbox
  - x9_inbox_is_valid
  - x9_write_to_inbox_spin
  - x9_read_from_shared_inbox_spin
  - x9_free_inbox

 Test is considered passed iff:
  - None of the threads stall and exit cleanly after doing the work.
  - All messages sent by the producer(s) are received and asserted to be
  valid by the consumer(s).
  - Each consumer processes at least one message.
```
-------------------------------------------------------------------------------
