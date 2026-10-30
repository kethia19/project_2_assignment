#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#define TEMPERATURE 0
#define PRESSURE    1
#define BATTERY     2

#define NO_DATA 0
#define NORMAL  1
#define ALERT   2
#define DRIFT   3

// Settings for each device type, in the order TEMPERATURE, PRESSURE, BATTERY. 
const char *type_name[]   = { "Temperature", "Pressure", "Battery" };
const char *unit[]        = { "C", "hPa", "V" };
const float sim_min[]     = { 10.0, 900.0, 3.0 };
const float sim_max[]     = { 50.0, 1100.0, 4.4 };
const float normal_min[]  = { 15.0, 950.0, 3.3 };
const float normal_max[]  = { 40.0, 1050.0, 4.2 };
const float drift_limit[] = { 8.0, 40.0, 0.3 };
const char *status_name[] = { "NO DATA", "NORMAL", "ALERT", "DRIFT" };

struct Device {
    int id;
    char name[40];
    int type;
    union {
        float temperature;
        int pressure;
        float voltage;
    } readings;
    int status;
    float previous;
    int has_previous;
    float last_generated;
    int has_generated;
    int reading_count;
    int alert_count;
};

// A reading that has been generated but not processed yet
struct Reading {
    int device_id;
    float value;
};

// copies the device details, so it stays valid if the device is removed
struct Alert {
    int device_id;
    char device_name[40];
    int type;
    float value;
    char expected[40];
    char reason[80];
    char callback[30];
};

struct Device *devices = NULL;
int device_count = 0;

struct Reading *pending = NULL;
int pending_count = 0;

struct Alert *alerts = NULL;
int alert_count = 0;

int readings_processed = 0;
int normal_readings = 0;
int abnormal_readings = 0;
int alerts_by_type[3] = {0, 0, 0};
int drift_by_type[3] = {0, 0, 0};
int drift_checks = 0;

void free_memory(void){
    free(devices);
    free(pending);
    free(alerts);
    devices = NULL;
    pending = NULL;
    alerts = NULL;
    device_count = pending_count = alert_count = 0;
}

void input_ended(void){
    printf("\nInput ended.\n");
    free_memory();
    exit(0);
}

// Throws away whatever is left on the current input line. 
void clear_line(void){
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { }
}

// Asks until the user types a whole number between min and max. 
int read_int(const char *prompt, int min, int max){
    int value, result, next;

    while (1) {
        printf("%s", prompt);
        result = scanf("%d", &value);
        if (result == EOF)
            input_ended();
        next = getchar();
        if (next != '\n' && next != EOF)
            clear_line();
        if (result == 1 && (next == '\n' || next == EOF) && value >= min && value <= max)
            return value;
        printf("  Invalid input. Enter a whole number from %d to %d.\n", min, max);
    }
}

// Reads the union member that matches the device type. 
float get_value(struct Device *d){
    if (d->type == TEMPERATURE) return d->readings.temperature;
    if (d->type == PRESSURE)    return d->readings.pressure;
    return d->readings.voltage;
}

// Writes the union member that matches the device type. 
void set_value(struct Device *d, float value){
    if (d->type == TEMPERATURE) d->readings.temperature = value;
    else if (d->type == PRESSURE) d->readings.pressure = (int)value;
    else d->readings.voltage = value;
}

// Pressure is a whole number; the others are shown with 2 decimals. 
void print_value(int type, float value){
    if (type == PRESSURE)
        printf("%.0f %s", value, unit[type]);
    else
        printf("%.2f %s", value, unit[type]);
}

struct Device *find_device(int id){
    struct Device *d;
    for (d = devices; d < devices + device_count; d++) {
        if (d->id == id)
            return d;
    }
    return NULL;
}

// Callbacks

