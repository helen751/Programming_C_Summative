#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>

#include "../utils.h"

typedef struct {
    char courseCode[20];
    char courseName[100];
    double maximumMark;
} CourseOption;

typedef struct {
    char courseCode[20];
    char courseName[100];
    double mark;
    double maximumMark;
} Course;

typedef struct {
    int studentId;
    char firstName[50];
    char lastName[50];
    char programme[100];
    int yearOfStudy;
    int courseCount;
    Course *courses;
} Student;

CourseOption courseCatalogue[] = {
    {"BSE101", "Intro to Software Engineering", 100},
    {"CSC101", "Intro to Programming",  100},
    {"CSC102", "Data Structures",  100},
    {"MAT101", "Mathematics for ML",  100},
    {"DBS101", "Relational Database",  100},
    {"NET101", "Computer Networks",  100},
    {"WEB101", "Web Development",  100},
    {"STA101", "Data Statistics",  100}
};

int courseCatalogueCount =
    sizeof(courseCatalogue) / sizeof(courseCatalogue[0]);

// dynamically storing all registered students
Student *students = NULL;
int studentCount = 0;
const char *studentFilename = "students.dat";

// releasing all allocated student records
void free_students(void) {
    int index;

    for (index = 0; index < studentCount; index++) {
        free(students[index].courses);
    }

    free(students);
    students = NULL;
    studentCount = 0;
}

// saving all students and their courses
bool save_students(void) {
    FILE *file;
    Student *student;
    int index;

    file = fopen(studentFilename, "wb");

    if (file == NULL) {
        perror(RED "Unable to open the student file." RESET);
        return (false);
    }

    if (fwrite(&studentCount, sizeof(int), 1, file) != 1) {
        fclose(file);
        return (false);
    }

    for (index = 0; index < studentCount; index++) {
        student = &students[index];

        if (
            fwrite(&student->studentId, sizeof(int), 1, file) != 1 ||
            fwrite(student->firstName, sizeof(student->firstName), 1, file) != 1 ||
            fwrite(student->lastName, sizeof(student->lastName), 1, file) != 1 ||
            fwrite(student->programme, sizeof(student->programme), 1, file) != 1 ||
            fwrite(&student->yearOfStudy, sizeof(int), 1, file) != 1 ||
            fwrite(&student->courseCount, sizeof(int), 1, file) != 1
        ) {
            fclose(file);
            return (false);
        }

        if (student->courseCount > 0 &&
            fwrite(student->courses,sizeof(Course),student->courseCount,file) != (size_t)student->courseCount) {
            fclose(file);
            return (false);
        }
    }

    fclose(file);
    return (true);
}

// loading all saved students and their courses
int load_students(void) {
    FILE *file;
    Student *student;
    int savedCount;
    int index;

    file = fopen(studentFilename, "rb");

    // starting normally when no file exists yet
    if (file == NULL) {
        return (0);
    }

    if (fread(&savedCount, sizeof(int), 1, file) != 1 ||savedCount < 0) {
        fclose(file);
        display_error("Invalid student file.");
        return (0);
    }

    if (savedCount == 0) {
        fclose(file);
        return (0);
    }

    students = calloc(savedCount, sizeof(Student));

    if (students == NULL) {
        fclose(file);
        display_error("Unable to allocate memory for students.");
        return (0);
    }

    studentCount = savedCount;

    for (index = 0; index < studentCount; index++) {
        student = &students[index];

        if (fread(&student->studentId, sizeof(int), 1, file) != 1 ||
            fread(student->firstName, sizeof(student->firstName), 1, file) != 1 ||
            fread(student->lastName, sizeof(student->lastName), 1, file) != 1 ||
            fread(student->programme, sizeof(student->programme), 1, file) != 1 ||
            fread(&student->yearOfStudy, sizeof(int), 1, file) != 1 ||
            fread(&student->courseCount, sizeof(int), 1, file) != 1 ||
            student->courseCount < 0) {
            fclose(file);
            free_students();
            display_error("The student records could not be loaded.");
            return (0);
        }

        if (student->courseCount > 0) {
            student->courses = malloc(student->courseCount * sizeof(Course));

            if (
                student->courses == NULL ||
                fread(student->courses,sizeof(Course),student->courseCount,file) != (size_t)student->courseCount) {
                fclose(file);
                free_students();
                display_error("The course records could not be loaded.");
                return (0);
            }
        }
    }

    fclose(file);

    printf(
        GREEN "%d saved student(s) found.\n" RESET,
        studentCount
    );

    return (studentCount);
}

