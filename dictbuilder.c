#include "./hashmap.c/hashmap.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// macro

#define MEMORY_MSG "Stop being poor! Buy more memory lol\n"
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

// map contents

typedef struct {
  char *word;
  long count;
} Bigram;

typedef struct {
  char *word;
  long count;
  struct hashmap *bigrams;
} Unigram;

// map methods

int unigram_compare(const void *a, const void *b, void *udata) {
  const Unigram *ua = a;
  const Unigram *ub = b;
  return strcmp(ua->word, ub->word);
}

uint64_t unigram_hash(const void *item, uint64_t seed0, uint64_t seed1) {
  const Unigram *unigram = item;
  return hashmap_sip(unigram->word, strlen(unigram->word), seed0, seed1);
}

void unigram_free(void *item) {
  const Unigram *unigram = item;
  free(unigram->word);
  hashmap_free(unigram->bigrams);
}

int bigram_compare(const void *a, const void *b, void *udata) {
  const Bigram *ua = a;
  const Bigram *ub = b;
  return strcmp(ua->word, ub->word);
}

uint64_t bigram_hash(const void *item, uint64_t seed0, uint64_t seed1) {
  const Bigram *digram = item;
  return hashmap_sip(digram->word, strlen(digram->word), seed0, seed1);
}

void bigram_free(void *item) {
  const Bigram *digram = item;
  free(digram->word);
}

#define oom_handler(ptr)                                                       \
  do {                                                                         \
    if (!ptr) {                                                                \
      fprintf(stderr, MEMORY_MSG);                                             \
      exit(errno);                                                             \
    }                                                                          \
  } while (0)

#define hashmap_oom_handler(map)                                               \
  do {                                                                         \
    if (hashmap_oom(map))                                                      \
      oom_handler(NULL);                                                       \
  } while (0)

#define set_empty_bigram(map, word_to_copy)                                    \
  do {                                                                         \
    char *cpy = strdup(word_to_copy);                                          \
    oom_handler(cpy);                                                          \
                                                                               \
    hashmap_set(map, &(Bigram){.word = cpy, .count = 1});                      \
    hashmap_oom_handler(map);                                                  \
  } while (0)

#define set_empty_unigram(map, word_to_copy)                                   \
  do {                                                                         \
    char *cpy = strdup(word_to_copy);                                          \
    oom_handler(cpy);                                                          \
                                                                               \
    void *bigrams = hashmap_new(sizeof(Bigram), 25, 0, 0, bigram_hash,         \
                                bigram_compare, bigram_free, NULL);            \
    oom_handler(bigrams);                                                      \
                                                                               \
    hashmap_set(map, &(Unigram){.word = cpy, .count = 1, .bigrams = bigrams}); \
    hashmap_oom_handler(map);                                                  \
  } while (0)

static int cmp_unigram(const void *p1, const void *p2) {
  Unigram *const *ua = p1;
  Unigram *const *ub = p2;
  return (*ub)->count - (*ua)->count;
}

static int cmp_bigram(const void *p1, const void *p2) {
  Bigram *const *ua = p1;
  Bigram *const *ub = p2;
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
  *max_occurancies = 1;
  size_t initial_size = hashmap_count(map);

  // max possible size of a word is 49
  char *last = malloc(sizeof(char) * 49);
  char *cur = malloc(sizeof(char) * 49);

  oom_handler(last);
  oom_handler(cur);

  // if no words in file, skip
  if (fscanf(fp, " %48s", last) <= 0)
    goto cleanup;

  strclean((unsigned char *)last);
  // set first word
  Unigram *last_unigram = (Unigram *)hashmap_get(map, &(Unigram){.word = last});
  if (last_unigram)
    last_unigram->count++;
  else
    set_empty_unigram(map, last);

  while (fscanf(fp, " %48s", cur) > 0) {
    strclean((unsigned char *)cur);
    if (strlen(cur) <= 0)
      continue;

    const Unigram *last_unigram = hashmap_get(map, &(Unigram){.word = last});
    // update bigram's counter
    Bigram *bigram =
        (Bigram *)hashmap_get(last_unigram->bigrams, &(Bigram){.word = cur});

    if (bigram) {
      bigram->count++;
    } else {
      set_empty_bigram(last_unigram->bigrams, cur);
    }
    // update unigram counter of the cur
    Unigram *cur_unigram = (Unigram *)hashmap_get(map, &(Unigram){.word = cur});

    if (cur_unigram) {
      if (cur_unigram->count++ > *max_occurancies) {
        *max_occurancies = cur_unigram->count;
      }
    } else {
      set_empty_unigram(map, cur);
    }

    // swap pointers
    char *tmp = last;
    last = cur;
    cur = tmp;
  }

cleanup:
  free(last);
  free(cur);

  return hashmap_count(map) - initial_size;
}

