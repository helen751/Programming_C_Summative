#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// console text colors
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define CYAN    "\033[36m"
#define RESET   "\033[0m"

// clearing the console screen
static inline void clear_console(void) {
    printf("\033c");
    fflush(stdout);
}

// displaying an error message
static inline void display_error(const char *message) {
    printf(RED "\nError: %s\n" RESET, message);
}

// displaying a success message
static inline void display_success(const char *message) {
    printf(GREEN "\n%s\n" RESET, message);
}

// displaying a warning message
static inline void display_warning(const char *message) {
    printf(YELLOW "\nWarning: %s\n" RESET, message);
}

// waiting before returning to the menu
static inline void wait_for_enter(void) {
    char input[100];

    printf(YELLOW "\nPress Enter to return to the menu..." RESET);
    fgets(input, sizeof(input), stdin);
}

// reading and validating an integer
static inline bool read_int(const char *prompt, int *number) {
    char input[100];
    char extra;

    while (true) {
        printf("%s", prompt);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            return (false);
        }

        if (sscanf(input, " %d %c", number, &extra) == 1) {
            return (true);
        }

        display_error("Enter a valid whole number.");
    }
}

// reading and validating a decimal number
static inline bool read_double(const char *prompt, double *number) {
    char input[100];
    char extra;

    while (true) {
        printf("%s", prompt);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            return (false);
        }

        if (sscanf(input, " %lf %c", number, &extra) == 1) {
            return (true);
        }

        display_error("Enter a valid number.");
    }
}

// reading and validating text
static inline bool read_text(
    const char *prompt,
    char *text,
    size_t text_size
) {
    while (true) {
        printf("%s", prompt);

        if (fgets(text, text_size, stdin) == NULL) {
            return (false);
        }

        // removing the newline added by fgets
        text[strcspn(text, "\n")] = '\0';

        if (strlen(text) > 0) {
            return (true);
        }

        display_error("Input cannot be empty.");
    }
}

#endif