// finding a student using their ID
int find_student_index(int studentId) {
    int index;

    for (index = 0; index < studentCount; index++) {
        if (students[index].studentId == studentId) {
            return (index);
        }
    }

    return (-1);
}

// registering a new student
void register_student(void) {
    Student newStudent;
    Student *resizedStudents;

    clear_console();

    printf(CYAN"\n\t=== REGISTER A NEW STUDENT ===\n\n"RESET);

    if (!read_int("Enter student ID: ", &newStudent.studentId)) {
        return;
    }

    if (newStudent.studentId <= 0) {
        display_error("Student ID must be greater than zero.");
        wait_for_enter();
        clear_console();
        return;
    }

    // preventing duplicate student IDs
    if (find_student_index(newStudent.studentId) != -1) {
        display_error("A student with this ID already exists.");
        wait_for_enter();
        clear_console();
        return;
    }

    if (!read_text(
        "Enter first name: ",
        newStudent.firstName,
        sizeof(newStudent.firstName))) {
        return;
    }

    if (!read_text(
        "Enter last name: ",
        newStudent.lastName,
        sizeof(newStudent.lastName))) {
        return;
    }

    if (!read_text("Enter programme of study: ",newStudent.programme,sizeof(newStudent.programme))) {
        return;
    }

    if (!read_int("Enter year of study: ",&newStudent.yearOfStudy)) {
        return;
    }

    if (newStudent.yearOfStudy < 1 ||newStudent.yearOfStudy > 6) {
        display_error("Year of study must be between 1 and 6.");
        wait_for_enter();
        clear_console();
        return;
    }

    // starting the student without any registered courses
    newStudent.courseCount = 0;
    newStudent.courses = NULL;

    // increasing the memory used for student records
    resizedStudents = realloc(
        students,
        (studentCount + 1) * sizeof(Student)
    );

    if (resizedStudents == NULL) {
        display_error("Unable to allocate memory for the student.");
        wait_for_enter();
        clear_console();
        return;
    }

    students = resizedStudents;
    students[studentCount] = newStudent;
    studentCount++;

    printf(YELLOW "\nRegistering student..." RESET);
    fflush(stdout);
    sleep(1);

    if (!save_students()) {
        studentCount--;
        display_error("The student could not be saved.");
        wait_for_enter();
        clear_console();
        return;
    }

    display_success("\tStudent registered successfully.");

    printf("\n\t\tStudent ID: %d\n", newStudent.studentId);
    printf("\t\tStudent Name: %s %s\n",newStudent.firstName,newStudent.lastName);
    printf("\t\tProgramme: %s\n", newStudent.programme);
    printf("\t\tYear of Study: %d\n", newStudent.yearOfStudy);
    printf("\t\tRegistered Courses: %d\n", newStudent.courseCount);

    wait_for_enter();
    clear_console();
}

// calculating the student's overall academic average
double calculate_student_average(Student *student) {
    double totalMarks = 0;
    double totalMaximumMarks = 0;
    int index;

    for (index = 0; index < student->courseCount; index++) {
        totalMarks += student->courses[index].mark;
        totalMaximumMarks += student->courses[index].maximumMark;
    }

    if (totalMaximumMarks == 0) {
        return (0);
    }

    return ((totalMarks / totalMaximumMarks) * 100);
}

// displaying student records in a table which I can reuse
void display_student_table(Student studentList[], int count) {
    Student *student;
    char fullName[101];
    double average;
    int index;

    printf("+------+--------------------------+"
        "----------------------+--------+---------+----------+\n");

    printf("| %-4s | %-24s | %-20s | %-6s | %-7s | %-8s |\n",
        "ID","Student Name","Programme","Year","Courses","Average");

    printf("+------+--------------------------+"
        "----------------------+--------+---------+----------+\n");

    for (index = 0; index < count; index++) {
        student = &studentList[index];

        snprintf(fullName,sizeof(fullName),"%s %s",student->firstName,student->lastName);

        if (student->courseCount == 0) {
            printf("| %-4d | %-24.24s | %-20.20s | %6d | %7d | %-8s |\n",
                student->studentId,fullName,
                student->programme,
                student->yearOfStudy,
                student->courseCount,"N/A");
        } else {
            average = calculate_student_average(student);

            printf("| %-4d | %-24.24s | %-20.20s | %6d | %7d | %7.2f%% |\n",
                student->studentId,fullName,
                student->programme,
                student->yearOfStudy,
                student->courseCount,average);
        }
    }

    printf("+------+--------------------------+"
        "----------------------+--------+---------+----------+\n");
}