void write_dictionary(FILE *fp, struct hashmap *map, unsigned min_f,
                      char *locale, size_t max_unigram_occurancies) {
  // write header
  fprintf(fp,
          "dictionary=main:%s,locale=%s,description=nabokov's dictionary,date="
          "1739810145,version=54\n",
          locale, locale);
  if (hashmap_count(map) == 0)
    return;
  // division factor
  double factor = round(max_unigram_occurancies / 240.L);
  // collect and sort map keys
  const Unigram **unigrams =
      malloc(sizeof(Unigram *) * (hashmap_count(map) + 1));
  oom_handler(unigrams);

  size_t iter = 0;
  void *item;
  for (size_t i = 0; hashmap_iter(map, &iter, &item); i++) {
    const Unigram *unigram = item;
    unigrams[i] = unigram;
  }
  unigrams[hashmap_count(map)] = NULL;
  qsort(unigrams, hashmap_count(map), sizeof(Unigram *), cmp_unigram);
  // write unigrams
  const Unigram **unigrams_ptr = unigrams;
  do {
    const Unigram *u = *unigrams;
    unsigned f = MAX(round(u->count / factor), 1);
    if (f < min_f && hashmap_count(u->bigrams) > 1)
      f = MIN(f + min_f, 255);
    fprintf(fp, " word=%s,f=%d,flags=,originalFreq=%d\n", u->word, f, f);
    // skip if no digrams
    if (hashmap_count(u->bigrams) == 0)
      continue;
    // collect and sort bigrams
    const Bigram **bigrams =
        malloc(sizeof(Bigram *) * (hashmap_count(u->bigrams) + 1));

    oom_handler(bigrams);

    size_t iter = 0;
    void *item;
    for (size_t i = 0; hashmap_iter(u->bigrams, &iter, &item); i++) {
      const Bigram *bigram = item;
      bigrams[i] = bigram;
    }
    bigrams[hashmap_count(u->bigrams)] = NULL;
    qsort(bigrams, hashmap_count(u->bigrams), sizeof(Bigram *), cmp_bigram);
    // write bigrams
    size_t c = 0;
    const Bigram **bigrams_ptr = bigrams;
    do {
      const Bigram *b = *bigrams;
      const Unigram *unigram_of_b =
          hashmap_get(map, &(Unigram){.word = b->word});

      unsigned f = MAX(round((b->count + unigram_of_b->count) / factor), 1);
      if (f < min_f)
        f = MIN(f + min_f, 255);
      fprintf(fp, "  bigram=%s,f=%d\n", b->word, f);

    } while (*++bigrams && c++ < 4);

    free(bigrams_ptr);

  } while (*++unigrams);

  free(unigrams_ptr);
}

// util

enum PREV_ARG { FILENAME, MIN_F, LOCALE };

void usage() {
  fprintf(stderr,
          "\nUsage: dictbuilder [Options] <file_1> <file_2> <file_3> ... "
          "<file_n>\n\n"
          "Files must be in English or/and Russian.\n"
          "The util supports only "
          "utf-8 and ascii encodings.\n\n"
          "Options:\n\n"
          "--minf           average minimal f value in dictionary(0-255, "
          "default: 150)\n"
          "--locale         locale to use in the header(default: ru)\n");
  exit(errno);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    usage();
  }

  // create word map with occurancies
  struct hashmap *map = hashmap_new(sizeof(Unigram), 5000, 0, 0, unigram_hash,
                                    unigram_compare, unigram_free, NULL);

  size_t max_occurancies;
  unsigned min_f = 150;
  char *locale = "ru";

  int arg_type = FILENAME;

  while (*++argv) {
    // parse options
    if (arg_type == MIN_F) {
      if (sscanf(*argv, "%u", &min_f) <= 0)
        usage();
      if (min_f > 255)
        usage();

      arg_type = FILENAME;
      continue;
    }
    if (arg_type == LOCALE) {
      locale = *argv;
      arg_type = FILENAME;
      continue;
    }
    if (strcmp(*argv, "--minf") == 0) {
      arg_type = MIN_F;
      continue;
    } else if (strcmp(*argv, "--locale") == 0) {
      arg_type = LOCALE;
      continue;
    } else {
      arg_type = FILENAME;
    }
    // build dict
    FILE *fp = fopen(*argv, "r");

    if (fp == NULL) {
      fprintf(stderr, "No such file '%s'\n", *argv);
      return errno;
    }

    fill_wordmap(fp, map, &max_occurancies);

    fclose(fp);
  }
  // print them to stdout
  write_dictionary(stdout, map, min_f, locale, max_occurancies);
  hashmap_free(map);

  return 0;
}
