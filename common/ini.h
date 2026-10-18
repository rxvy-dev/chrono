/* Tiny INI reader shared by the compositor, the shell and the settings app.
 *   [section]
 *   key = value        # full-line comments start with # or ;
 * Keys are looked up as "section.key". */
#ifndef CHRONO_INI_H
#define CHRONO_INI_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct chrono_ini_entry {
	char *key;    /* "section.key", lower case */
	char *value;
};

struct chrono_ini {
	struct chrono_ini_entry *entries;
	size_t count, cap;
};

bool chrono_ini_load(struct chrono_ini *ini, const char *path);
bool chrono_ini_parse(struct chrono_ini *ini, const char *text);
void chrono_ini_free(struct chrono_ini *ini);

const char *chrono_ini_get(const struct chrono_ini *ini, const char *key, const char *def);
double chrono_ini_num(const struct chrono_ini *ini, const char *key, double def);
bool chrono_ini_bool(const struct chrono_ini *ini, const char *key, bool def);

/* Rewrite one key in an INI file in place (keeps comments and order); adds the section/key if missing. */
bool chrono_ini_set_in_file(const char *path, const char *section, const char *key, const char *value);

#ifdef __cplusplus
}
#endif

#endif
