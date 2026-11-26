#include <Adafruit_LiquidCrystal.h>
#include <stdlib.h>

// storing the LCD connection
Adafruit_LiquidCrystal lcd(0);

// storing the hardware pins
const int entry_button_pin = 12;
const int exit_button_pin = 4;
const int green_led_pin = 8;
const int red_led_pin = 5;

// storing the system timing
const unsigned long button_press_interval = 50;
const unsigned long lcd_update_interval = 300;
const unsigned long welcome_duration = 2000;
const unsigned long full_blink_interval = 500;

// storing occupancy information
typedef struct {
    int current_occupancy;
    int maximum_capacity;
    int total_entries;
    int total_exits;
    int rejected_entries;
    int rejected_exits;
} OccupancyInfo;

// storing button information
typedef struct {
    int pin;
    int last_reading;
    unsigned long last_press_time;
} ButtonInfo;

// dynamically storing the occupancy record
OccupancyInfo *occupancy_info = NULL;

// storing the two button records
ButtonInfo entry_button;
ButtonInfo exit_button;

// storing the display timing and status
unsigned long last_lcd_update = 0;
unsigned long welcome_start_time = 0;
unsigned long last_full_blink = 0;

bool system_ready = false;
bool showing_welcome = true;
bool full_message_visible = true;

// declaring functions that use the custom structures
void initialize_button(ButtonInfo *button, int pin);
bool initialize_occupancy_info();
void release_occupancy_info();
bool button_was_pressed(ButtonInfo *button);
int get_available_capacity(OccupancyInfo *record);
void display_statistics(OccupancyInfo *record);
void update_led_status(OccupancyInfo *record);
void update_lcd_display(OccupancyInfo *record);
void display_welcome_message();
void display_full_message();
void update_full_alert(
    OccupancyInfo *record,
    unsigned long current_time
);
void process_entry(OccupancyInfo *record);
void process_exit(OccupancyInfo *record);
void display_system_error();

// initializing one button
void initialize_button(ButtonInfo *button, int pin) {
    button->pin = pin;
    button->last_reading = HIGH;
    button->last_press_time = 0;

    pinMode(button->pin, INPUT_PULLUP);
}

// allocating the occupancy information
bool initialize_occupancy_info() {
    occupancy_info = (OccupancyInfo *)malloc(
        sizeof(OccupancyInfo)
    );

    if (occupancy_info == NULL) {
        Serial.println(
            "Error: Occupancy memory allocation failed."
        );
        return false;
    }

    occupancy_info->current_occupancy = 0;
    occupancy_info->maximum_capacity = 10;
    occupancy_info->total_entries = 0;
    occupancy_info->total_exits = 0;
    occupancy_info->rejected_entries = 0;
    occupancy_info->rejected_exits = 0;

    return true;
}

// releasing the dynamically allocated memory
void release_occupancy_info() {
    if (occupancy_info != NULL) {
        free(occupancy_info);
        occupancy_info = NULL;
    }
}

// checking if a button has been pressed
bool button_was_pressed(ButtonInfo *button) {
    int current_reading;
    unsigned long current_time;
    bool pressed = false;

    current_reading = digitalRead(button->pin);
    current_time = millis();

    // detecting the button as soon as it is pressed
    if (
        current_reading == LOW &&
        button->last_reading == HIGH &&
        current_time - button->last_press_time >=
            button_press_interval
    ) {
        button->last_press_time = current_time;
        pressed = true;
    }

    button->last_reading = current_reading;
    return pressed;
}

// getting the number of available spaces
int get_available_capacity(OccupancyInfo *record) {
    return (
        record->maximum_capacity -
        record->current_occupancy
    );
}

// displaying occupancy statistics in the Serial Monitor
void display_statistics(OccupancyInfo *record) {
    Serial.print("Current Occupancy: ");
    Serial.println(record->current_occupancy);

    Serial.print("Available Capacity: ");
    Serial.println(get_available_capacity(record));

    Serial.print("Maximum Capacity: ");
    Serial.println(record->maximum_capacity);

    Serial.print("Total Entries: ");
    Serial.println(record->total_entries);

    Serial.print("Total Exits: ");
    Serial.println(record->total_exits);

    Serial.print("Rejected Entries: ");
    Serial.println(record->rejected_entries);

    Serial.print("Rejected Exits: ");
    Serial.println(record->rejected_exits);

    Serial.println("--------------------------------");
}

// displaying the welcome message
void display_welcome_message() {
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("Welcome to");

    lcd.setCursor(0, 1);
    lcd.print("Helen's House");
}

// displaying the full-capacity message
void display_full_message() {
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("      FULL");

    lcd.setCursor(0, 1);
    lcd.print("   No Space!");
}

// updating the green and red LEDs
void update_led_status(OccupancyInfo *record) {
    if (
        record->current_occupancy >=
        record->maximum_capacity
    ) {
        digitalWrite(green_led_pin, LOW);
        digitalWrite(red_led_pin, HIGH);
    } else {
        digitalWrite(green_led_pin, HIGH);
        digitalWrite(red_led_pin, LOW);
    }
}

