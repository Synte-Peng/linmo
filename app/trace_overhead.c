#include <hal.h>
#include <linmo.h>
#include <sys/debug_trace.h>

#if CONFIG_DEBUG_TRACE

#define TRACE_OVERHEAD_ITERATIONS 1024U

static inline uint32_t read_cycle(void)
{
    return read_csr(mcycle);
}

static inline uint32_t read_instret(void)
{
    return read_csr(minstret);
}

typedef struct {
    uint32_t cycles;
    uint32_t instructions;
} trace_measurement_t;

static trace_measurement_t measure_empty_loop(uint32_t iterations)
{
    trace_measurement_t result;
    uint32_t start_cycles;
    uint32_t start_insts;
    uint32_t end_cycles;
    uint32_t end_insts;

    start_cycles = read_cycle();
    start_insts = read_instret();
    for (uint32_t i = 0; i < iterations; i++)
        asm volatile("" ::: "memory");
    end_insts = read_instret();
    end_cycles = read_cycle();

    result.cycles = end_cycles - start_cycles;
    result.instructions = end_insts - start_insts;
    return result;
}

static trace_measurement_t measure_trace_loop(uint32_t iterations)
{
    trace_measurement_t result;
    uint32_t start_cycles;
    uint32_t start_insts;
    uint32_t end_cycles;
    uint32_t end_insts;

    debug_clear_events();

    start_cycles = read_cycle();
    start_insts = read_instret();
    for (uint32_t i = 0; i < iterations; i++)
        debug_trace_event(EVENT_TASK_YIELD, i, i + 1U);
    end_insts = read_instret();
    end_cycles = read_cycle();

    result.cycles = end_cycles - start_cycles;
    result.instructions = end_insts - start_insts;
    return result;
}

int32_t app_main(void)
{
    int32_t irq_state;
    trace_measurement_t baseline;
    trace_measurement_t trace;
    uint32_t net_cycles;
    uint32_t net_insts;
    uint32_t cycles_per_event;
    uint32_t cycles_per_event_x100;
    uint32_t insts_per_event;
    uint32_t insts_per_event_x100;

    irq_state = _di();
    baseline = measure_empty_loop(TRACE_OVERHEAD_ITERATIONS);
    trace = measure_trace_loop(TRACE_OVERHEAD_ITERATIONS);
    hal_interrupt_set(irq_state);

    net_cycles = trace.cycles - baseline.cycles;
    net_insts = trace.instructions - baseline.instructions;
    cycles_per_event = net_cycles / TRACE_OVERHEAD_ITERATIONS;
    cycles_per_event_x100 = (net_cycles * 100U) / TRACE_OVERHEAD_ITERATIONS;
    insts_per_event = net_insts / TRACE_OVERHEAD_ITERATIONS;
    insts_per_event_x100 =
        (net_insts * 100U) / TRACE_OVERHEAD_ITERATIONS;

    mo_logger_flush();

    printf("\n=== Debug Trace Overhead Benchmark ===\n");
    printf("iterations: %u\n", TRACE_OVERHEAD_ITERATIONS);
    printf("baseline cycles: %u\n", baseline.cycles);
    printf("trace cycles: %u\n", trace.cycles);
    printf("net trace cycles: %u\n", net_cycles);
    printf("cycles per event: %u.%02u\n", cycles_per_event,
           cycles_per_event_x100 % 100U);
    printf("baseline instructions: %u\n", baseline.instructions);
    printf("trace instructions: %u\n", trace.instructions);
    printf("net trace instructions: %u\n", net_insts);
    printf("instructions per event: %u.%02u\n", insts_per_event,
           insts_per_event_x100 % 100U);
    printf("retained events: %u\n", debug_trace_count());
    printf("overwrites: %u\n", debug_trace_overwrites());
    printf("Overall: %s\n",
           (net_cycles > 0U && net_insts > 0U) ? "PASS" : "FAIL");

    *(volatile uint32_t *) 0x100000U = 0x5555U;

    for (;;)
        mo_task_wfi();
}

#else

int32_t app_main(void)
{
    printf("CONFIG_DEBUG_TRACE disabled; benchmark skipped\n");
    mo_logger_flush();
    *(volatile uint32_t *) 0x100000U = 0x5555U;

    return 0;
}

#endif