// displaying a student's course records in a table
void display_course_table(Student *student) {
    Course *course;
    double percentage;
    int index;

    printf("\n+--------------+--------------------------+"
        "----------+----------+------------+--------+\n");

    printf("| %-12s | %-24s | %-8s | %-8s | %-10s | %-6s |\n",
        "Code","Course Name","Mark","Maximum","Percentage","Result");

    printf("+--------------+--------------------------+"
        "----------+----------+------------+--------+\n");

    for (index = 0; index < student->courseCount; index++) {
        course = &student->courses[index];

        if (course->maximumMark == 0) {
            percentage = 0;
        } else {
            percentage = (course->mark / course->maximumMark) * 100;
        }

        printf("| %-12.12s | %-24.24s | %8.2f | %8.2f | %9.2f%% | %-6s |\n",
            course->courseCode,
            course->courseName,
            course->mark,
            course->maximumMark,percentage,
            percentage >= 50 ? "Pass" : "Fail");
    }

    printf("+--------------+--------------------------+"
        "----------+----------+------------+--------+\n");
}

// displaying all registered students
void display_students(void) {
    clear_console();

    printf(CYAN"\n\t=== ALL REGISTERED STUDENTS ===\n\n"RESET);

    if (studentCount == 0) {
        display_warning("No students are currently registered.");
        wait_for_enter();
        clear_console();
        return;
    }

    display_student_table(students, studentCount);

    printf(GREEN "\n\tTotal Registered Students: %d\n" RESET,studentCount);

    wait_for_enter();
    clear_console();
}

// viewing the complete details of one student
void view_student_details(void) {
    Student *student;
    double average;
    int studentId;
    int studentIndex;

    clear_console();

    printf(CYAN"\n\t=== VIEW STUDENT DETAILS ===\n\n"RESET);

    if (studentCount == 0) {
        display_warning("No students are currently registered.");
        wait_for_enter();
        clear_console();
        return;
    }

    if (!read_int("Enter student ID to view: ", &studentId)) {
        return;
    }

    studentIndex = find_student_index(studentId);

    if (studentIndex == -1) {
        display_error("No student was found with this ID.");
        wait_for_enter();
        clear_console();
        return;
    }

    student = &students[studentIndex];

    printf("\n\tStudent ID: %d\n", student->studentId);
    printf("\tStudent Name: %s %s\n",student->firstName,student->lastName);
    printf("\tProgramme: %s\n", student->programme);
    printf("\tYear of Study: %d\n", student->yearOfStudy);
    printf("\tRegistered Courses: %d\n", student->courseCount);

    if (student->courseCount == 0) {
        display_warning("This student has no registered courses.");
        wait_for_enter();
        clear_console();
        return;
    }

    display_course_table(student);

    average = calculate_student_average(student);

    printf(GREEN "\n\tOverall Academic Average: %.2f%%\n" RESET,average);

    wait_for_enter();
    clear_console();
}

