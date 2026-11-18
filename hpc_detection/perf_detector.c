// HPC-based performance monitoring for a target Linux task

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <inttypes.h>

int main(int argc, char *argv[]) {
   
    // Validate command-line arguments and the optional CSV output path.
    if (argc != 2 && argc != 3){
        fprintf(stderr, "Usage: %s <PID> [output.csv]\n", argv[0]);
        return 1;
    }

    const char *csv_path = (argc == 3) ? argv[2] :"example.csv";

    // check whether the PID is a positive integer
    char *endptr;
    long num;
    errno = 0;

    num = strtol(argv[1], &endptr, 10);
    if( errno == ERANGE || num > INT_MAX ){
        printf("The PID value is out of range!\n");
        return 1;
    }else if( *endptr != '\0' || num <= 0 ){
         printf("PID must be a positive integer!\n");
        return 1;
    }

    // Configure three hardware performance events as a group:
    // retired instructions, cache misses, and cache references.
    struct perf_event_attr pe= {0}; 
    struct perf_event_attr cm = {0};
    struct perf_event_attr cr = {0};

    pe.type = PERF_TYPE_HARDWARE;  //Set hardware event type
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;  // Monitor retired instructions
    pe.size = sizeof(pe);  // set the structure size
    pe.disabled = 1;  // Initial disable the counter
    pe.exclude_kernel = 1;  // Exclude kernel-space events
    pe.exclude_hv = 1;  // Exclude hypervisor events
    pe.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;

    cm.type = PERF_TYPE_HARDWARE;
    cm.config = PERF_COUNT_HW_CACHE_MISSES;
    cm.size = sizeof(cm);  
    cm.disabled = 0;  
    cm.exclude_kernel = 1;  
    cm.exclude_hv = 1;

    cr.type = PERF_TYPE_HARDWARE;
    cr.config = PERF_COUNT_HW_CACHE_REFERENCES;
    cr.size = sizeof(cr);  
    cr.disabled = 0;  
    cr.exclude_kernel = 1;  
    cr.exclude_hv = 1; 

    // Open the instruction counter as the group leader.
    // Attach the other two counters to the same event group.
    int fd, fd_cm, fd_cr;
    fd = syscall(SYS_perf_event_open, &pe, num, -1, -1, 0);
    // Check whether the counter was opened successfully
    if (fd == -1){
        perror("errno");
        return 1;
    }
    fd_cm = syscall(SYS_perf_event_open, &cm, num, -1, fd, 0);
    if (fd_cm == -1){
        perror("errno");
        close(fd);
        return 1;
    }
    fd_cr = syscall(SYS_perf_event_open, &cr, num, -1, fd, 0);
    if (fd_cr == -1){
        perror("errno");
        close(fd);
        close(fd_cm);
        return 1;
    }
    
    // Prepare grouped counter storage and the CSV output file.
    // Group read layout:
    // count[0]: number of events
    // count[1]: total time enabled
    // count[2]: total time running
    // count[3]: retired instructions
    // count[4]: cache misses
    // count[5]: cache references
    uint64_t count[6];
    ssize_t read_b;
    int check, write_result, close_result;

    FILE *file = fopen(csv_path, "w");
    if(file == NULL){
        perror("errno");
        close(fd);
        close(fd_cm);
        close(fd_cr);
        return 1;
    }

    write_result = fprintf(file, "sample,pid,instructions,cache_misses,cache_references,time_enabled,time_running\n");
    if(write_result < 0){
            perror("File write failed");
            close(fd);
            close(fd_cm);
            close(fd_cr);
            fclose(file);
            
            return 1;
        }

    // Collect ten approximately one-second sampling windows.
    // Reset and enable the counters before each measurement.
    // Disable the counters before reading their values.
    for(int i = 0; i < 10; i++){
        check = ioctl(fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
        if(check == -1){
            perror("Counter reset failed");
            close(fd);
            close(fd_cm);
            close(fd_cr);
            fclose(file);
            return 1;
        }
        check = ioctl(fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
        if(check == -1){
            perror("Counter enable failed");
            close(fd);
            close(fd_cm);
            close(fd_cr);
            fclose(file);
            return 1;
        }
        sleep(1);
        check = ioctl(fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
        if(check == -1){
            perror("Counter disable failed");
            close(fd);
            close(fd_cm);
            close(fd_cr);
            fclose(file);
            return 1;
        }
        read_b = read(fd, count, sizeof(count));
        if (read_b == -1){
            perror("Counter read failed");
            close(fd);
            close(fd_cm);
            close(fd_cr);
            fclose(file);
            return 1;
        }else if((size_t)read_b != sizeof(count)){
            printf("Incomplete counter read\n");
            close(fd);
            close(fd_cm);
            close(fd_cr);
            fclose(file);
            return 1;
        }

        if(count[0] != 3){
            fprintf(stderr, "Incorrect number of data records returned\n");
            close(fd);
            close(fd_cm);
            close(fd_cr);
            fclose(file);
            return 1;
        }else if(count[2] == 0){
            fprintf(stderr, "Counter not running\n");
            close(fd);
            close(fd_cm);
            close(fd_cr);
            fclose(file);
            return 1;
        }else if(count[2] < count[1]){
            fprintf(stderr, "Multiplexing warning\n");
        }

        // Write the raw counter values and cumulative timing information to CSV.
        write_result = fprintf(file, "%d,%ld,%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64"\n", i+1, num, count[3], count[4], count[5], count[1], count[2]);
        if(write_result < 0){
            perror("File write failed");
            close(fd);
            close(fd_cm);
            close(fd_cr);
            fclose(file);
            
            return 1;
        }
    }

    close(fd);
    close(fd_cm);
    close(fd_cr);
    close_result = fclose(file);
    if(close_result != 0)
    {
        perror("CSV close failed");
        return 1;
    }
    return 0;
}