void save_alert(struct Device *d, const char *expected, const char *reason, const char *callback){
    struct Alert *bigger = realloc(alerts, (alert_count + 1) * sizeof(struct Alert));
    struct Alert *a;

    d->alert_count++;
    if (bigger == NULL) {
        printf(" (memory full: alert not saved)");
        return;
    }
    alerts = bigger;
    a = &alerts[alert_count];
    a->device_id = d->id;
    strcpy(a->device_name, d->name);
    a->type = d->type;
    a->value = get_value(d);
    strcpy(a->expected, expected);
    strcpy(a->reason, reason);
    strcpy(a->callback, callback);
    alert_count++;
}

// Shared by the 3 type callbacks
void check_range(struct Device *d, float value, const char *callback,
                 const char *too_low, const char *too_high){
    char expected[40];
    int t = d->type;

    sprintf(expected, "%.2f - %.2f %s", normal_min[t], normal_max[t], unit[t]);
    if (value < normal_min[t]) {
        d->status = ALERT;
        alerts_by_type[t]++;
        save_alert(d, expected, too_low, callback);
        printf(" | %s: ALERT - %s", callback, too_low);
    } else if (value > normal_max[t]) {
        d->status = ALERT;
        alerts_by_type[t]++;
        save_alert(d, expected, too_high, callback);
        printf(" | %s: ALERT - %s", callback, too_high);
    } else {
        d->status = NORMAL;
        printf(" | %s: OK", callback);
    }
}

void temperature_monitor(struct Device *d){
    check_range(d, d->readings.temperature, "temperature_monitor",
                "Too cold", "Overheating");
}

void pressure_monitor(struct Device *d){
    check_range(d, d->readings.pressure, "pressure_monitor",
                "Pressure too low (possible leak)", "Pressure too high");
}

void battery_monitor(struct Device *d){
    check_range(d, d->readings.voltage, "battery_monitor",
                "Battery low", "Over-voltage (charging fault)");
}

// Custom callback: warns when a reading jumps too far from the previous one. 
void drift_detector(struct Device *d){
    float current = get_value(d);
    float change;
    char expected[40], reason[80];
    int t = d->type;

    if (d->has_previous) {
        drift_checks++;
        change = current - d->previous;
        if (change < 0)
            change = -change;
        if (change > drift_limit[t]) {
            sprintf(expected, "change <= %.2f %s", drift_limit[t], unit[t]);
            sprintf(reason, "Sudden jump of %.2f %s (was %.2f)", change, unit[t], d->previous);
            if (d->status == NORMAL)
                d->status = DRIFT;
            drift_by_type[t]++;
            save_alert(d, expected, reason, "drift_detector");
            printf(" | drift_detector: %s", reason);
        }
    }
    d->previous = current;
    d->has_previous = 1;
}

// Runs any callback on a device
void process_device(struct Device *device, void (*callback)(struct Device *)){
    callback(device);
}

// Array of function pointers
void (*type_callback[])(struct Device *) = {
    temperature_monitor, pressure_monitor, battery_monitor
};


int add_device(int id, const char *name, int type){
    struct Device *bigger = realloc(devices, (device_count + 1) * sizeof(struct Device));
    struct Device *d;

    if (bigger == NULL)
        return 0;
    devices = bigger;
    d = &devices[device_count];
    memset(d, 0, sizeof(struct Device));
    d->id = id;
    strncpy(d->name, name, sizeof(d->name) - 1);
    d->type = type;
    d->status = NO_DATA;
    device_count++;
    return 1;
}

void register_device(void){
    char name[40];
    int id, type;

    id = read_int("Enter device ID (1-9999): ", 1, 9999);
    if (find_device(id) != NULL) {
        printf("Error: device ID %d is already registered.\n", id);
        return;
    }
    printf("Enter device name: ");
    if (scanf(" %39[^\n]", name) != 1)
        input_ended();
    clear_line();
    type = read_int("Type: 1) Temperature  2) Pressure  3) Battery: ", 1, 3) - 1;

    if (add_device(id, name, type))
        printf("Device %d registered. There are now %d devices.\n", id, device_count);
    else
        printf("Memory allocation failed. Device not registered.\n");
}