// updating a student's personal information
void update_student(void) {
    char extra;
    Student originalStudent;
    Student *student;
    char input[150];
    int studentId, studentIndex, newYear;

    clear_console();

    printf(CYAN"\n\t=== UPDATE STUDENT INFORMATION ===\n\n"RESET);

    if (studentCount == 0) {
        display_warning("No students are currently registered.");
        wait_for_enter();
        clear_console();
        return;
    }

    if (!read_int("Enter student ID: ", &studentId)) {
        return;
    }

    studentIndex = find_student_index(studentId);

    if (studentIndex == -1) {
        display_error("No student was found with this ID.");
        wait_for_enter();
        clear_console();
        return;
    }

    student = &students[studentIndex];
    originalStudent = *student;

    display_student_table(student, 1);

    printf("\nPress Enter without typing anything ""to keep the current value.\n\n");

    printf("First Name [%s]: ", student->firstName);
    fgets(input, sizeof(input), stdin);
    input[strcspn(input, "\n")] = '\0';

    if (strlen(input) > 0) {
        if (strlen(input) >= sizeof(student->firstName)) {
            display_error("First name is too long.");
            return;
        }
        strcpy(student->firstName, input);
    }

    printf("Last Name [%s]: ", student->lastName);
    fgets(input, sizeof(input), stdin);
    input[strcspn(input, "\n")] = '\0';

    if (strlen(input) > 0) {
        if (strlen(input) >= sizeof(student->lastName)) {
            *student = originalStudent;
            display_error("Last name is too long.");
            return;
        }

        strcpy(student->lastName, input);
    }

    printf("Programme [%s]: ", student->programme);
    fgets(input, sizeof(input), stdin);
    input[strcspn(input, "\n")] = '\0';

    if (strlen(input) > 0) {
        if (strlen(input) >= sizeof(student->programme)) {
            *student = originalStudent;
            display_error("Programme name is too long.");
            return;
        }

        strcpy(student->programme, input);
    }

    printf("Year of Study [%d]: ", student->yearOfStudy);
    fgets(input, sizeof(input), stdin);
    input[strcspn(input, "\n")] = '\0';

    if (strlen(input) > 0) {
        if (sscanf(input, "%d %c", &newYear, &extra) != 1 ||newYear < 1 ||newYear > 6) {
            *student = originalStudent;
            display_error("Enter a year between 1 and 6.");
            wait_for_enter();
            clear_console();
            return;
        }
        student->yearOfStudy = newYear;
    }

    if (!save_students()) {
        *student = originalStudent;
        display_error("The updated student could not be saved.");
        wait_for_enter();
        clear_console();
        return;
    }

    display_success("Student information updated successfully.");

    wait_for_enter();
    clear_console();
}

// deleting a student record
void delete_student(void) {
    Student *resizedStudents;
    char confirmation[10];
    int studentId;
    int studentIndex;
    int index;

    clear_console();

    printf(CYAN"\n\t=== DELETE STUDENT ===\n\n"RESET);

    if (studentCount == 0) {
        display_warning("No students are currently registered.");
        wait_for_enter();
        clear_console();
        return;
    }

    if (!read_int("Enter student ID: ", &studentId)) {
        return;
    }

    studentIndex = find_student_index(studentId);

    if (studentIndex == -1) {
        display_error("No student was found with this ID.");
        wait_for_enter();
        clear_console();
        return;
    }

    printf("\nStudent Selected\n\n");
    display_student_table(&students[studentIndex], 1);

    printf("\nDelete this student? Enter yes or no: ");
    fgets(confirmation, sizeof(confirmation), stdin);
    confirmation[strcspn(confirmation, "\n")] = '\0';

    if (tolower(confirmation[0]) != 'y') {
        display_warning("Student deletion cancelled.");
        wait_for_enter();
        clear_console();
        return;
    }

    // releasing the student's courses
    free(students[studentIndex].courses);

    // moving the remaining students forward
    for (index = studentIndex; index < studentCount - 1; index++) {
        students[index] = students[index + 1];
    }

    studentCount--;

    if (studentCount == 0) {
        free(students);
        students = NULL;
    } else {
        resizedStudents = realloc(students,studentCount * sizeof(Student));

        if (resizedStudents != NULL) {
            students = resizedStudents;
        }
    }

    if (!save_students()) {
        display_warning("The student was deleted, but the file could not be updated.");
        wait_for_enter();
        clear_console();
        return;
    }

    display_success("Student deleted successfully.");

    wait_for_enter();
    clear_console();
}

// finding a course in a student's records
int find_course_index(Student *student, char courseCode[]) {
    int index;

    for (index = 0; index < student->courseCount; index++) {
        if (strcmp(student->courses[index].courseCode, courseCode) == 0) {
            return (index);
        }
    }

    return (-1);
}

// displaying the available courses
void display_course_catalogue(void) {
    int index;

    printf("\nAvailable Courses\n\n");

    for (index = 0; index < courseCatalogueCount; index++) {
        printf("%d. %s - %s\n",index + 1,courseCatalogue[index].courseCode, courseCatalogue[index].courseName);
    }
}

