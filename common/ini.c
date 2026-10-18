#define _GNU_SOURCE
#include "ini.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char *trim(char *s)
{
	while (*s && isspace((unsigned char)*s))
		s++;
	char *e = s + strlen(s);
	while (e > s && isspace((unsigned char)e[-1]))
		*--e = '\0';
	return s;
}

static void lower(char *s)
{
	for (; *s; s++)
		*s = (char)tolower((unsigned char)*s);
}

static void put(struct chrono_ini *ini, const char *key, const char *value)
{
	for (size_t i = 0; i < ini->count; i++) {
		if (strcmp(ini->entries[i].key, key) == 0) {
			free(ini->entries[i].value);
			ini->entries[i].value = strdup(value);
			return;
		}
	}
	if (ini->count == ini->cap) {
		ini->cap = ini->cap ? ini->cap * 2 : 32;
		ini->entries = realloc(ini->entries, ini->cap * sizeof(*ini->entries));
	}
	ini->entries[ini->count].key = strdup(key);
	ini->entries[ini->count].value = strdup(value);
	ini->count++;
}

bool chrono_ini_parse(struct chrono_ini *ini, const char *text)
{
	char section[128] = "";
	char *copy = strdup(text ? text : "");
	char *save = NULL;
	for (char *line = strtok_r(copy, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
		char *l = trim(line);
		if (!*l || *l == '#' || *l == ';')
			continue;
		size_t n = strlen(l);
		if (*l == '[' && l[n - 1] == ']') {
			l[n - 1] = '\0';
			snprintf(section, sizeof section, "%s", trim(l + 1));
			lower(section);
			continue;
		}
		char *eq = strchr(l, '=');
		if (!eq)
			continue;
		*eq = '\0';
		char *k = trim(l), *v = trim(eq + 1);
		size_t vn = strlen(v);
		if (vn >= 2 && v[0] == '"' && v[vn - 1] == '"') {
			v[vn - 1] = '\0';
			v++;
		}
		char key[256];
		snprintf(key, sizeof key, "%s.%s", section, k);
		lower(key);
		put(ini, key, v);
	}
	free(copy);
	return true;
}

bool chrono_ini_load(struct chrono_ini *ini, const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return false;
	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (len < 0 || len > 4 * 1024 * 1024) {
		fclose(f);
		return false;
	}
	char *buf = calloc(1, (size_t)len + 1);
	size_t got = fread(buf, 1, (size_t)len, f);
	buf[got] = '\0';
	fclose(f);
	chrono_ini_parse(ini, buf);
	free(buf);
	return true;
}

void chrono_ini_free(struct chrono_ini *ini)
{
	for (size_t i = 0; i < ini->count; i++) {
		free(ini->entries[i].key);
		free(ini->entries[i].value);
	}
	free(ini->entries);
	ini->entries = NULL;
	ini->count = ini->cap = 0;
}

const char *chrono_ini_get(const struct chrono_ini *ini, const char *key, const char *def)
{
	char k[256];
	snprintf(k, sizeof k, "%s", key);
	lower(k);
	for (size_t i = 0; i < ini->count; i++)
		if (strcmp(ini->entries[i].key, k) == 0)
			return ini->entries[i].value;
	return def;
}

double chrono_ini_num(const struct chrono_ini *ini, const char *key, double def)
{
	const char *v = chrono_ini_get(ini, key, NULL);
	if (!v || !*v)
		return def;
	char *end;
	double d = strtod(v, &end);
	return end == v ? def : d;
}

bool chrono_ini_bool(const struct chrono_ini *ini, const char *key, bool def)
{
	const char *v = chrono_ini_get(ini, key, NULL);
	if (!v)
		return def;
	if (!strcasecmp(v, "true") || !strcasecmp(v, "yes") || !strcasecmp(v, "on") || !strcmp(v, "1"))
		return true;
	if (!strcasecmp(v, "false") || !strcasecmp(v, "no") || !strcasecmp(v, "off") || !strcmp(v, "0"))
		return false;
	return def;
}

static void mkdirs_for(const char *path)
{
	char tmp[4096];
	snprintf(tmp, sizeof tmp, "%s", path);
	for (char *p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			mkdir(tmp, 0755);
			*p = '/';
		}
	}
}

bool chrono_ini_set_in_file(const char *path, const char *section, const char *key, const char *value)
{
	/* read all lines */
	char *text = NULL;
	size_t tlen = 0;
	FILE *f = fopen(path, "r");
	if (f) {
		fseek(f, 0, SEEK_END);
		long n = ftell(f);
		fseek(f, 0, SEEK_SET);
		if (n > 0) {
			text = calloc(1, (size_t)n + 1);
			tlen = fread(text, 1, (size_t)n, f);
			text[tlen] = '\0';
		}
		fclose(f);
	}
	size_t outcap = tlen + strlen(section) + strlen(key) + strlen(value) + 64;
	char *out = calloc(1, outcap);
	size_t olen = 0;
#define EMIT(...) do { int _w = snprintf(out + olen, outcap - olen, __VA_ARGS__); if (_w > 0) olen += (size_t)_w; } while (0)

	bool in_sec = false, done = false, sec_seen = false;
	char *save = NULL;
	char *copy = text ? strdup(text) : strdup("");
	/* keep empty lines: split manually */
	char *p = copy;
	while (p && *p) {
		char *nl = strchr(p, '\n');
		if (nl)
			*nl = '\0';
		char line[4096];
		snprintf(line, sizeof line, "%s", p);
		char *l = trim(line);
		if (*l == '[') {
			if (in_sec && !done) {
				EMIT("%s = %s\n", key, value);
				done = true;
			}
			char name[128];
			snprintf(name, sizeof name, "%s", l + 1);
			char *rb = strchr(name, ']');
			if (rb)
				*rb = '\0';
			in_sec = strcasecmp(trim(name), section) == 0;
			if (in_sec)
				sec_seen = true;
			EMIT("%s\n", p);
		} else if (in_sec && !done && *l != '#' && *l != ';' && strchr(l, '=')) {
			char k[256];
			snprintf(k, sizeof k, "%s", l);
			*strchr(k, '=') = '\0';
			if (strcasecmp(trim(k), key) == 0) {
				EMIT("%s = %s\n", key, value);
				done = true;
			} else {
				EMIT("%s\n", p);
			}
		} else {
			EMIT("%s\n", p);
		}
		p = nl ? nl + 1 : NULL;
	}
	(void)save;
	if (!done) {
		if (!sec_seen)
			EMIT("%s[%s]\n", olen && out[olen - 1] != '\n' ? "\n" : (olen ? "\n" : ""), section);
		else if (!in_sec) {
			/* section exists earlier but we left it: append a duplicate section header (still valid INI) */
			EMIT("\n[%s]\n", section);
		}
		EMIT("%s = %s\n", key, value);
	}
#undef EMIT
	free(copy);
	free(text);

	mkdirs_for(path);
	char tmp[4200];
	snprintf(tmp, sizeof tmp, "%s.tmp", path);
	f = fopen(tmp, "w");
	if (!f) {
		free(out);
		return false;
	}
	fwrite(out, 1, olen, f);
	fclose(f);
	free(out);
	return rename(tmp, path) == 0;
}
