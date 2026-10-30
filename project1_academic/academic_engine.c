#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STUDENT_ID "Kethia Ngabire Kayigire"
#define PASS_MARK  50
#define SAFE_GRADE 70

typedef struct {
    int id;
    char name[50];
    char course[40];
    float grade;
} Student;

// The list of students lives in memory borrowed with malloc/realloc
Student *students = NULL;
int count = 0;

void free_memory(void){
    free(students);
    students = NULL;
    count = 0;
}

void input_ended(void){
    printf("\nInput ended.\n");
    free_memory();
    exit(0);
}

//  Throws away whatever is left on the current input line.
void clear_line(void){
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { }
}

// Asks until the user types a whole number between min and max
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

// Asks until the user types a grade between 0 and 100.
float read_grade(void){
    float value;
    int result, next;

    while (1) {
        printf("Enter grade (0-100): ");
        result = scanf("%f", &value);
        if (result == EOF)
            input_ended();
        next = getchar();
        if (next != '\n' && next != EOF)
            clear_line();
        if (result == 1 && (next == '\n' || next == EOF) && value >= 0 && value <= 100)
            return value;
        printf("  Invalid grade. It must be a number from 0 to 100.\n");
    }
}

//  Returns a pointer to the student with this ID, or NULL if there is none
Student *find_student(int id){
    Student *p;
    for (p = students; p < students + count; p++) {
        if (p->id == id)
            return p;
    }
    return NULL;
}

float average_grade(void){
    float total = 0;
    Student *p;
    for (p = students; p < students + count; p++)
        total += p->grade;
    return total / count;
}

float risk_index(float grade){
    if (grade >= SAFE_GRADE)
        return 0;
    return (SAFE_GRADE - grade) / SAFE_GRADE * 100;
}

const char *risk_level(float grade){
    if (grade >= SAFE_GRADE) return "Safe";
    if (grade >= PASS_MARK)  return "Watch";
    if (grade >= 30)         return "At Risk";
    return "Critical";
}

void print_header(void){
    printf("%-8s %-22s %-18s %6s\n", "ID", "Name", "Course", "Grade");
    printf("--------------------------------------------------------\n");
}

void print_student(Student *s){
    printf("%-8d %-22s %-18s %6.2f\n", s->id, s->name, s->course, s->grade);
}

int list_is_empty(void){
    if (count == 0) {
        printf("No student records.\n");
        return 1;
    }
    return 0;
}

// ---------- Menu options ---------- 

void add_student(void){
    Student s;
    Student *bigger;

    s.id = read_int("Enter student ID (1-99999): ", 1, 99999);
    if (find_student(s.id) != NULL) {
        printf("Error: ID %d already exists. Student not added.\n", s.id);
        return;
    }

    printf("Enter full name: ");
    if (scanf(" %49[^\n]", s.name) != 1)
        input_ended();
    clear_line();
    printf("Enter course: ");
    if (scanf(" %39[^\n]", s.course) != 1)
        input_ended();
    clear_line();
    s.grade = read_grade();

    // make the list one student bigger 
    bigger = realloc(students, (count + 1) * sizeof(Student));
    if (bigger == NULL) {
        printf("Memory allocation failed. Student not added.\n");
        return;
    }
    students = bigger;
    students[count] = s;
    count++;
    printf("Student added. The list now holds %d students.\n", count);
}

void display_students(void){
    Student *p;

    if (list_is_empty())
        return;
    print_header();
    for (p = students; p < students + count; p++)
        print_student(p);
    printf("Total: %d students\n", count);
}

// Compare functions for qsort: negative = a goes first, positive = b goes first. 
int compare_grade(const void *a, const void *b){
    const Student *x = a, *y = b;
    if (x->grade < y->grade) return 1;          // highest grade first 
    if (x->grade > y->grade) return -1;
    return 0;
}

int compare_name(const void *a, const void *b){
    const Student *x = a, *y = b;
    return strcmp(x->name, y->name);           
}
// smallest ID first 
int compare_id(const void *a, const void *b){
    const Student *x = a, *y = b;
    return x->id - y->id;                       
}

void sort_students(void){
    // array of function pointers: one compare function per sort choice 
    int (*compare[])(const void *, const void *) = { compare_grade, compare_name, compare_id };
    int choice;

    if (list_is_empty())
        return;
    choice = read_int("Sort by 1) Grade  2) Name  3) ID: ", 1, 3);
    qsort(students, count, sizeof(Student), compare[choice - 1]);
    display_students();
}

void calculate_average(void){
    if (list_is_empty())
        return;
    printf("Average grade: %.2f\n", average_grade());
}

void find_top_student(void){
    Student *p, *top;

    if (list_is_empty())
        return;
    top = students;
    for (p = students; p < students + count; p++) {
        if (p->grade > top->grade)
            top = p;
    }
    printf("Top student:\n");
    print_header();
    print_student(top);
}

void search_student(void){
    char name[50];
    Student *p;
    int choice, found = 0;

    if (list_is_empty())
        return;
    choice = read_int("Search by 1) ID  2) Name: ", 1, 2);

    if (choice == 1) {
        p = find_student(read_int("Enter student ID: ", 1, 99999));
        if (p == NULL) {
            printf("No student with that ID.\n");
        } else {
            print_header();
            print_student(p);
        }
        return;
    }

    printf("Enter full name: ");
    if (scanf(" %49[^\n]", name) != 1)
        input_ended();
    clear_line();
    for (p = students; p < students + count; p++) {
        if (strcmp(p->name, name) == 0) {     // strcmp returns 0 when the names match 
            if (!found)
                print_header();
            print_student(p);
            found = 1;
        }
    }
    if (!found)
        printf("No student named \"%s\".\n", name);
}