// adding a course to a student
void add_course(void) {
    Student *student;
    CourseOption *selectedCourse;
    Course *resizedCourses;
    Course newCourse;
    int studentId, studentIndex, choice;

    clear_console();
    printf(CYAN "\n\t=== ADD COURSE TO STUDENT ===\n" RESET);

    if (studentCount == 0) {
        display_warning("No students are currently registered.");
        wait_for_enter();
        return;
    }

    if (!read_int("\n\tEnter student ID: ", &studentId)) {
        return;
    }

    studentIndex = find_student_index(studentId);

    if (studentIndex == -1) {
        display_error("No student was found with this ID.");
        wait_for_enter();
        return;
    }

    student = &students[studentIndex];

    printf("\nStudent: %s %s\n",
        student->firstName, student->lastName);

    display_course_catalogue();

    if (!read_int("\n\t Select a course: ", &choice)) {
        return;
    }

    if (choice < 1 || choice > courseCatalogueCount) {
        display_error("Select a valid course.");
        wait_for_enter();
        return;
    }

    selectedCourse = &courseCatalogue[choice - 1];

    if (find_course_index(student, selectedCourse->courseCode) != -1) {
        display_error("This student already has this course.");
        wait_for_enter();
        return;
    }

    strcpy(newCourse.courseCode, selectedCourse->courseCode);
    strcpy(newCourse.courseName, selectedCourse->courseName);
    newCourse.maximumMark = selectedCourse->maximumMark;

    if (!read_double("\tEnter mark obtained: ", &newCourse.mark)) {
        return;
    }

    if (newCourse.mark < 0 || newCourse.mark > newCourse.maximumMark) {
        display_error("Mark must be between 0 and 100.");
        wait_for_enter();
        return;
    }

    resizedCourses = realloc(student->courses,(student->courseCount + 1) * sizeof(Course));

    if (resizedCourses == NULL) {
        display_error("Unable to allocate memory for the course.");
        wait_for_enter();
        return;
    }

    student->courses = resizedCourses;
    student->courses[student->courseCount] = newCourse;
    student->courseCount++;

    if (!save_students()) {
        student->courseCount--;
        display_error("The course could not be saved.");
        wait_for_enter();
        return;
    }

    display_success("Course added to the student successfully.");

    printf("\n%s - %s\n", newCourse.courseCode, newCourse.courseName);
    printf("Mark: %.2f / %.2f\n", newCourse.mark, newCourse.maximumMark);

    wait_for_enter();
    clear_console();
}

// updating a student's course mark
void update_course_mark(void) {
    Student *student;
    Course *course;
    double oldMark, newMark;
    int studentId, studentIndex, choice, index;

    clear_console();
    printf(CYAN "\n\t=== UPDATE COURSE MARK ===\n" RESET);

    if (studentCount == 0) {
        display_warning("No students are currently registered.");
        wait_for_enter();
        return;
    }

    if (!read_int("\n\tEnter student ID: ", &studentId)) {
        return;
    }

    studentIndex = find_student_index(studentId);

    if (studentIndex == -1) {
        display_error("No student was found with this ID.");
        wait_for_enter();
        return;
    }

    student = &students[studentIndex];

    if (student->courseCount == 0) {
        display_warning("This student has no registered courses.");
        wait_for_enter();
        return;
    }

    printf("\nStudent: %s %s\n\n", student->firstName, student->lastName);

    for (index = 0; index < student->courseCount; index++) {
        printf("%d. %s - %s (%.2f / %.2f)\n", index + 1,
            student->courses[index].courseCode,
            student->courses[index].courseName,
            student->courses[index].mark,
            student->courses[index].maximumMark);
    }

    if (!read_int("\n\tSelect a course: ", &choice)) {
        return;
    }

    if (choice < 1 || choice > student->courseCount) {
        display_error("Select a valid course.");
        wait_for_enter();
        return;
    }

    course = &student->courses[choice - 1];
    oldMark = course->mark;

    if (!read_double("\tEnter new mark: ", &newMark)) {
        return;
    }

    if (newMark < 0 || newMark > course->maximumMark) {
        display_error("Mark must be between 0 and 100.");
        wait_for_enter();
        return;
    }

    course->mark = newMark;

    if (!save_students()) {
        course->mark = oldMark;
        display_error("The updated mark could not be saved.");
        wait_for_enter();
        return;
    }

    display_success("Course mark updated successfully.");

    printf("\n%s: %.2f / %.2f\n", course->courseName, course->mark, course->maximumMark);

    wait_for_enter();
    clear_console();
}

