/* x9_profiler.c:
 *
 *  One producer
 *  One consumer
 *  One message type
 *
 *  ┌────────┐       ┏━━━━━━━━┓       ┌────────┐
 *  │Producer│──────▷┃ inbox  ┃◁ ─ ─ ─│Consumer│
 *  └────────┘       ┗━━━━━━━━┛       └────────┘
 *
 *  '--test 1' uses 'x9_write_to_inbox_spin' and 'x9_read_from_inbox_spin'
 *  '--test 2' uses x9_read_from_inbox and 'x9_read_from_inbox'
 *
 *  The advantage of '--test 2' is that, given its non spinning nature,
 *  it's possible to gather more performance metrics.
 */

#define _GNU_SOURCE  /* cpu_*, pthread_setaffinity_np */
#include <assert.h>  /* assert */
#include <getopt.h>  /* required_argument, getopt_long */
#include <pthread.h> /* pthread_t, pthread functions */
#include <stdint.h>  /* uint8_t, uint64_t, int64_t */
#include <stdio.h>   /* printf */
#include <stdlib.h>  /* qsort, rand, RAND_MAX */
#include <string.h>  /* memset, strcmp */
#include <unistd.h>  /* sysconf, _SC_NPROCESSORS_ONLN */

#include "../x9.h"

typedef enum { HEADER = 1, SEPARATOR } stdout_output;

#define ARG(arg_name) (!strcmp(long_options[option_idx].name, arg_name))

typedef struct vector {
  uint64_t size;
  uint64_t used;
  int64_t* data;
} vector;

typedef struct {
  vector* inboxes_sizes;
  vector* msgs_sizes;
  vector* run_in_cores;
  int64_t n_messages;
  int64_t n_iterations;
  int64_t test;
} perf_config;

typedef struct {
  double time_secs;
  double writer_hit_ratio;
  double reader_hit_ratio;
} perf_results;

typedef struct {
  x9_inbox* inbox;
  uint64_t  msg_sz;
  uint64_t  n_msgs;
  double    writer_hit_ratio;
  double    reader_hit_ratio;
} th_struct;

typedef struct {
  uint8_t* a;
} msg;

__attribute__((noreturn)) static void abort_test(char const* const msg) {
  printf("%s\n", msg);
  abort();
}

static vector* vector_init(uint64_t const sz) {
  if ((sz % 2)) { abort_test("ERROR: vector sz must be % 2 == 0"); }
  vector* const v = calloc(1, sizeof(vector));
  if (NULL == v) { abort_test("ERROR: failed to allocate vector."); }
  v->data = calloc(1, sz * sizeof(*v->data));
  if (NULL == v->data) { abort_test("ERROR: failed to allocate v->data."); }
  v->size = sz;
  return v;
}

static void vector_insert(vector* const v, int64_t value) {
  if (v->used == v->size) {
    v->size *= 2;
    v->data = realloc(v->data, v->size * sizeof(*v->data));
    if (NULL == v->data) { abort_test("ERROR: realloc failed."); }
  }
  v->data[v->used++] = value;
}

static void vector_free(vector* const v) {
  free(v->data);
  free(v);
}

static int random_int(int const min, int const max) {
  return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}

static void* producer_fn_test_1(void* args) {
  th_struct* data = (th_struct*)args;

  msg m = {.a = calloc(data->msg_sz, sizeof(uint8_t))};
  if (NULL == m.a) { abort_test("ERROR: failed to allocate msg buffer"); }

  for (uint64_t k = 0; k != data->n_msgs; ++k) {
    int32_t const random_val = random_int(1, 9);
    memset(m.a, random_val, data->msg_sz);
    x9_write_to_inbox_spin(data->inbox, data->msg_sz, m.a);
  }
  free(m.a);
  return 0;
}

static void* consumer_fn_test_1(void* args) {
  th_struct* data = (th_struct*)args;

  msg m = {.a = calloc(data->msg_sz, sizeof(uint8_t))};
  if (NULL == m.a) { abort_test("ERROR: failed to allocate msg buffer"); }

  for (uint64_t k = 0; k != data->n_msgs; ++k) {
    x9_read_from_inbox_spin(data->inbox, data->msg_sz, m.a);
    assert(m.a[(data->msg_sz - 1)] == (m.a[0]));
  }
  free(m.a);
  return 0;
}

