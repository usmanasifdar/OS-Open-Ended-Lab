#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define HISTORY_WINDOW 10
#define CPU_TEMP_THRESHOLD 75
#define GPU_TEMP_THRESHOLD 80
#define CPU_HIGH_THRESHOLD 70
#define GPU_HIGH_THRESHOLD 70
#define NETWORK_LOW_THRESHOLD 10  // KBps

// Global State
float history_cpu[HISTORY_WINDOW] = {0};
float history_memory[HISTORY_WINDOW] = {0};
float history_gpu[HISTORY_WINDOW] = {0};
float history_network[HISTORY_WINDOW] = {0};

// Power States
char power_state_cpu[10] = "high";
char power_state_gpu[10] = "high";
char power_state_network[10] = "active";

// Helper function to shift and update history arrays
void update_history(float *history, float new_value) {
    // Shift all elements to the right
    for (int i = HISTORY_WINDOW - 1; i > 0; i--) {
        history[i] = history[i - 1];
    }
    // Add the new value at the beginning
    history[0] = new_value;
}

// Get CPU usage (Linux - using /proc/stat)
float get_cpu_usage() {
    FILE *fp;
    char line[256];
    unsigned long long int user, nice, system, idle;
    unsigned long long int total_1, total_2, idle_1, idle_2;

    fp = fopen("/proc/stat", "r");
    if (!fp) {
        perror("Error opening /proc/stat");
        return -1;
    }

    fgets(line, sizeof(line), fp);
    sscanf(line, "cpu  %llu %llu %llu %llu", &user, &nice, &system, &idle);
    fclose(fp);

    total_1 = user + nice + system + idle;
    idle_1 = idle;

    sleep(1);

    fp = fopen("/proc/stat", "r");
    if (!fp) {
        perror("Error opening /proc/stat");
        return -1;
    }

    fgets(line, sizeof(line), fp);
    sscanf(line, "cpu  %llu %llu %llu %llu", &user, &nice, &system, &idle);
    fclose(fp);

    total_2 = user + nice + system + idle;
    idle_2 = idle;

    float total_diff = total_2 - total_1;
    float idle_diff = idle_2 - idle_1;

    return 100.0 * (total_diff - idle_diff) / total_diff;
}

// Predict resource usage based on previous history (simple linear regression)
float predict_usage(float *history, int history_size) {
    if (history_size < 2) return history[0];  // Not enough data to predict

    // Calculate simple linear regression coefficients (slope)
    float sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
    for (int i = 0; i < history_size; i++) {
        sum_x += i;
        sum_y += history[i];
        sum_xy += i * history[i];
        sum_x2 += i * i;
    }

    float slope = (history_size * sum_xy - sum_x * sum_y) / (history_size * sum_x2 - sum_x * sum_x);
    float intercept = (sum_y - slope * sum_x) / history_size;

    // Predict next value based on the slope and intercept
    float predicted = slope * history_size + intercept;
    return predicted < 0 ? 0 : predicted;  // Ensure non-negative predictions
}

// Monitor and adjust temperatures
void monitor_temperature() {
    int cpu_temp = rand() % 40 + 40;  // Simulate CPU temp between 40-80°C
    int gpu_temp = rand() % 40 + 40;  // Simulate GPU temp between 40-80°C

    printf("CPU Temp: %d°C, GPU Temp: %d°C\n", cpu_temp, gpu_temp);

    if (cpu_temp > CPU_TEMP_THRESHOLD) {
        printf("Warning: CPU temperature exceeded threshold. Throttling CPU.\n");
        system("sudo cpufreq-set -g powersave");
        strcpy(power_state_cpu, "low");
    } else if (cpu_temp <= CPU_TEMP_THRESHOLD && strcmp(power_state_cpu, "low") == 0) {
        system("sudo cpufreq-set -g performance");
        strcpy(power_state_cpu, "high");
    }

    if (gpu_temp > GPU_TEMP_THRESHOLD) {
        printf("Warning: GPU temperature exceeded threshold. Throttling GPU.\n");
        strcpy(power_state_gpu, "low");
    } else if (gpu_temp <= GPU_TEMP_THRESHOLD && strcmp(power_state_gpu, "low") == 0) {
        strcpy(power_state_gpu, "high");
    }
}

// Adjust power states based on predicted resource usage
void adjust_power_states() {
    // Predict future resource usage
    float future_cpu = predict_usage(history_cpu, HISTORY_WINDOW);
    float future_gpu = predict_usage(history_gpu, HISTORY_WINDOW);
    float future_network = predict_usage(history_network, HISTORY_WINDOW);

    printf("Predicted CPU usage: %.2f%%, GPU usage: %.2f%%, Network usage: %.2f KBps\n", future_cpu, future_gpu, future_network);

    if (future_cpu > CPU_HIGH_THRESHOLD && strcmp(power_state_cpu, "high") == 0) {
        system("sudo cpufreq-set -g performance");
        strcpy(power_state_cpu, "high");
    }

    if (future_gpu > GPU_HIGH_THRESHOLD && strcmp(power_state_gpu, "high") == 0) {
        strcpy(power_state_gpu, "high");
    }

    if (future_network < NETWORK_LOW_THRESHOLD && strcmp(power_state_network, "active") == 0) {
        system("sudo ip link set eth0 down");
        strcpy(power_state_network, "idle");
    } else if (future_network >= NETWORK_LOW_THRESHOLD && strcmp(power_state_network, "idle") == 0) {
        system("sudo ip link set eth0 up");
        strcpy(power_state_network, "active");
    }
}

// Logging function
void log_status() {
    FILE *log_file = fopen("power_management.log", "a");
    if (log_file != NULL) {
        fprintf(log_file, "CPU State: %s, GPU State: %s, Network State: %s\n", power_state_cpu, power_state_gpu, power_state_network);
        fclose(log_file);
    }
}

// Main function
int main() {
    srand(time(NULL));

    while (1) {
        // Update histories with current values
        update_history(history_cpu, get_cpu_usage());
        update_history(history_memory, rand() % 100);  // Simulated memory usage
        update_history(history_gpu, rand() % 100);    // Simulated GPU usage
        update_history(history_network, rand() % 50); // Simulated network usage (KBps)

        monitor_temperature();  // Check temperatures and adjust power states
        adjust_power_states();  // Adjust based on predictions
        log_status();           // Log the current states

        sleep(2);  // Wait before next iteration
    }

    return 0;
}