// displaying a student's academic performance
void display_student_performance(void) {
    Student *student;
    double average;
    int studentId;
    int studentIndex;

    clear_console();
    printf(CYAN "\n\t=== STUDENT ACADEMIC PERFORMANCE ===\n" RESET);

    if (studentCount == 0) {
        display_warning("No students are currently registered.");
        wait_for_enter();
        return;
    }

    if (!read_int("\n\tEnter student ID: ", &studentId)) {
        return;
    }

    studentIndex = find_student_index(studentId);

    if (studentIndex == -1) {
        display_error("No student was found with this ID.");
        wait_for_enter();
        return;
    }

    student = &students[studentIndex];

    printf("\n\tStudent: %s %s\n", student->firstName, student->lastName);
    printf("\tProgramme: %s\n", student->programme);

    if (student->courseCount == 0) {
        display_warning("This student has no registered courses.");
        wait_for_enter();
        return;
    }

    display_course_table(student);
    average = calculate_student_average(student);

    printf("\n\tOverall Average: %.2f%%\n", average);
    printf("\tAcademic Status: %s\n", average >= 50 ? GREEN"Passing"RESET : RED"Failing"RESET);

    wait_for_enter();
    clear_console();
}

// checking whether text contains a search value
bool contains_text(const char text[], const char search[]) {
    char textCopy[200];
    char searchCopy[100];
    int index;

    strcpy(textCopy, text);
    strcpy(searchCopy, search);

    for (index = 0; textCopy[index] != '\0'; index++) {
        textCopy[index] = tolower((unsigned char)textCopy[index]);
    }

    for (index = 0; searchCopy[index] != '\0'; index++) {
        searchCopy[index] = tolower((unsigned char)searchCopy[index]);
    }

    return (strstr(textCopy, searchCopy) != NULL);
}

// searching across student records
void search_students(void) {
    Student *matches;
    char searchValue[100], studentId[20];
    int matchCount = 0, index;

    clear_console();
    printf(CYAN "\n\t=== SEARCH STUDENTS ===\n" RESET);

    if (studentCount == 0) {
        display_warning("No students are currently registered.");
        wait_for_enter();
        return;
    }

    if (!read_text("\n\tEnter a search value: ", searchValue,
        sizeof(searchValue))) {
        return;
        }

    matches = malloc(studentCount * sizeof(Student));

    if (matches == NULL) {
        display_error("Unable to allocate memory for search results.");
        wait_for_enter();
        return;
    }

    for (index = 0; index < studentCount; index++) {
        snprintf(studentId, sizeof(studentId), "%d",students[index].studentId);

        if (contains_text(studentId, searchValue) ||
            contains_text(students[index].firstName, searchValue) ||
            contains_text(students[index].lastName, searchValue) ||
            contains_text(students[index].programme, searchValue)) {
            matches[matchCount] = students[index];
            matchCount++;
            }
    }

    if (matchCount == 0) {
        display_warning("No matching students were found.");
    } else {
        printf(GREEN "\n%d matching student(s) found.\n\n" RESET,
            matchCount);
        display_student_table(matches, matchCount);
    }

    free(matches);
    wait_for_enter();
    clear_console();
}

