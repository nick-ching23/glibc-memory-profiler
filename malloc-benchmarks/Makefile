CC = gcc
CFLAGS = -O2 -g -Wall -Wextra -std=c11

# All benchmarks
BENCHES = bench_fixed bench_var bench_mt bench_churn bench_churn_mt

all: $(BENCHES)

bench_fixed: bench_fixed.c common.h
	$(CC) $(CFLAGS) -o $@ bench_fixed.c -lpthread

bench_var: bench_var.c common.h
	$(CC) $(CFLAGS) -o $@ bench_var.c -lpthread

bench_mt: bench_mt.c common.h
	$(CC) $(CFLAGS) -o $@ bench_mt.c -lpthread

bench_churn: bench_churn.c common.h
	$(CC) $(CFLAGS) -o $@ bench_churn.c -lpthread

bench_churn_mt: bench_churn_mt.c common.h
	$(CC) $(CFLAGS) -o $@ bench_churn_mt.c -lpthread

clean:
	rm -f $(BENCHES)