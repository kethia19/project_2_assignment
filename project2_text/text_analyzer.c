
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define SOURCE_TITLE  "The Brothers Karamazov"
#define SOURCE_AUTHOR "Fyodor Dostoevsky, Book VI, Chapter 3 (trans. Constance Garnett, 1912)"

const char *BUILT_IN_TEXT =
    "Brothers, have no fear of men's sin. Love a man even in his sin, for that is the "
    "semblance of Divine Love and is the highest love on earth. Love all God's creation, "
    "the whole and every grain of sand in it. Love every leaf, every ray of God's light. "
    "Love the animals, love the plants, love everything. If you love everything, you will "
    "perceive the divine mystery in things. Once you perceive it, you will begin to "
    "comprehend it better every day. And you will come at last to love the whole world "
    "with an all-embracing love. Love the animals: God has given them the rudiments of "
    "thought and joy untroubled. Do not trouble it, don't harass them, don't deprive them "
    "of their happiness, don't work against God's intent. Man, do not pride yourself on "
    "superiority to the animals; they are without sin, and you, with your greatness, "
    "defile the earth by your appearance on it, and leave the traces of your foulness "
    "after you -- alas, it is true of almost every one of us! Love children especially, "
    "for they too are sinless like the angels; they live to soften and purify our hearts "
    "and, as it were, to guide us. Woe to him who offends a child!";

struct wordStat {
    char name[40];
    int count;
};

// The text, stored in memory that grows as characters are added. 
char *text = NULL;
int text_length = 0;
int text_size = 0;

// The list of distinct words, also stored in memory that grows. 
struct wordStat *words = NULL;
int distinct_words = 0;
int words_size = 0;

int total_words = 0;
int total_letters = 0;      // used for the average word length 
int cut_words = 0;          // words longer than 39 characters 
int using_built_in = 1;

void free_memory(void){
    free(text);
    free(words);
    text = NULL;
    words = NULL;
    text_length = text_size = 0;
    distinct_words = words_size = 0;
    total_words = total_letters = cut_words = 0;
}

void out_of_memory(void){
    printf("Memory allocation failed. Exiting.\n");
    free_memory();
    exit(1);
}

// Adds one character to the text. Doubles the memory when it is full. 
void add_char(char c){
    if (text_length + 1 >= text_size) {
        char *bigger = realloc(text, text_size * 2);
        if (bigger == NULL)
            out_of_memory();
        text = bigger;
        text_size = text_size * 2;
    }
    text[text_length] = c;
    text_length++;
    text[text_length] = '\0';
}

// Counts one word. A new word is added to the list; a known word gets count + 1. 
void add_word(const char *word){
    struct wordStat *w;

    total_words++;
    total_letters += strlen(word);

    for (w = words; w < words + distinct_words; w++) {
        if (strcmp(w->name, word) == 0) {
            w->count++;
            return;
        }
    }
    if (distinct_words == words_size) {
        struct wordStat *bigger = realloc(words, words_size * 2 * sizeof(struct wordStat));
        if (bigger == NULL)
            out_of_memory();
        words = bigger;
        words_size = words_size * 2;
    }
    strcpy(words[distinct_words].name, word);    // word is at most 39 characters 
    words[distinct_words].count = 1;
    distinct_words++;
}

// Splitting the text into words

int is_letter(char c){
    return isalnum((unsigned char)c);
}

