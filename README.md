X9
---

X9 is a low level high performance message passing library, based on a
lock-free ring buffer implemented with atomic variables, for low latency
multithreading work.  
It allows for multiple producers/consumers to concurrently access the same
underlying ring buffer and provides both spinning (busy loop) and non-blocking
read and write functions.

The library is based on three concepts:

- A message, which is a user defined struct.
- A `x9_inbox`, which is where messages are both written to and read from.
- A `x9_node`, which is an abstraction that unifies x9_inbox(es).

The library provides multiple functions to both read from and write to a
`x9_inbox`, as the right choice depends on the user needs.  
Refer to _x9.h_, where all public functions are properly documented and their
use cases explained, and the examples folder for comprehensive examples of
different architectures.

Enabling `X9_DEBUG` at compile time will print to stdout the reason why the
functions `x9_inbox_is_valid` and `x9_node_is_valid` returned 'false' (if they
indeed returned 'false'), or why `x9_select_inbox_from_node` did not return a
valid `x9_inbox`.

To use the library just link with x9.c and include x9.h where necessary.

X9 is as generic, performant and intuitive as C allows, without forcing the
user to any sort of build system preprocessor hell, pseudo-C macro based
library, or worse.  
It was originally written in the context of an high-frequency-trading system
that this author developed, and was made public in June of 2023.  
It is released under the BSD-2-Clause license, with the purpose of serving
others and other programs.  

Benchmarks
---

- Single producer and single consumer transmitting 100M messages via a single 
`x9_inbox`.  
- Run on Intel 11900k (cpu and ram untuned).  
- _Msg size_ expressed in bytes, and _Inbox size_ in number of slots in the 
ring buffer.   
- _(See /profiling for how to run your own tests)_  

```
Inbox size | Msg size | Time (secs) | Msgs/second
-------------------------------------------------
      1024 |       16 |        4.00 |      25.01M
      1024 |       32 |        4.03 |      24.82M
      1024 |       64 |        4.17 |      23.99M
-------------------------------------------------
      2048 |       16 |        3.90 |      25.63M
      2048 |       32 |        3.96 |      25.26M
      2048 |       64 |        4.09 |      24.43M
-------------------------------------------------
      4096 |       16 |        3.86 |      25.88M
      4096 |       32 |        3.92 |      25.50M
      4096 |       64 |        4.05 |      24.67M
-------------------------------------------------
```