void display_devices(void){
    struct Device *d;

    if (device_count == 0) {
        printf("No devices registered.\n");
        return;
    }
    printf("%-6s %-22s %-12s %-14s %-8s %s\n", "ID", "Name", "Type", "Last reading", "Status", "Alerts");
    printf("------------------------------------------------------------------------\n");
    for (d = devices; d < devices + device_count; d++) {
        printf("%-6d %-22s %-12s ", d->id, d->name, type_name[d->type]);
        if (d->status == NO_DATA)
            printf("%-14s", "-");
        else if (d->type == PRESSURE)
            printf("%-10d %-3s", d->readings.pressure, unit[d->type]);
        else
            printf("%-10.2f %-3s", get_value(d), unit[d->type]);
        printf(" %-8s %d\n", status_name[d->status], d->alert_count);
    }
}

void remove_device(void){
    struct Device *d, *smaller;
    int id, i, position;

    if (device_count == 0) {
        printf("No devices registered.\n");
        return;
    }
    id = read_int("Enter ID of device to remove: ", 1, 9999);
    d = find_device(id);
    if (d == NULL) {
        printf("No device with ID %d.\n", id);
        return;
    }

    position = d - devices;
    for (i = position; i < device_count - 1; i++)
        devices[i] = devices[i + 1];
    device_count--;

    if (device_count == 0) {
        free(devices);
        devices = NULL;
    } else {
        smaller = realloc(devices, device_count * sizeof(struct Device));
        if (smaller != NULL)
            devices = smaller;
    }
    printf("Device %d removed. There are now %d devices. Its old alerts are kept.\n", id, device_count);
}

float random_between(float low, float high){
    return low + (high - low) * rand() / RAND_MAX;
}

// Makes up a believable reading for a device. 
float simulate(struct Device *d){
    int t = d->type;
    float value, step = (sim_max[t] - sim_min[t]) * 0.08;

    if (!d->has_generated)
        value = random_between(normal_min[t], normal_max[t]);
    else if (rand() % 4 == 0)
        value = random_between(sim_min[t], sim_max[t]);
    else
        value = d->last_generated + random_between(-step, step);

    if (value < sim_min[t]) value = sim_min[t];
    if (value > sim_max[t]) value = sim_max[t];
    if (t == PRESSURE)
        value = (int)(value + 0.5);
    else
        value = (int)(value * 100 + 0.5) / 100.0;

    d->last_generated = value;
    d->has_generated = 1;
    return value;
}

void generate_readings(void){
    struct Device *d;
    struct Reading *bigger;
    int rounds, r;

    if (device_count == 0) {
        printf("No devices registered.\n");
        return;
    }
    rounds = read_int("How many rounds? (each device makes 1 reading per round, 1-20): ", 1, 20);

    bigger = realloc(pending, (pending_count + rounds * device_count) * sizeof(struct Reading));
    if (bigger == NULL) {
        printf("Memory allocation failed.\n");
        return;
    }
    pending = bigger;

    for (r = 1; r <= rounds; r++) {
        for (d = devices; d < devices + device_count; d++) {
            pending[pending_count].device_id = d->id;
            pending[pending_count].value = simulate(d);
            printf("Round %d: device %-5d %-22s ", r, d->id, d->name);
            print_value(d->type, pending[pending_count].value);
            printf("\n");
            pending_count++;
        }
    }
    printf("%d readings are waiting. Use option 5 to process them.\n", pending_count);
}