void remove_student(void){
    Student *p, *smaller;
    int id, i, position;

    if (list_is_empty())
        return;
    id = read_int("Enter ID of student to remove: ", 1, 99999);
    p = find_student(id);
    if (p == NULL) {
        printf("No student with ID %d.\n", id);
        return;
    }

    // move every student after this one a place to the left 
    position = p - students;
    for (i = position; i < count - 1; i++)
        students[i] = students[i + 1];
    count--;

    // make the list one student smaller 
    if (count == 0) {
        free(students);
        students = NULL;
    } else {
        smaller = realloc(students, count * sizeof(Student));
        if (smaller != NULL)
            students = smaller;
    }
    printf("Student %d removed. The list now holds %d students.\n", id, count);
}

// Prints how many students are at each risk level and the class average ARI. 
void print_risk_totals(void){
    int safe = 0, watch = 0, at_risk = 0, critical = 0;
    float total_risk = 0;
    Student *p;

    for (p = students; p < students + count; p++) {
        total_risk += risk_index(p->grade);
        if (p->grade >= SAFE_GRADE)     safe++;
        else if (p->grade >= PASS_MARK) watch++;
        else if (p->grade >= 30)        at_risk++;
        else                            critical++;
    }
    printf("Safe: %d | Watch: %d | At Risk: %d | Critical: %d\n", safe, watch, at_risk, critical);
    printf("Class Risk Index (average ARI): %.2f\n", total_risk / count);
    printf("Students who need help (At Risk + Critical): %d of %d\n", at_risk + critical, count);
}

void custom_analysis(void){
    Student *p;

    if (list_is_empty())
        return;
    printf("Academic Risk Index: ARI = (70 - grade) / 70 x 100  (0 if grade >= 70)\n");
    printf("0 means no risk. The higher the number, the more help the student needs.\n\n");
    printf("%-8s %-22s %6s %7s  %s\n", "ID", "Name", "Grade", "ARI", "Level");
    printf("--------------------------------------------------------\n");
    for (p = students; p < students + count; p++)
        printf("%-8d %-22s %6.2f %7.2f  %s\n",
               p->id, p->name, p->grade, risk_index(p->grade), risk_level(p->grade));
    printf("\n");
    print_risk_totals();
}

void summary_report(void){
    Student *p;
    float highest, lowest;
    int passed = 0;

    if (list_is_empty())
        return;
    highest = students[0].grade;
    lowest = students[0].grade;
    for (p = students; p < students + count; p++) {
        if (p->grade > highest) highest = p->grade;
        if (p->grade < lowest)  lowest = p->grade;
        if (p->grade >= PASS_MARK) passed++;
    }
    printf("========== SUMMARY REPORT ==========\n");
    printf("Total students      : %d\n", count);
    printf("Average grade       : %.2f\n", average_grade());
    printf("Highest grade       : %.2f\n", highest);
    printf("Lowest grade        : %.2f\n", lowest);
    printf("Passed (>= %d)      : %d\n", PASS_MARK, passed);
    printf("Below %d            : %d\n", PASS_MARK, count - passed);
    printf("--- Academic Risk Index ---\n");
    print_risk_totals();
    printf("====================================\n");
}

// Puts the first 12 students into memory
int load_students(void){
    Student start[] = {
        {1001, "Alice Uwase",        "Math in Machine Learning", 88.5},
        {1002, "Brian Mugisha",      "Programming in C", 45.0},
        {1003, "Chloe Ingabire",     "Mobile Dev",   72.0},
        {1004, "David Habimana",     "Linear Algebra",   28.5},
        {1005, "Esther Mukamana",    "Web Development",  91.0},
        {1006, "Frank Niyonzima",    "Web Development",  63.5},
        {1007, "Grace Umutoni",      "Programming in C", 55.0},
        {1008, "Henry Nkurunziza",   "Software Engineering",39.0},
        {1009, "Irene Uwimana",      "Web Development",  77.5},
        {1010, "Jean Bosco Kamanzi", "Programming in C", 66.0},
        {1011, "Kevin Ishimwe",      "Linear Algebra",   81.0},
        {1012, "Linda Keza",         "Web Development",  49.5},
    };
    int i, n = sizeof(start) / sizeof(start[0]);

    students = malloc(n * sizeof(Student));
    if (students == NULL)
        return 0;
    for (i = 0; i < n; i++)
        students[i] = start[i];
    count = n;
    return 1;
}

int main(void){
    int choice;
    // Array of function pointers
    void (*menu[])(void) = {
        add_student, display_students, sort_students, calculate_average,
        find_top_student, search_student, remove_student, custom_analysis,
        summary_report
    };
    

    if (!load_students()) {
        printf("Memory allocation failed.\n");
        return 1;
    }

    printf("Student ID: %s\n", STUDENT_ID);
    printf("Project: Dynamic Academic Performance and Risk Analysis Engine\n");
    printf("Custom Analysis: Academic Risk Index\n");

    while (1) {
        printf("\n---------- MENU ----------\n");
        printf(" 1. Add student\n 2. Display students\n 3. Sort students\n");
        printf(" 4. Average grade\n 5. Top student\n 6. Search student\n");
        printf(" 7. Remove student\n 8. Custom analysis (Risk Index)\n");
        printf(" 9. Summary report\n10. Exit\n");
        choice = read_int("Choose an option: ", 1, 10);
        if (choice == 10)
            break;
        printf("\n");
        menu[choice - 1]();        // calls the chosen function through its pointer 
    }

    free_memory();
    printf("Memory freed. Goodbye!\n");
    return 0;
}
