#ifndef __PEARS_BENCHMARK_H__
#define __PEARS_BENCHMARK_H__

/* definitions created by cmake for choosing benchmarks */
#ifdef BM_CLIENT_LATENCY
#define bm_client_latency_start(t) get_time(t);
#define bm_client_latency_end(t) get_time(t);
#define bm_client_latency_show(msg, t_s, t_e) print_time_diff(msg, t_s, t_e);

#else 
#define bm_client_latency_start(t)
#define bm_client_latency_stop(t)
#define bm_client_latency_show(msg, t_s, t_e)
#endif

#endif