void process_readings(void){
    struct Reading *r;
    struct Device *d;
    int alerts_before;

    if (pending_count == 0) {
        printf("No readings waiting. Use option 4 first.\n");
        return;
    }
    for (r = pending; r < pending + pending_count; r++) {
        d = find_device(r->device_id);
        if (d == NULL) {
            printf("Device %d was removed - reading skipped.\n", r->device_id);
            continue;
        }
        set_value(d, r->value);
        d->reading_count++;
        readings_processed++;
        alerts_before = d->alert_count;

        printf("%-5d %-22s %-12s ", d->id, d->name, type_name[d->type]);
        print_value(d->type, get_value(d));
        // callback chosen by device type
        process_device(d, type_callback[d->type]); 
        process_device(d, drift_detector);
        printf("\n");

        if (d->alert_count > alerts_before)
            abnormal_readings++;
        else
            normal_readings++;
    }
    free(pending);
    pending = NULL;
    pending_count = 0;
}

void display_alerts(void){
    struct Alert *a;

    if (alert_count == 0) {
        printf("No alerts yet.\n");
        return;
    }
    for (a = alerts; a < alerts + alert_count; a++) {
        printf("Alert %d\n", (int)(a - alerts) + 1);
        printf("  Device   : %d - %s (%s)\n", a->device_id, a->device_name, type_name[a->type]);
        printf("  Reading  : ");
        print_value(a->type, a->value);
        printf("\n  Expected : %s\n", a->expected);
        printf("  Reason   : %s\n", a->reason);
        printf("  Callback : %s()\n", a->callback);
    }
    printf("Total alerts: %d\n", alert_count);
}

void monitoring_summary(void){
    int t, total_drift = 0;

    printf("========== MONITORING SUMMARY ==========\n");
    printf("Registered devices  : %d\n", device_count);
    printf("Readings processed  : %d\n", readings_processed);
    printf("Normal readings     : %d\n", normal_readings);
    printf("Abnormal readings   : %d\n", abnormal_readings);
    printf("Out-of-range alerts per type:\n");
    for (t = 0; t < 3; t++)
        printf("  %-12s: %d\n", type_name[t], alerts_by_type[t]);
    printf("--- Custom callback: drift_detector ---\n");
    printf("Readings compared   : %d\n", drift_checks);
    for (t = 0; t < 3; t++) {
        printf("  %-12s: %d drift alert(s) (limit %.2f %s)\n",
               type_name[t], drift_by_type[t], drift_limit[t], unit[t]);
        total_drift += drift_by_type[t];
    }
    printf("Total drift alerts  : %d\n", total_drift);
    printf("========================================\n");
}


int main(int argc, char *argv[]){
    void (*menu[])(void) = {
        register_device, display_devices, remove_device, generate_readings,
        process_readings, display_alerts, monitoring_summary
    };
    int choice;

    if (argc > 1)
        srand(atoi(argv[1]));
    else
        srand(time(NULL));

    printf("Student ID: Kethia Kayigire\n");
    printf("Project: Dynamic IoT Event Monitoring and Callback Engine\n");
    printf("Custom Callback: Sensor Drift Detector\n");

    // 6 starting devices
    if (!add_device(101, "Server Room Temp", TEMPERATURE) ||
        !add_device(102, "Greenhouse Temp", TEMPERATURE) ||
        !add_device(201, "Water Pump Pressure", PRESSURE) ||
        !add_device(202, "Boiler Pressure", PRESSURE) ||
        !add_device(301, "Gateway Battery", BATTERY) ||
        !add_device(302, "Field Node Battery", BATTERY)) {
        printf("Memory allocation failed.\n");
        free_memory();
        return 1;
    }

    while (1) {
        printf("\n---------- MENU ----------\n");
        printf("1. Register device\n2. Show devices\n3. Remove device\n");
        printf("4. Generate readings\n5. Process readings\n6. Show alerts\n");
        printf("7. Monitoring summary\n8. Exit\n");
        choice = read_int("Choose an option: ", 1, 8);
        if (choice == 8)
            break;
        printf("\n");
        menu[choice - 1]();        // call the chosen function through its pointer 
    }

    free_memory();
    printf("Memory freed. Goodbye!\n");
    return 0;
}
