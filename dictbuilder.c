#include "./hashmap.c/hashmap.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// macro

#define MEMORY_MSG "Stop being poor! Buy more memory lol\n"

// map contents

typedef struct {
  char *word;
  long count;
} Next;

typedef struct {
  char *word;
  long count;
  struct hashmap *next;
} Word;

// map methods

int word_compare(const void *a, const void *b, void *udata) {
  const Word *ua = a;
  const Word *ub = b;
  return strcmp(ua->word, ub->word);
}

uint64_t word_hash(const void *item, uint64_t seed0, uint64_t seed1) {
  const Word *word = item;
  return hashmap_sip(word->word, strlen(word->word), seed0, seed1);
}

void word_free(void *item) {
  const Word *word = item;
  free(word->word);
  hashmap_free(word->next);
}

int next_compare(const void *a, const void *b, void *udata) {
  const Next *ua = a;
  const Next *ub = b;
  return strcmp(ua->word, ub->word);
}

uint64_t next_hash(const void *item, uint64_t seed0, uint64_t seed1) {
  const Next *word = item;
  return hashmap_sip(word->word, strlen(word->word), seed0, seed1);
}

void next_free(void *item) {
  const Word *next = item;
  free(next->word);
}

#define set_empty_word(map, cap, word_to_set)                                  \
  hashmap_set(map,                                                             \
              &(Word){.word = strdup(word_to_set),                             \
                      .count = 1,                                              \
                      .next = hashmap_new(sizeof(Next), 50, 0, 0, next_hash,   \
                                          next_compare, next_free, NULL)})

static int cmp_next(const void *p1, const void *p2) {
  Next *const *ua = p1;
  Next *const *ub = p2;
  return (*ub)->count - (*ua)->count;
}

static int cmp_word(const void *p1, const void *p2) {
  Word *const *ua = p1;
  Word *const *ub = p2;
  return (*ub)->count - (*ua)->count;
}

// str helpers

/// Takes utf8 string that containts Russian and English characters, removes
/// none-alphabetic ones except numbers and '-', converts it to lower case
void strclean(unsigned char *src) {
  unsigned char *dst = src;

  while (*src) {
    unsigned char next = *(src + 1);

    if ((*src >= 0x30 && *src <= 0x39) || (*src >= 0x40 && *src <= 0x5a) ||
        (*src >= 0x61 && *src <= 0x7a) || *src == 0x2d) {
      // make lower case
      if (*src < 0x61 && *src != 0x2d && !(*src >= 0x30 && *src <= 0x39))
        *src += 32;

      *dst++ = *src;
    }

    if ((*src == 0xd0 && (next >= 0x90 && next <= 0xbf)) ||
        (*src == 0xd1 && (next >= 0x80 && next <= 0x8f))) {
      // make lower case
      if (*src == 0xd0 && next <= 0x9f)
        next += 32;
      if (*src == 0xd0 && next > 0x9f && next <= 0xaf) {
        *src = 0xd1;
        next -= 32;
      }

      *dst++ = *src;
      *dst++ = next;
      src++;
    }

    src++;
  }

  *dst = '\0';
}

// dict functions

size_t fill_wordmap(FILE *fp, struct hashmap *map, size_t *max_occurancies) {
  *max_occurancies = 0;
  size_t initial_size = hashmap_count(map);

  char *last_word = malloc(sizeof(char) * 100);
  char *cur_word = malloc(sizeof(char) * 100);

  if (!last_word || !cur_word) {
    fprintf(stderr, MEMORY_MSG);
    exit(errno);
  }

  // if no words in file, skip
  if (fscanf(fp, " %99s", last_word) <= 0)
    return 0;

  strclean((unsigned char *)last_word);
  // set first word
  set_empty_word(map, 25, last_word);

  while (fscanf(fp, " %99s", cur_word) > 0) {
    strclean((unsigned char *)cur_word);
    if (strlen(cur_word) <= 0)
      continue;

    Word *word = (Word *)hashmap_get(map, &(Word){.word = last_word});
    Next *next = (Next *)hashmap_get(word->next, &(Next){.word = cur_word});

    if (next) {
      if (next->count++ > *max_occurancies)
        *max_occurancies = next->count;
    } else {
      hashmap_set(word->next, &(Next){.word = strdup(cur_word), .count = 1});
    }

    Word *next_word = (Word *)hashmap_get(map, &(Word){.word = cur_word});
    if (next_word) {
      if (next_word->count++ > *max_occurancies)
        *max_occurancies = next_word->count;
    } else {
      set_empty_word(map, 25, cur_word);
    }

    char *tmp = last_word;
    last_word = cur_word;
    cur_word = tmp;
  }

  free(last_word);
  free(cur_word);

  return hashmap_count(map) - initial_size;
}

