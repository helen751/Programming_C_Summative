#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int threadNumber;
    const char *filename;
    const char *keyword;
    char reportFilename[500];
    int totalLines;
    int totalOccurrences;
    int matchingLines;
    int *matchingLineNumbers;
    int matchingLineCapacity;
    bool threadCreated;
    bool processingSuccessful;
    bool reportGenerated;
    char errorMessage[200];
    pthread_t thread;
} ThreadData;

// creating the report name from the input filename
void create_report_filename(const char *filename, char *reportFilename, size_t size) {
    const char *baseName;
    char name[400];
    char *extension;

    baseName = strrchr(filename, '/');

    if (baseName == NULL) {
        baseName = filename;
    } else {
        baseName++;
    }

    snprintf(name, sizeof(name), "%s", baseName);
    extension = strrchr(name, '.');

    if (extension != NULL) {
        *extension = '\0';
    }

    snprintf(reportFilename, size, "%s_report.txt", name);
}

// checking if the keyword starts at the current position
bool keyword_matches(const char *text, const char *keyword) {
    int index;

    for (index = 0; keyword[index] != '\0'; index++) {
        if (text[index] == '\0') {
            return false;
        }

        if (tolower((unsigned char)text[index]) !=
            tolower((unsigned char)keyword[index])) {
            return false;
        }
    }

    return true;
}

// counting keyword occurrences in one line
int count_keyword_occurrences(const char *line, const char *keyword) {
    int occurrences = 0;
    size_t index = 0;
    size_t keywordLength = strlen(keyword);

    if (keywordLength == 0) {
        return 0;
    }

    while (line[index] != '\0') {
        if (keyword_matches(&line[index], keyword)) {
            occurrences++;
            index += keywordLength;
        } else {
            index++;
        }
    }

    return occurrences;
}

// storing the number of a matching line
bool store_matching_line(ThreadData *data, int lineNumber) {
    int newCapacity;
    int *resizedLines;

    if (data->matchingLines == data->matchingLineCapacity) {
        if (data->matchingLineCapacity == 0) {
            newCapacity = 10;
        } else {
            newCapacity = data->matchingLineCapacity * 2;
        }

        resizedLines = realloc(
            data->matchingLineNumbers,
            (size_t)newCapacity * sizeof(int)
        );

        if (resizedLines == NULL) {
            return false;
        }

        data->matchingLineNumbers = resizedLines;
        data->matchingLineCapacity = newCapacity;
    }

    data->matchingLineNumbers[data->matchingLines] = lineNumber;
    data->matchingLines++;
    return true;
}

// generating the analysis report for one file
bool generate_analysis_report(ThreadData *data) {
    FILE *reportFile;
    int index;

    reportFile = fopen(data->reportFilename, "w");

    if (reportFile == NULL) {
        snprintf(
            data->errorMessage,
            sizeof(data->errorMessage),
            "Report could not be created: %s",
            strerror(errno)
        );
        return false;
    }

    fprintf(reportFile, "============================================\n");
    fprintf(reportFile, "       DOCUMENT ANALYSIS REPORT\n");
    fprintf(reportFile, "============================================\n\n");
    fprintf(reportFile, "Input File: %s\n", data->filename);
    fprintf(reportFile, "Search Keyword: %s\n", data->keyword);
    fprintf(reportFile, "Total Lines Processed: %d\n", data->totalLines);
    fprintf(reportFile, "Total Keyword Occurrences: %d\n", data->totalOccurrences);
    fprintf(reportFile, "Matching Lines: %d\n", data->matchingLines);
    fprintf(reportFile, "Matching Line Numbers: ");

    if (data->matchingLines == 0) {
        fprintf(reportFile, "None");
    } else {
        for (index = 0; index < data->matchingLines; index++) {
            fprintf(reportFile, "%d", data->matchingLineNumbers[index]);

            if (index < data->matchingLines - 1) {
                fprintf(reportFile, ", ");
            }
        }
    }

    fprintf(reportFile, "\nProcessing Status: %s\n",
        data->processingSuccessful ? "Successful" : "Failed");

    if (!data->processingSuccessful && data->errorMessage[0] != '\0') {
        fprintf(reportFile, "Error: %s\n", data->errorMessage);
    }

    fprintf(reportFile, "\n============================================\n");
    fclose(reportFile);
    return true;
}

// processing one input file inside its assigned thread
void *process_document(void *argument) {
    ThreadData *data = argument;
    FILE *inputFile;
    char *line = NULL;
    size_t lineCapacity = 0;
    ssize_t charactersRead;
    int lineOccurrences;

    printf(
        "[Thread %d] Processing %s ...\n",
        data->threadNumber,
        data->filename
    );

    inputFile = fopen(data->filename, "r");

    if (inputFile == NULL) {
        snprintf(
            data->errorMessage,
            sizeof(data->errorMessage),
            "Input file could not be opened: %s",
            strerror(errno)
        );

        data->processingSuccessful = false;
        data->reportGenerated = generate_analysis_report(data);

        printf(
            "[Thread %d] Error processing %s: %s\n",
            data->threadNumber,
            data->filename,
            data->errorMessage
        );

        return NULL;
    }

    while ((charactersRead = getline(
        &line,
        &lineCapacity,
        inputFile
    )) != -1) {
        (void)charactersRead;
        data->totalLines++;

        lineOccurrences = count_keyword_occurrences(
            line,
            data->keyword
        );

        if (lineOccurrences > 0) {
            data->totalOccurrences += lineOccurrences;

            if (!store_matching_line(data, data->totalLines)) {
                snprintf(
                    data->errorMessage,
                    sizeof(data->errorMessage),
                    "Memory allocation failed while storing line numbers."
                );

                data->processingSuccessful = false;
                free(line);
                fclose(inputFile);
                data->reportGenerated = generate_analysis_report(data);

                printf(
                    "[Thread %d] Error processing %s: %s\n",
                    data->threadNumber,
                    data->filename,
                    data->errorMessage
                );

                return NULL;
            }
        }
    }

    if (ferror(inputFile)) {
        snprintf(
            data->errorMessage,
            sizeof(data->errorMessage),
            "An error occurred while reading the input file."
        );
        data->processingSuccessful = false;
    } else {
        data->processingSuccessful = true;
    }

    free(line);
    fclose(inputFile);
    data->reportGenerated = generate_analysis_report(data);

    if (!data->reportGenerated) {
        data->processingSuccessful = false;

        printf(
            "[Thread %d] Report error for %s: %s\n",
            data->threadNumber,
            data->filename,
            data->errorMessage
        );
    } else if (data->processingSuccessful) {
        printf(
            "[Thread %d] Successfully processed %s\n",
            data->threadNumber,
            data->filename
        );
    } else {
        printf(
            "[Thread %d] Completed %s with an error\n",
            data->threadNumber,
            data->filename
        );
    }

    return NULL;
}

