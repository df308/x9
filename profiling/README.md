Notes:
---

```
x9_profiler.c

- One producer thread
- One consumer thread
- One x9_inbox

┌────────┐       ┏━━━━━━━━┓       ┌────────┐
│Producer│──────▷┃ inbox  ┃◁ ─ ─ ─│Consumer│
└────────┘       ┗━━━━━━━━┛       └────────┘
```

`x9_profiler.c` allows to test the performance of the library given different
parameters, such as the _inbox size_, _message size_ and in which _cpu cores_
the test is performed.  
It does so by having the producer/writer write _N messages_ to the _inbox_ and
the consumer/reader read them.  
All of these parameters are user defined and passed to the program as command 
line arguments, as shown below.

Additionally, two different types of tests can be run:
- **--test 1** should be used to get an idea of the raw performance of the 
library since it calls `x9_write_to_inbox_spin` and `x9_read_from_inbox_spin` 
in the background, which is ideal for low latency systems.
- **--test 2** uses the non spinning version of the functions above, and while
it will be slower, it allows to understand the _hit ratio_ of
both the producer and consumer.

_Hit ratio_ is defined as the number of messages the writer(reader) wrote(read)
divided by the number of times it attempted to write(read), and can be helpful 
when deciding in which _cpu cores_ to run specific threads.

Overall, both tests are useful for fine-tuning which _inbox size_ to use and 
gaining a better understanding of the expected performance when transmitting 
_messages_ of a given type (i.e. sizeof(some_type)).  
Moreover, they can be used to measure the impact of overclocking 
(on specific cores or as a whole).

- When `--n_its` > 1  the results presented will be the _median_ of all 
iterations.
- The writer will run on the first core of the values passed to
`--run_in_cores` and the reader in the second.

The program can be compiled with: _./compile_profiler.sh_ and run as follows:

```
Example (test 1):

$ ./X9_PROF \
  --test 1 \
  --inboxes_szs 1024,2048,4096 \
  --msgs_szs 16,32,64 \
  --n_msgs 100000000 \
  --n_its 1 \
  --run_in_cores 2,4


Inbox size | Msg size | Time (secs) | Msgs/Second
-------------------------------------------------
      1024 |       16 |        8.82 |      11.33M
      1024 |       32 |        8.70 |      11.50M
      1024 |       64 |        8.65 |      11.56M
-------------------------------------------------
      2048 |       16 |        9.10 |      10.99M
      2048 |       32 |        9.53 |      10.49M
      2048 |       64 |        9.44 |      10.60M
-------------------------------------------------
      4096 |       16 |        9.08 |      11.01M
      4096 |       32 |        9.53 |      10.50M
      4096 |       64 |        9.43 |      10.60M
-------------------------------------------------
```

```
Example (test 2):

$ ./X9_PROF \
  --test 2 \
  --inboxes_szs 1024,2048,4096 \
  --msgs_szs 16,32,64,128 \
  --n_msgs 100000000 \
  --n_its 1 \
  --run_in_cores 2,4


Inbox size | Msg size | Time (secs) | Msgs/second | Writer hit ratio | Reader hit ratio
---------------------------------------------------------------------------------------
      1024 |       16 |       12.69 |       7.88M |          100.00% |           69.92%
      1024 |       32 |       13.00 |       7.69M |          100.00% |           56.46%
      1024 |       64 |       13.79 |       7.25M |           46.85% |           99.88%
      1024 |      128 |       11.53 |       8.68M |          100.00% |           67.11%
---------------------------------------------------------------------------------------
      2048 |       16 |       11.92 |       8.39M |          100.00% |           68.22%
      2048 |       32 |       11.65 |       8.58M |          100.00% |           82.29%
      2048 |       64 |       10.05 |       9.95M |           81.91% |           99.81%
      2048 |      128 |       11.64 |       8.59M |          100.00% |           78.21%
---------------------------------------------------------------------------------------
      4096 |       16 |       12.19 |       8.21M |          100.00% |           79.13%
      4096 |       32 |       12.75 |       7.84M |          100.00% |           65.95%
      4096 |       64 |       12.10 |       8.26M |          100.00% |           67.25%
      4096 |      128 |       11.61 |       8.61M |          100.00% |           78.29%
---------------------------------------------------------------------------------------
```