// sorting and displaying student records
void sort_students(void) {
    Student *sortedStudents, temporaryStudent;
    bool shouldSwap;
    int choice, first, second;

    clear_console();
    printf(CYAN "\n\t=== SORT STUDENTS ===\n" RESET);

    if (studentCount == 0) {
        display_warning("No students are currently registered.");
        wait_for_enter();
        return;
    }

    printf("\n1. Student ID");
    printf("\n2. Student Name (Alphabetic order)");
    printf("\n3. Academic Average");
    printf("\n4. Programme");
    printf("\n5. Year of Study\n");

    if (!read_int("\n\tSelect how to sort: ", &choice)) {
        return;
    }

    if (choice < 1 || choice > 5) {
        display_error("Select an option between 1 and 5.");
        wait_for_enter();
        return;
    }

    sortedStudents = malloc(studentCount * sizeof(Student));

    if (sortedStudents == NULL) {
        display_error("Unable to allocate memory for sorting.");
        wait_for_enter();
        return;
    }

    for (first = 0; first < studentCount; first++) {
        sortedStudents[first] = students[first];
    }

    // manually sorting using bubble sort
    for (first = 0; first < studentCount - 1; first++) {
        for (second = 0; second < studentCount - first - 1; second++) {
            shouldSwap = false;

            if (choice == 1 &&sortedStudents[second].studentId >sortedStudents[second + 1].studentId) {
                shouldSwap = true;
            }

            if (choice == 2 &&
                strcmp(sortedStudents[second].firstName,sortedStudents[second + 1].firstName) > 0) {
                shouldSwap = true;
            }

            if (choice == 3 &&
                calculate_student_average(&sortedStudents[second]) <
                calculate_student_average(&sortedStudents[second + 1])) {
                shouldSwap = true;
            }

            if (choice == 4 &&
                strcmp(sortedStudents[second].programme,sortedStudents[second + 1].programme) > 0) {
                shouldSwap = true;
            }

            if (choice == 5 &&sortedStudents[second].yearOfStudy >sortedStudents[second + 1].yearOfStudy) {
                shouldSwap = true;
            }

            if (shouldSwap) {
                temporaryStudent = sortedStudents[second];
                sortedStudents[second] = sortedStudents[second + 1];
                sortedStudents[second + 1] = temporaryStudent;
            }
        }
    }

    printf("\n");
    display_student_table(sortedStudents, studentCount);

    free(sortedStudents);
    wait_for_enter();
    clear_console();
}

