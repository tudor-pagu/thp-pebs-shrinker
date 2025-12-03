#include <stdio.h>
#include <string.h>
#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_perf_event.h>

int main() {
    pfm_err_t ret;

    // Initialize libpfm
    ret = pfm_initialize();
    if (ret != PFM_SUCCESS) {
        fprintf(stderr, "pfm_initialize failed: %s\n", pfm_strerror(ret));
        return 1;
    }

    // Prepare perf_event_attr struct
    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.size = sizeof(attr);

    // Prepare pfm_perf_encode_arg_t
    pfm_perf_encode_arg_t arg;
    memset(&arg, 0, sizeof(arg));
    arg.size = sizeof(arg);
    arg.attr = &attr;

    const char* event_name = "MEM_INST_RETIRED.ALL_LOADS";

    // Get encoding for perf_event
    ret = pfm_get_os_event_encoding(event_name, PFM_PLM0 | PFM_PLM3, PFM_OS_PERF_EVENT, &arg);
    if (ret != PFM_SUCCESS) {
        fprintf(stderr, "pfm could not find event encoding: %s\n", pfm_strerror(ret));
        pfm_terminate();
        return 1;
    }

    printf("Event: %s\n", event_name);
    printf("perf_event config: 0x%llx\n", (unsigned long long)arg.attr->config);
    printf("perf_event config1: 0x%llx\n", (unsigned long long)arg.attr->config1);
    printf("perf_event config2: 0x%llx\n", (unsigned long long)arg.attr->config2);

    pfm_terminate();
    return 0;
}
