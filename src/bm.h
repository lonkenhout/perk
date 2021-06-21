#ifndef __PERK_BENCHMARK_H__
#define __PERK_BENCHMARK_H__

/* definitions created by cmake for choosing benchmarks
 * by adding environment variable these can be enabled and disabled,
 * the compiler can then remove these macros if they are defined
 * as empty macros, thus not having any impact on performance */

/* for making server exit after all clients disconnect */
#ifdef BM_SERVER_EXIT
#define PERK_SERVER_EXIT_ON_DC 1

#else
#define PERK_SERVER_EXIT_ON_DC 0
#endif

/* benchmark for latency */
#ifdef BM_LATENCY
#define bm_latency_start(t) get_time(t);
#define bm_latency_end(t) get_time(t);
#define bm_latency_show(msg, t_s, t_e) print_time_diff(msg, t_s, t_e);

#else 
#define bm_latency_start(t)
#define bm_latency_end(t)
#define bm_latency_show(msg, t_s, t_e)
#endif

/* benchmark for ops per sec */
#ifdef BM_OPS_PER_SEC
#define bm_ops_start(t) get_time(t);
#define bm_ops_end(t) get_time(t);
#define bm_ops_show(o, t_s, t_e) print_ops_per_sec(o, t_s, t_e);

#else
#define bm_ops_start(t)
#define bm_ops_end(t)
#define bm_ops_show(o, t_s, t_e)
#endif

/* count and print the number of READs, this has to be enabled manually */
//#define BM_COUNT_READS 1
#ifdef BM_COUNT_READS
#define bm_reads_incr(c) incr_num(c);
#define bm_reads_print(type, c) printf("benchmark [READcount %s][%lu READs]\n", type, c);

#else
#define bm_reads_incr(c)
#define bm_reads_print(type, c)
#endif



#endif
