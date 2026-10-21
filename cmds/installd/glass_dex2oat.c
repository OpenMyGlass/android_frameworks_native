/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * OpenMyGlass: dex2oat compiler filter selection for installd. See glass_dex2oat.h.
 */
#include "glass_dex2oat.h"

#include <ctype.h>
#include <string.h>

static const char *const kFilters[] = {
    "interpret-only", "space", "balanced", "speed", "everything",
};

int glass_d2o_valid_filter(const char *name)
{
    size_t i;
    if (name == NULL) {
        return 0;
    }
    for (i = 0; i < sizeof(kFilters) / sizeof(kFilters[0]); i++) {
        if (strcmp(name, kFilters[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int starts_with(const char *s, const char *prefix)
{
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/* Copies [start, end) into out, dropping a trailing "-<digits>". */
static int copy_key(const char *start, const char *end, char *out, size_t outlen)
{
    const char *p = end;
    while (p > start && isdigit((unsigned char) p[-1])) {
        p--;
    }
    if (p < end && p > start + 1 && p[-1] == '-') {
        end = p - 1;
    }
    if (end <= start || (size_t) (end - start) >= outlen) {
        return -1;
    }
    memcpy(out, start, end - start);
    out[end - start] = '\0';
    return 0;
}

int glass_d2o_package_key(const char *apk_path, char *out, size_t outlen)
{
    const char *base;
    const char *end;

    if (apk_path == NULL || out == NULL || outlen == 0) {
        return -1;
    }
    if (starts_with(apk_path, "/mnt/asec/")) {
        /* /mnt/asec/<pkg>-<n>/pkg.apk: the container directory names the package. */
        const char *dir = apk_path + strlen("/mnt/asec/");
        const char *slash = strchr(dir, '/');
        if (slash != NULL) {
            return copy_key(dir, slash, out, outlen);
        }
    }
    base = strrchr(apk_path, '/');
    base = base ? base + 1 : apk_path;
    end = base + strlen(base);
    if (end - base > 4 && strcmp(end - 4, ".apk") == 0) {
        end -= 4;
    } else if (end - base > 4 && strcmp(end - 4, ".jar") == 0) {
        end -= 4;
    }
    if (starts_with(apk_path, "/data/app/") || starts_with(apk_path, "/data/app-private/")) {
        return copy_key(base, end, out, outlen);
    }
    if (end <= base || (size_t) (end - base) >= outlen) {
        return -1;
    }
    memcpy(out, base, end - base);
    out[end - base] = '\0';
    return 0;
}

/* Splits a line into its first two whitespace-separated words, in place. */
static int split_line(char *line, char **key, char **value)
{
    char *p = line;
    while (*p && isspace((unsigned char) *p)) p++;
    if (*p == '\0' || *p == '#') {
        return 0;
    }
    *key = p;
    while (*p && !isspace((unsigned char) *p)) p++;
    if (*p == '\0') {
        return 0;
    }
    *p++ = '\0';
    while (*p && isspace((unsigned char) *p)) p++;
    *value = p;
    while (*p && !isspace((unsigned char) *p)) p++;
    *p = '\0';
    return **value != '\0';
}

int glass_d2o_pick_filter(const char *apk_path, const char *sys_value, const char *usr_value,
                          FILE *overrides, char *out)
{
    const char *chosen = NULL;
    char key[256];
    char line[512];
    int have_key;

    if (apk_path == NULL || out == NULL) {
        return 0;
    }
    if (starts_with(apk_path, "/system/") || starts_with(apk_path, "/vendor/")) {
        chosen = sys_value;
    } else {
        chosen = usr_value;
    }
    if (!glass_d2o_valid_filter(chosen)) {
        chosen = NULL;
    }
    if (chosen != NULL) {
        strcpy(out, chosen);
    }

    have_key = glass_d2o_package_key(apk_path, key, sizeof(key)) == 0;
    if (overrides != NULL) {
        while (fgets(line, sizeof(line), overrides) != NULL) {
            char *k, *v;
            if (strchr(line, '\n') == NULL && !feof(overrides)) {
                /* Overlong line: skip the rest of it and ignore it. */
                int c;
                while ((c = fgetc(overrides)) != EOF && c != '\n') {}
                continue;
            }
            if (!split_line(line, &k, &v) || !glass_d2o_valid_filter(v)) {
                continue;
            }
            if (strcmp(k, apk_path) == 0 || (have_key && strcmp(k, key) == 0)) {
                strcpy(out, v);
                chosen = out;
            }
        }
    }
    return chosen != NULL;
}