static void* producer_fn_test_2(void* args) {
  th_struct* data = (th_struct*)args;

  msg m = {.a = calloc(data->msg_sz, sizeof(uint8_t))};
  if (NULL == m.a) { abort_test("ERROR: failed to allocate msg buffer"); }

  uint64_t write_attempts = 0;
  uint64_t msgs_written   = 0;

  for (;;) {
    if (msgs_written == data->n_msgs) { break; }
    int32_t const random_val = random_int(1, 9);
    memset(m.a, random_val, data->msg_sz);
    if (x9_write_to_inbox(data->inbox, data->msg_sz, m.a)) { ++msgs_written; }
    ++write_attempts;
  }

  data->writer_hit_ratio = (double)msgs_written / (double)write_attempts;
  free(m.a);
  return 0;
}

static void* consumer_fn_test_2(void* args) {
  th_struct* data = (th_struct*)args;

  msg m = {.a = calloc(data->msg_sz, sizeof(uint8_t))};
  if (NULL == m.a) { abort_test("ERROR: failed to allocate msg buffer"); }

  uint64_t read_attempts = 0;
  uint64_t msgs_read     = 0;

  for (;;) {
    if (msgs_read == data->n_msgs) { break; }
    if (x9_read_from_inbox(data->inbox, data->msg_sz, m.a)) {
      ++msgs_read;
      assert(m.a[(data->msg_sz - 1)] == (m.a[0]));
    }
    ++read_attempts;
  }

  data->reader_hit_ratio = (double)msgs_read / (double)read_attempts;
  free(m.a);
  return 0;
}

static perf_results run_test(uint64_t const ibx_sz,
                             uint64_t const msg_sz,
                             uint64_t const n_msgs,
                             uint64_t const first_core,
                             uint64_t const second_core,
                             uint64_t const test

) {
  /* Create inbox */
  x9_inbox* const inbox = x9_create_inbox(ibx_sz, "ibx_1", msg_sz);

  /* Confirm that it's valid */
  if (!(x9_inbox_is_valid(inbox))) {
    abort_test("ERROR: x9_inbox is invalid");
  }

  /* Producer */
  pthread_t      producer_th   = {0};
  pthread_attr_t producer_attr = {0};
  pthread_attr_init(&producer_attr);
  th_struct producer_struct = {
      .inbox = inbox, .msg_sz = msg_sz, .n_msgs = n_msgs};

  /* Consumer */
  pthread_t      consumer_th   = {0};
  pthread_attr_t consumer_attr = {0};
  pthread_attr_init(&consumer_attr);
  th_struct consumer_struct = {
      .inbox = inbox, .msg_sz = msg_sz, .n_msgs = n_msgs};

  /* Set affinity */
  cpu_set_t f_core = {0};
  CPU_ZERO(&f_core);
  CPU_SET(first_core, &f_core);

  cpu_set_t s_core = {0};
  CPU_ZERO(&s_core);
  CPU_SET(second_core, &s_core);

  pthread_attr_setaffinity_np(&producer_attr, sizeof(cpu_set_t), &f_core);
  pthread_attr_setaffinity_np(&consumer_attr, sizeof(cpu_set_t), &s_core);

  /* Start timer */
  struct timespec tic = {0};
  clock_gettime(CLOCK_MONOTONIC, &tic);

  /* Launch threads */
  if (1 == test) {
    pthread_create(&consumer_th, &consumer_attr, consumer_fn_test_1,
                   &consumer_struct);
    pthread_create(&producer_th, &producer_attr, producer_fn_test_1,
                   &producer_struct);

  } else {
    pthread_create(&consumer_th, &consumer_attr, consumer_fn_test_2,
                   &consumer_struct);
    pthread_create(&producer_th, &producer_attr, producer_fn_test_2,
                   &producer_struct);
  }

  /* Join them */
  pthread_join(consumer_th, NULL);
  pthread_join(producer_th, NULL);

  /* Stop timer */
  struct timespec toc = {0};
  clock_gettime(CLOCK_MONOTONIC, &toc);

  uint64_t const before =
      ((uint64_t)tic.tv_sec * 1000000000UL) + (uint64_t)tic.tv_nsec;
  uint64_t const after =
      ((uint64_t)toc.tv_sec * 1000000000UL) + (uint64_t)toc.tv_nsec;

  /* Cleanup */
  pthread_attr_destroy(&producer_attr);
  pthread_attr_destroy(&consumer_attr);
  x9_free_inbox(inbox);

  return (perf_results){.time_secs        = (double)(after - before) / 1e9,
                        .writer_hit_ratio = producer_struct.writer_hit_ratio,
                        .reader_hit_ratio = consumer_struct.reader_hit_ratio};
}

static void parse_array_arguments(char* restrict const args,
                                  vector* const write_to) {
  char*             args_start = args;
  char const* const args_end   = args + strlen(args);

  for (; args_start < args_end;) {
    char* begin = args_start;
    char* end   = args_start;
    for (; end != args_end; ++end) {
      if (',' == *end) { break; }
    }
    args_start      = end + 1;
    int64_t const n = strtoll(begin, &end, 10);
    vector_insert(write_to, n);
  }
}