// displaying occupancy information on the LCD
void update_lcd_display(OccupancyInfo *record) {
    if (
        record->current_occupancy >=
        record->maximum_capacity
    ) {
        full_message_visible = true;
        last_full_blink = millis();

        digitalWrite(green_led_pin, LOW);
        digitalWrite(red_led_pin, HIGH);

        display_full_message();
        return;
    }

    lcd.setCursor(0, 0);
    lcd.print("Inside: ");
    lcd.print(record->current_occupancy);
    lcd.print("/");
    lcd.print(record->maximum_capacity);
    lcd.print("   ");

    lcd.setCursor(0, 1);
    lcd.print("Available: ");
    lcd.print(get_available_capacity(record));
    lcd.print("   ");
}

// blinking the full message while keeping the red LED on
void update_full_alert(
    OccupancyInfo *record,
    unsigned long current_time
) {
    if (
        record->current_occupancy <
        record->maximum_capacity
    ) {
        return;
    }

    // keeping the full-capacity LED status steady
    digitalWrite(green_led_pin, LOW);
    digitalWrite(red_led_pin, HIGH);

    if (
        current_time - last_full_blink >=
        full_blink_interval
    ) {
        last_full_blink = current_time;
        full_message_visible = !full_message_visible;

        lcd.clear();

        if (full_message_visible) {
            display_full_message();
        }
    }
}
// processing a person entering the facility
void process_entry(OccupancyInfo *record) {
    if (
        record->current_occupancy >=
        record->maximum_capacity
    ) {
        record->rejected_entries++;

        Serial.println();
        Serial.println(
            "Entry Rejected: Helen's House is full."
        );

        Serial.print("Rejected Entry Attempts: ");
        Serial.println(record->rejected_entries);

        update_led_status(record);
        display_full_message();
        display_statistics(record);
        return;
    }

    record->current_occupancy++;
    record->total_entries++;

    Serial.println();
    Serial.println(
        "Entry Accepted: One person entered."
    );

    update_led_status(record);
    update_lcd_display(record);
    display_statistics(record);
}

// processing a person leaving the facility
void process_exit(OccupancyInfo *record) {
    if (record->current_occupancy <= 0) {
        record->rejected_exits++;

        Serial.println();
        Serial.println(
            "Exit Rejected: Helen's House is empty."
        );

        Serial.print("Rejected Exit Attempts: ");
        Serial.println(record->rejected_exits);

        update_led_status(record);
        update_lcd_display(record);
        display_statistics(record);
        return;
    }

    record->current_occupancy--;
    record->total_exits++;

    Serial.println();
    Serial.println(
        "Exit Accepted: One person left."
    );

    full_message_visible = true;

    update_led_status(record);
    update_lcd_display(record);
    display_statistics(record);
}

// displaying a memory allocation error
void display_system_error() {
    digitalWrite(green_led_pin, LOW);
    digitalWrite(red_led_pin, HIGH);

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("System Error");

    lcd.setCursor(0, 1);
    lcd.print("Memory Failed");
}

// preparing the complete system
void setup() {
    Serial.begin(9600);

    pinMode(green_led_pin, OUTPUT);
    pinMode(red_led_pin, OUTPUT);

    initialize_button(
        &entry_button,
        entry_button_pin
    );

    initialize_button(
        &exit_button,
        exit_button_pin
    );

    lcd.begin(16, 2);
    lcd.setBacklight(1);

    Serial.println();
    Serial.println(
        "=== HELEN'S HOUSE OCCUPANCY MONITOR ==="
    );

    system_ready = initialize_occupancy_info();

    if (!system_ready) {
        display_system_error();
        return;
    }

    digitalWrite(green_led_pin, HIGH);
    digitalWrite(red_led_pin, LOW);

    display_welcome_message();
    welcome_start_time = millis();

    Serial.println(
        "System initialized successfully."
    );

    Serial.println("Maximum Capacity: 10");
    Serial.println("Entry Button: Pin 12");
    Serial.println("Exit Button: Pin 4");
}

// continuously monitoring the buttons and display
void loop() {
    unsigned long current_time;
    bool entry_pressed;
    bool exit_pressed;

    if (
        !system_ready ||
        occupancy_info == NULL
    ) {
        return;
    }

    current_time = millis();

    // keeping the welcome message for two seconds
    if (showing_welcome) {
        if (
            current_time - welcome_start_time >=
            welcome_duration
        ) {
            showing_welcome = false;

            update_led_status(occupancy_info);
            update_lcd_display(occupancy_info);
            display_statistics(occupancy_info);
        }

        return;
    }

    entry_pressed = button_was_pressed(
        &entry_button
    );

    exit_pressed = button_was_pressed(
        &exit_button
    );

    // rejecting both buttons pressed together
    if (entry_pressed && exit_pressed) {
        Serial.println();
        Serial.println(
            "Error: Both buttons were pressed together."
        );
    } else if (entry_pressed) {
        process_entry(occupancy_info);
    } else if (exit_pressed) {
        process_exit(occupancy_info);
    }

    // blinking the full warning
    if (
        occupancy_info->current_occupancy >=
        occupancy_info->maximum_capacity
    ) {
        update_full_alert(
            occupancy_info,
            current_time
        );
        return;
    }

    // updating the LCD without stopping the program
    if (
        current_time - last_lcd_update >=
        lcd_update_interval
    ) {
        last_lcd_update = current_time;
        update_lcd_display(occupancy_info);
    }
}