void extract_words(void){
    char word[40];
    char *p = text;
    int len, was_cut;

    while (*p != '\0') {
        // skip spaces and punctuation
        if (!is_letter(*p)) {         
            p++;
            continue;
        }
        len = 0;
        was_cut = 0;
        // keep reading letters, plus ' or - when a letter comes right after 
        while (is_letter(*p) || ((*p == '\'' || *p == '-') && is_letter(*(p + 1)))) {
            if (len < 39) {
                word[len] = tolower((unsigned char)*p);
                len++;
            } else {
                was_cut = 1;
            }
            p++;
        }
        word[len] = '\0';
        if (was_cut)
            cut_words++;
        add_word(word);
    }
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

// Reads pasted text one character at a time until a line that says END. 
void read_pasted_text(void){
    int c, line_start = 0;

    printf("Paste your paragraph, then type END on a new line and press Enter:\n");
    while ((c = getchar()) != EOF) {
        if (c == '\r')
            continue;
        if (c == '\n') {
            if (strcmp(text + line_start, "END") == 0) {
                text[line_start] = '\0';
                text_length = line_start;
                return;
            }
            add_char(' ');
            line_start = text_length;
        } else {
            add_char(c);
        }
    }
}

// Throws away the old text and loads a new one. 
void load_text(void){
    const char *p;
    int choice;

    printf("1) Use the built-in paragraph (%s)\n2) Paste your own paragraph\n", SOURCE_TITLE);
    choice = read_int("Choice: ", 1, 2);

    free_memory();
    text_size = 64;
    words_size = 16;
    text = malloc(text_size);
    words = malloc(words_size * sizeof(struct wordStat));
    if (text == NULL || words == NULL)
        out_of_memory();
    text[0] = '\0';

    using_built_in = (choice == 1);
    if (using_built_in) {
        for (p = BUILT_IN_TEXT; *p != '\0'; p++)
            add_char(*p);
    } else {
        read_pasted_text();
    }
    extract_words();

    printf("\nText loaded: %d characters, %d words, %d distinct words.\n",
           text_length, total_words, distinct_words);
    printf("Memory used: text buffer %d bytes, word list %d slots.\n", text_size, words_size);
    if (total_words == 0)
        printf("Warning: the text has no words.\n");
    else if (total_words < 150)
        printf("Note: the assignment needs at least 150 words; this text has %d.\n", total_words);
    if (cut_words > 0)
        printf("Note: %d word(s) were longer than 39 characters and were cut.\n", cut_words);
}

int no_words(void){
    if (total_words == 0) {
        printf("There are no words to analyse. Load a text first (option 10).\n");
        return 1;
    }
    return 0;
}

struct wordStat *find_longest(void){
    struct wordStat *w, *best = words;
    for (w = words; w < words + distinct_words; w++) {
        if (strlen(w->name) > strlen(best->name))
            best = w;
    }
    return best;
}

struct wordStat *find_most_frequent(void){
    struct wordStat *w, *best = words;
    for (w = words; w < words + distinct_words; w++) {
        if (w->count > best->count)
            best = w;
    }
    return best;
}

void print_richness(void){
    struct wordStat *w;
    int used_once = 0;
    float diversity, unique_ratio, score;

    for (w = words; w < words + distinct_words; w++) {
        if (w->count == 1)
            used_once++;
    }
    diversity = (float)distinct_words / total_words;
    unique_ratio = (float)used_once / distinct_words;
    score = (diversity + unique_ratio) / 2 * 100;

    printf("Lexical diversity = %d / %d = %.3f\n", distinct_words, total_words, diversity);
    printf("Unique-word ratio = %d / %d = %.3f\n", used_once, distinct_words, unique_ratio);
    printf("Richness score    = %.1f out of 100\n", score);
    if (score >= 70)      printf("Meaning: very rich vocabulary, little repetition.\n");
    else if (score >= 50) printf("Meaning: rich vocabulary with some repeated key words.\n");
    else if (score >= 30) printf("Meaning: moderate - many words are repeated.\n");
    else                  printf("Meaning: low - the same few words are used again and again.\n");
}

void count_words(void){
    if (no_words()) return;
    printf("Total words: %d\n", total_words);
}

void longest_word(void){
    struct wordStat *w;
    if (no_words()) return;
    w = find_longest();
    printf("Longest word: %s (%d characters)\n", w->name, (int)strlen(w->name));
}

void most_frequent(void){
    struct wordStat *w;
    if (no_words()) return;
    w = find_most_frequent();
    printf("Most frequent word: \"%s\" (%d times)\n", w->name, w->count);
}

void display_word_statistics(void){
    struct wordStat *w;
    if (no_words()) return;
    printf("%-40s %s\n", "Word", "Count");
    printf("----------------------------------------------\n");
    for (w = words; w < words + distinct_words; w++)
        printf("%-40s %d\n", w->name, w->count);
    printf("%d distinct words\n", distinct_words);
}

// Compare functions for qsort: negative = a goes first, positive = b goes first. 
int compare_alphabetical(const void *a, const void *b){
    const struct wordStat *x = a, *y = b;
    return strcmp(x->name, y->name);
}

int compare_frequency(const void *a, const void *b){
    const struct wordStat *x = a, *y = b;
    if (x->count != y->count)
        return y->count - x->count;
    return strcmp(x->name, y->name);
}

void sort_word_statistics(void){
    // array of function pointers
    int (*compare[])(const void *, const void *) = { compare_alphabetical, compare_frequency };
    int choice;

    if (no_words()) return;
    choice = read_int("Sort by 1) Alphabetical  2) Frequency: ", 1, 2);
    qsort(words, distinct_words, sizeof(struct wordStat), compare[choice - 1]);
    display_word_statistics();
}

void search_word(void){
    char word[100];
    struct wordStat *w;
    int i;

    if (no_words()) return;
    printf("Enter a word: ");
    if (scanf("%99s", word) != 1)
        input_ended();
    clear_line();
    for (i = 0; word[i] != '\0'; i++)
        word[i] = tolower((unsigned char)word[i]);

    for (w = words; w < words + distinct_words; w++) {
        if (strcmp(w->name, word) == 0) {
            printf("\"%s\" appears %d time(s).\n", word, w->count);
            return;
        }
    }
    printf("\"%s\" is not in the text.\n", word);
}

void generate_text_summary(void){
    struct wordStat *longest, *frequent;

    if (no_words()) return;
    longest = find_longest();
    frequent = find_most_frequent();

    printf("========== TEXT SUMMARY ==========\n");
    if (using_built_in)
        printf("Source              : %s, %s\n", SOURCE_TITLE, SOURCE_AUTHOR);
    else
        printf("Source              : text pasted by the user\n");
    printf("Total words         : %d\n", total_words);
    printf("Distinct words      : %d\n", distinct_words);
    printf("Average word length : %.2f characters\n", (float)total_letters / total_words);
    printf("Longest word        : %s (%d characters)\n", longest->name, (int)strlen(longest->name));
    printf("Most frequent word  : %s (%d times)\n", frequent->name, frequent->count);
    printf("--- Vocabulary Richness Score ---\n");
    print_richness();
    printf("==================================\n");
}

void custom_analysis(void){
    if (no_words()) return;
    printf("Vocabulary Richness Score\n");
    printf("Formula: ((distinct / total) + (used once / distinct)) / 2 x 100\n\n");
    print_richness();
}

void memory_inspection(void){
    struct wordStat *selected;

    if (no_words()) return;
    selected = find_most_frequent();
    printf("Memory Inspection\n");
    printf("--------------------------\n");
    // where the paragraph is stored
    printf("Text buffer address      : %p  \n", (void *)text);
    // where the list of words starts
    printf("Word statistics array    : %p  \n", (void *)words);
    printf("Selected word-stat entry : %p \"%s\"\n", (void *)selected, selected->name);
    printf("Each entry takes %d bytes, so entries are %d bytes apart.\n",
           (int)sizeof(struct wordStat), (int)sizeof(struct wordStat));
}



int main(void){
    // Array of function pointers 
    void (*menu[])(void) = {
        count_words, longest_word, most_frequent, display_word_statistics,
        sort_word_statistics, search_word, generate_text_summary,
        custom_analysis, memory_inspection, load_text
    };
    int choice;


    printf("Student ID: Kethia Ngabire Kayigire\n");
    printf("Project: Dynamic Text Mining and Linguistic Pattern Analyzer\n");
    printf("Source: %s\n", SOURCE_TITLE);
    printf("Author: %s\n", SOURCE_AUTHOR);
    printf("Custom Analysis: Vocabulary Richness Score\n\n");

    load_text();

    while (1) {
        printf("\n---------- MENU ----------\n");
        printf(" 1. Count words\n 2. Longest word\n 3. Most frequent word\n");
        printf(" 4. Show all word counts\n 5. Sort words\n 6. Search for a word\n");
        printf(" 7. Text summary\n 8. Custom analysis (Richness Score)\n");
        printf(" 9. Memory addresses\n10. Load a new text\n11. Exit\n");
        choice = read_int("Choose an option: ", 1, 11);
        if (choice == 11)
            break;
        printf("\n");
        menu[choice - 1]();
    }

    free_memory();
    printf("Memory freed. Goodbye!\n");
    return 0;
}