// initializing the information passed to each thread
void initialize_thread_data(
    ThreadData *data,
    int threadNumber,
    const char *filename,
    const char *keyword
) {
    memset(data, 0, sizeof(ThreadData));
    data->threadNumber = threadNumber;
    data->filename = filename;
    data->keyword = keyword;

    create_report_filename(
        filename,
        data->reportFilename,
        sizeof(data->reportFilename)
    );
}

// displaying the final processing summary
void display_processing_summary(ThreadData data[], int fileCount) {
    int index;

    printf("\n===== PROCESSING SUMMARY =====\n\n");

    for (index = 0; index < fileCount; index++) {
        printf("%s: ", data[index].filename);

        if (!data[index].threadCreated) {
            printf("Thread Creation Failed\n");
        } else if (data[index].processingSuccessful &&
            data[index].reportGenerated) {
            printf("Completed - %s\n", data[index].reportFilename);
        } else {
            printf("Failed");

            if (data[index].errorMessage[0] != '\0') {
                printf(" - %s", data[index].errorMessage);
            }

            printf("\n");
        }
    }
}

// releasing the memory used for matching line numbers
void free_thread_data(ThreadData data[], int fileCount) {
    int index;

    for (index = 0; index < fileCount; index++) {
        free(data[index].matchingLineNumbers);
    }

    free(data);
}

int main(int argc, char *argv[]) {
    const char *keyword;
    int fileCount;
    int index;
    int createdThreads = 0;
    int failedTasks = 0;
    int threadResult;
    ThreadData *threadData;

    if (argc < 3) {
        fprintf(
            stderr,
            "Usage: %s \"keyword\" file1.txt [file2.txt ...]\n",
            argv[0]
        );
        return 1;
    }

    keyword = argv[1];

    if (keyword[0] == '\0') {
        fprintf(stderr, "Error: The search keyword cannot be empty.\n");
        return 1;
    }

    fileCount = argc - 2;
    threadData = calloc((size_t)fileCount, sizeof(ThreadData));

    if (threadData == NULL) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return 1;
    }

    printf("===== MULTI-THREADED DOCUMENT ANALYZER =====\n\n");
    printf("Search Keyword: %s\n", keyword);
    printf("Number of Input Files: %d\n\n", fileCount);

    for (index = 0; index < fileCount; index++) {
        initialize_thread_data(
            &threadData[index],
            index + 1,
            argv[index + 2],
            keyword
        );

        printf(
            "[Main] Creating Thread %d for %s\n",
            index + 1,
            argv[index + 2]
        );

        threadResult = pthread_create(
            &threadData[index].thread,
            NULL,
            process_document,
            &threadData[index]
        );

        if (threadResult != 0) {
            snprintf(
                threadData[index].errorMessage,
                sizeof(threadData[index].errorMessage),
                "Thread creation failed: %s",
                strerror(threadResult)
            );

            fprintf(
                stderr,
                "[Main] Could not create Thread %d: %s\n",
                index + 1,
                strerror(threadResult)
            );

            threadData[index].processingSuccessful = false;
            threadData[index].reportGenerated =
                generate_analysis_report(&threadData[index]);
            failedTasks++;
        } else {
            threadData[index].threadCreated = true;
            createdThreads++;
        }
    }

    for (index = 0; index < fileCount; index++) {
        if (!threadData[index].threadCreated) {
            continue;
        }

        threadResult = pthread_join(threadData[index].thread, NULL);

        if (threadResult != 0) {
            snprintf(
                threadData[index].errorMessage,
                sizeof(threadData[index].errorMessage),
                "Thread join failed: %s",
                strerror(threadResult)
            );

            fprintf(
                stderr,
                "[Main] Could not join Thread %d: %s\n",
                index + 1,
                strerror(threadResult)
            );

            failedTasks++;
        } else if (!threadData[index].processingSuccessful ||
            !threadData[index].reportGenerated) {
            failedTasks++;
        }
    }

    display_processing_summary(threadData, fileCount);

    printf("\nCreated Threads: %d of %d\n", createdThreads, fileCount);

    if (failedTasks == 0) {
        printf("All processing tasks completed.\n");
        printf("Reports generated successfully.\n");
    } else {
        printf("Processing completed with %d failed task(s).\n", failedTasks);
    }

    free_thread_data(threadData, fileCount);
    return failedTasks == 0 ? 0 : 1;
}
