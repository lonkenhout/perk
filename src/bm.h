#ifndef __PERK_BENCHMARK_H__
#define __PERK_BENCHMARK_H__

/* definitions created by cmake for choosing benchmarks */
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

#define BM_SERVER_CYCLE 1
#ifdef BM_SERVER_CYCLE
#define bm_cycle_start(t) get_time(t);
#define bm_cycle_end(t) get_time(t);
#define bm_cycle_show(msg, t_s, t_e) print_time_diff_nano(msg, t_s, t_e);

#else
#define bm_cycle_start(t)
#define bm_cycle_end(t)
#define bm_cycle_show(msg, t_s, t_e)
#endif


#endif