// generating the complete academic report
void generate_academic_report(void) {
    Student *student;
    Student *highestStudent = NULL;
    Student *lowestStudent = NULL;
    double totalMarks = 0, totalMaximumMarks = 0, highestAverage = 0, lowestAverage = 0, average, courseMarks, courseMaximum;
    int totalCourses = 0, passingStudents = 0, failingStudents = 0, studentsWithoutCourses = 0, courseStudents;
    int programmeCount, studentIndex, courseIndex, catalogueIndex, checkIndex;
    bool alreadyCounted;

    clear_console();
    printf(CYAN "\n\t\t=== ACADEMIC PERFORMANCE REPORT ===\n" RESET);

    if (studentCount == 0) {
        display_warning("No students are currently registered.");
        wait_for_enter();
        return;
    }

    for (studentIndex = 0; studentIndex < studentCount; studentIndex++) {
        student = &students[studentIndex];
        totalCourses += student->courseCount;

        if (student->courseCount == 0) {
            studentsWithoutCourses++;
            continue;
        }

        average = calculate_student_average(student);

        if (average >= 50) {
            passingStudents++;
        } else {
            failingStudents++;
        }

        if (highestStudent == NULL || average > highestAverage) {
            highestStudent = student;
            highestAverage = average;
        }

        if (lowestStudent == NULL || average < lowestAverage) {
            lowestStudent = student;
            lowestAverage = average;
        }

        for (courseIndex = 0;courseIndex < student->courseCount;courseIndex++) {
            totalMarks += student->courses[courseIndex].mark;
            totalMaximumMarks += student->courses[courseIndex].maximumMark;
        }
    }

    printf("\n\t\tTotal Registered Students: %d\n", studentCount);
    printf("\t\tTotal Registered Courses: %d\n", totalCourses);

    if (totalMaximumMarks > 0) {
        printf("\t\tOverall Academic Average: %.2f%%\n",
            (totalMarks / totalMaximumMarks) * 100);
    } else {
        printf("\t\tOverall Academic Average: N/A\n");
    }

    printf("\t\tStudents Passing: %d\n", passingStudents);
    printf("\t\tStudents Failing: %d\n", failingStudents);
    printf("\t\tStudents Without Courses: %d\n", studentsWithoutCourses);

    if (highestStudent != NULL) {
        printf(GREEN"\n\t\tHighest Performing Student: %s %s (%.2f%%)\n" RESET,highestStudent->firstName,
            highestStudent->lastName,
            highestAverage);

        printf(YELLOW"\t\tLowest Performing Student: %s %s (%.2f%%)\n" RESET,
            lowestStudent->firstName,
            lowestStudent->lastName,
            lowestAverage);
    }

    printf(CYAN "\n\t\tAverage Performance Per Course\n" RESET);

    printf("\t+------------+--------------------------------+----------+----------+\n");
    printf("\t| %-10s | %-30s | %-8s | %-8s |\n","Code", "Course Name", "Students", "Average");
    printf("\t+------------+--------------------------------+----------+----------+\n");

    for (catalogueIndex = 0; catalogueIndex < courseCatalogueCount; catalogueIndex++) {
        courseMarks = 0;
        courseMaximum = 0;
        courseStudents = 0;

        for (studentIndex = 0; studentIndex < studentCount; studentIndex++) {
            student = &students[studentIndex];

            for (courseIndex = 0; courseIndex < student->courseCount; courseIndex++) {
                if (strcmp(student->courses[courseIndex].courseCode,
                    courseCatalogue[catalogueIndex].courseCode) == 0) {
                    courseMarks += student->courses[courseIndex].mark;
                    courseMaximum += student->courses[courseIndex].maximumMark;
                    courseStudents++;
                    }
            }
        }

        if (courseStudents > 0) {
            printf("\t| %-10s | %-30.30s | %8d | %7.2f%% |\n",
                courseCatalogue[catalogueIndex].courseCode,
                courseCatalogue[catalogueIndex].courseName,
                courseStudents,
                (courseMarks / courseMaximum) * 100);
        }
    }

    printf("\t+------------+--------------------------------+----------+----------+\n");

    printf(CYAN "\n\t\tStudents Per Programme\n" RESET);
    printf("\t+--------------------------------+----------+\n");
    printf("\t| %-30s | %-8s |\n", "Programme", "Students");
    printf("\t+--------------------------------+----------+\n");

    for (studentIndex = 0;studentIndex < studentCount;studentIndex++) {
        alreadyCounted = false;

        for (checkIndex = 0;checkIndex < studentIndex;checkIndex++) {
            if (strcmp(students[studentIndex].programme,
                students[checkIndex].programme) == 0) {
                alreadyCounted = true;
                break;
            }
        }

        if (alreadyCounted) {
            continue;
        }
        programmeCount = 0;

        for (checkIndex = 0;checkIndex < studentCount;checkIndex++) {
            if (strcmp(students[studentIndex].programme,students[checkIndex].programme) == 0) {
                programmeCount++;
            }
        }

        printf("\t| %-30.30s | %8d |\n",students[studentIndex].programme,programmeCount);    }
    printf("\t+--------------------------------+----------+\n");

    wait_for_enter();
    clear_console();
}

void display_menu(void) {
    int choice;

    // storing the menu operations using function pointers
    void (*menu_operations[])(void) = {
        register_student,
        display_students,
        view_student_details,
        update_student,
        delete_student,
        add_course,
        update_course_mark,
        display_student_performance,
        search_students,
        sort_students,
        generate_academic_report
    };

    while (true) {
        printf("\n\t===Helen School Academic Analytics===");

        printf("\n\t1. Register a New Student");
        printf("\n\t2. Display All Registered Students");
        printf("\n\t3. View a Student Details");
        printf("\n\t4. Update Student Information");
        printf("\n\t5. Delete a Student");
        printf("\n\t6. Add Course to Student");
        printf("\n\t7. Update Course Mark");
        printf("\n\t8. Display Student Academic Performance");
        printf("\n\t9. Search Students");
        printf("\n\t10. Sort Students");
        printf("\n\t11. Generate Academic Report");

        printf("\n\n\t0. Exit");

        if (!read_int(CYAN"\n\n\tSelect an action: "RESET, &choice)) {
            break;
        }

        // checking if the selected menu option exists
        if (choice < 0 || choice > 11) {
            display_error("Select an option between 0 and 11.");
            usleep(500000);
            continue;
        }

        // ending the program when zero is selected
        if (choice == 0) {
            printf("\nClosing the academic analytics application.\n");
            sleep(1);
            break;
        }

        // calling the selected function through its pointer
        menu_operations[choice - 1]();
    }
}

int main(void) {
    load_students();
    display_menu();
    free_students();

    return 0;
}