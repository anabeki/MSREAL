#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/fir"
#define NUM_SAMPLES 256

int main() {
    FILE *device;
    int val = 0;

    device = fopen(DEVICE_PATH, "r+");
    if (device == NULL) {
        printf("Failed to open %s\n", DEVICE_PATH);
        return EXIT_FAILURE;
    }

    // Sawtooth signal
    printf("Inputs:\n");
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        val = i%38 * 16/19 + i%5;
        if (i%38 >= 19) {
            val = 37 - val;
        }
        printf("%d\n", val);
        fprintf(device, "%d", val);
        fflush(device);
    }

    // Start
    fprintf(device, "%d", 1);
    fflush(device);

    // Polling ready
    int ready = 0;
    while (!ready) {
        fscanf(device, "%d", &ready);
        usleep(1000);
    }

    // Output
    printf("Outputs:\n");
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        fscanf(device, "%d", &val);
        printf("%d\n", val);
    }

    fclose(device);

    return 0;
}