static perf_config* parse_command_line_args(int argc, char** argv) {
  perf_config* config = calloc(1, sizeof(perf_config));
  if (NULL == config) {
    abort_test("ERROR: failed to allocate 'perf_config'");
  }

  for (;;) {
    int                  option_idx     = 0;
    static struct option long_options[] = {
        {"inboxes_szs", required_argument, 0, 0},
        {"msgs_szs", required_argument, 0, 0},
        {"n_msgs", required_argument, 0, 0},
        {"n_its", required_argument, 0, 0},
        {"run_in_cores", required_argument, 0, 0},
        {"test", required_argument, 0, 0},
        {0, 0, 0, 0}

    };

    int const c = getopt_long(argc, argv, "", long_options, &option_idx);
    if (-1 == c) { break; }

    switch (c) {
      case 0:
        if (ARG("inboxes_szs")) {
          config->inboxes_sizes = vector_init(8);

          parse_array_arguments(optarg, config->inboxes_sizes);

          if (!config->inboxes_sizes->data[0]) {
            abort_test(
                "ERROR: test requires at least one value for "
                "'--inboxes_sizes'");
          }

          for (uint64_t k = 0; k != config->inboxes_sizes->used; ++k) {
            int64_t const n = config->inboxes_sizes->data[k];
            if (!((n > 0) && ((n % 2) == 0))) {
              abort_test(
                  "ERROR: '--inboxes_sizes' values must be > 0 and % 2 == "
                  "0");
            }
          }
        }
        if (ARG("msgs_szs")) {
          config->msgs_sizes = vector_init(8);
          parse_array_arguments(optarg, config->msgs_sizes);

          if (!config->msgs_sizes->data[0]) {
            abort_test(
                "ERROR: test requires at least one value for "
                "'--msgs_sizes'");
          }

          for (uint64_t k = 0; k != config->msgs_sizes->used; ++k) {
            int64_t const n = config->msgs_sizes->data[k];
            if (!(n > 0)) {
              abort_test("ERROR: '--msgs_sizes' values must be > 0");
            }
          }
        }

        if (ARG("run_in_cores")) {
          config->run_in_cores = vector_init(8);
          parse_array_arguments(optarg, config->run_in_cores);

          if (config->run_in_cores->used != 2) {
            abort_test("ERROR: --run_in_cores requires two values.");
          }

          int64_t const n_cores = sysconf(_SC_NPROCESSORS_ONLN);

          if ((config->run_in_cores->data[0] < 0) ||
              config->run_in_cores->data[0] > n_cores ||
              (config->run_in_cores->data[1] < 0) ||
              config->run_in_cores->data[1] > n_cores) {
            char abort_msg[128] = {0};
            sprintf(abort_msg,
                    "ERROR: '--run_in_cores' values must be between 0 "
                    "and %ld",
                    n_cores);
            abort_test(abort_msg);
          }
        }

        if (ARG("n_msgs")) {
          int64_t const n = atoll(optarg);
          if (!(n > 0)) { abort_test("ERROR: '--n_msgs' value must be > 0"); }
          config->n_messages = n;
        }

        if (ARG("n_its")) {
          int64_t const n = atoll(optarg);
          if (!(n > 0)) { abort_test("ERROR: '--n_its' value must be > 0"); }
          config->n_iterations = n;
        }

        if (ARG("test")) {
          int64_t n = atoll(optarg);
          if (!((n > 0) && (n < 3))) {
            abort_test("ERROR: '--test' value must be either '1' or '2'");
          }
          config->test = n;
        }
    }
  }

  if (!config->test || !config->inboxes_sizes || !config->msgs_sizes ||
      !config->n_messages || !config->n_iterations || !config->run_in_cores) {
    abort_test("ERROR: missing command line arguments.");
  }

  if (1 == config->test) {
    if (config->run_in_cores->data[0] == config->run_in_cores->data[1]) {
      abort_test(
          "ERROR: for '--test 1' the values of '--run_in_cores' can not be "
          "equal because there's no sched_yield())'");
    }
  }
  return config;
}

static void free_perf_config(perf_config* config) {
  vector_free(config->inboxes_sizes);
  vector_free(config->msgs_sizes);
  vector_free(config->run_in_cores);
  free(config);
}