void write_dictionary(FILE *fp, struct hashmap *map, size_t max_occurancies) {
  // write header
  fprintf(fp,
          "dictionary=main:ru,locale=ru,description=набоковский словарь,date="
          "1739810145,version=54\n");
  // division factor
  double factor = floor(max_occurancies / 240.L);
  // collect the map keys
  size_t len = hashmap_count(map) + 1;
  const Word **wordlist = malloc(sizeof(Word *) * len);
  const Word **wordlist_ptr = wordlist;

  if (!wordlist) {
    fprintf(stderr, MEMORY_MSG);
    exit(errno);
  }

  wordlist[len - 1] = NULL;

  size_t iter = 0;
  size_t i = 0;
  void *item;

  while (hashmap_iter(map, &iter, &item)) {
    wordlist[i++] = (const Word *)item;
  }
  // sort by occurancies
  qsort(wordlist, len - 1, sizeof(Word *), cmp_word);
  // convert maps to word sequnces
  while (*++wordlist) {
    // collect words
    size_t max_occurancies = 0;
    const Word *word = *wordlist;
    size_t len = hashmap_count(word->next) + 1;
    const Next **top_words = malloc(sizeof(Next *) * len);
    const Next **top_words_ptr = top_words;

    if (!top_words) {
      fprintf(stderr, MEMORY_MSG);
      exit(errno);
    }

    top_words[len - 1] = NULL;

    size_t iter = 0;
    size_t i = 0;
    void *item;

    while (hashmap_iter(word->next, &iter, &item)) {
      const Next *next = item;

      if (next->count > max_occurancies)
        max_occurancies = next->count;

      top_words[i] = next;
      i++;
    }
    // sort words by count
    if (len > 1) {
      qsort(top_words, len - 1, sizeof(Next *), cmp_next);
    }

    unsigned f = round(word->count / factor) + 1;
    fprintf(fp, " word=%s,f=%d,flags=,originalFreq=%d\n", word->word, f, f);

    double factor = max_occurancies > 255 ? max_occurancies / 255L : 1L;
    unsigned next_f = floor((*top_words)->count / factor) + f;
    int c = 0;
    while (*++top_words && c < 10) {
      fprintf(fp, "  bigram=%s,f=%d\n", (*top_words)->word,
              next_f > 255 ? 255 : next_f);
      c++;
    }

    free(top_words_ptr);
  }
  free(wordlist_ptr);
}

// program

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr,
            "Usage: %s <file_1> <file_2> <file_3> ... <file_n>\n"
            "Files must be utf8 encoded\n",
            *argv);
    return errno;
  }

  struct hashmap *map = hashmap_new(sizeof(Word), 5000, 0, 0, word_hash,
                                    word_compare, word_free, NULL);
  size_t max_occurancies;

  // create word map with occurancies
  while (*++argv) {
    FILE *fp = fopen(*argv, "r");

    if (fp == NULL) {
      fprintf(stderr, "No such file %s\n", *argv);
      return errno;
    }

    fill_wordmap(fp, map, &max_occurancies);

    fclose(fp);
  }
  // print them to stdout
  write_dictionary(stdout, map, max_occurancies);
  hashmap_free(map);

  return 0;
}