static void print_to_stdout(perf_config const* const config,
                            stdout_output const      what_to_print) {
  char const* const i_sz     = "Inbox size";
  char const* const sep      = " | ";
  char const* const m_sz     = "Msg size";
  char const* const time     = "Time (secs)";
  char const* const m_sec    = "Msgs/second";
  char const* const prod_hit = "Writer hit ratio";
  char const* const cons_hit = "Reader hit ratio";

  uint64_t const test_1_sep_len = strlen(i_sz) + strlen(m_sz) + strlen(time) +
                                  strlen(m_sec) + (3 * strlen(sep));

  uint64_t const test_2_sep_len = strlen(i_sz) + strlen(m_sz) + strlen(time) +
                                  strlen(m_sec) + strlen(prod_hit) +
                                  strlen(cons_hit) + (5 * strlen(sep));

  if (HEADER == what_to_print) {
    if (1 == config->test) {
      printf("\n%s%s%s%s%s%s%s\n", i_sz, sep, m_sz, sep, time, sep, m_sec);

    } else {
      printf("\n%s%s%s%s%s%s%s%s%s%s%s\n", i_sz, sep, m_sz, sep, time, sep,
             m_sec, sep, prod_hit, sep, cons_hit);
    }
  }
  if (1 == config->test) {
    for (uint64_t k = 0; k != test_1_sep_len; ++k) { fputs("-", stdout); }
  } else {
    for (uint64_t k = 0; k != test_2_sep_len; ++k) { fputs("-", stdout); }
  }
  puts("");
}

static int cmp(const void* a, const void* b) {
  return (*(const double*)a > *(const double*)b)   ? 1
         : (*(const double*)a < *(const double*)b) ? -1
                                                   : 0;
}

static double calculate_median(uint64_t const sz, double* const arr) {
  qsort(arr, sz, sizeof(double), cmp);
  return ((sz % 2) == 0) ? ((arr[sz / 2 - 1] + arr[sz / 2]) / 2) : arr[sz / 2];
}

int main(int argc, char** argv) {
  /* Seed random generator */
  srand((uint32_t)time(0));

  perf_config* config = parse_command_line_args(argc, argv);

  print_to_stdout(config, HEADER);

  double* time_secs = calloc((uint64_t)config->n_iterations, sizeof(double));
  if (NULL == time_secs) {
    abort_test("ERROR: failed to allocate 'time_secs'");
  }

  double* writer_hit_ratio =
      calloc((uint64_t)config->n_iterations, sizeof(double));
  if (NULL == writer_hit_ratio) {
    abort_test("ERROR: failed to allocate 'writer_hit_ratio'");
  }

  double* reader_hit_ratio =
      calloc((uint64_t)config->n_iterations, sizeof(double));
  if (NULL == reader_hit_ratio) {
    abort_test("ERROR: failed to allocate 'reader_hit_ratio'");
  }

  for (uint64_t k = 0; k != config->inboxes_sizes->used; ++k) {
    if (config->inboxes_sizes->data[k]) {
      for (uint64_t j = 0; j != config->msgs_sizes->used; ++j) {
        if (config->msgs_sizes->data[j]) {
          for (uint64_t it = 0; it != (uint64_t)config->n_iterations; ++it) {
            perf_results results =
                run_test((uint64_t)config->inboxes_sizes->data[k],
                         (uint64_t)config->msgs_sizes->data[j],
                         (uint64_t)config->n_messages,
                         (uint64_t)config->run_in_cores->data[0],
                         (uint64_t)config->run_in_cores->data[1],
                         (uint64_t)config->test);

            time_secs[it]        = results.time_secs;
            writer_hit_ratio[it] = results.writer_hit_ratio;
            reader_hit_ratio[it] = results.reader_hit_ratio;
          }

          double const median_secs =
              calculate_median((uint64_t)config->n_iterations, time_secs);

          printf("%10ld | ", config->inboxes_sizes->data[k]);
          printf("%8ld | ", config->msgs_sizes->data[j]);
          median_secs > 1 ? printf("%*.2f | ", 11, median_secs)
                          : printf("%*.4f | ", 11, median_secs);
          printf("%*.2fM", 10,
                 ((double)config->n_messages / median_secs) / 1e6);

          if (2 == config->test) {
            double const median_writer_hit = calculate_median(
                (uint64_t)config->n_iterations, writer_hit_ratio);

            double const median_reader_hit = calculate_median(
                (uint64_t)config->n_iterations, reader_hit_ratio);

            printf(" |%*.2f%% | ", 16, median_writer_hit * 100);
            printf("%*.2f%%", 15, median_reader_hit * 100);
          }
        }
        puts("");
      }
      print_to_stdout(config, SEPARATOR);
    }
  }

  puts("");
  free(time_secs);
  free(writer_hit_ratio);
  free(reader_hit_ratio);
  free_perf_config(config);
  return EXIT_SUCCESS;
}
