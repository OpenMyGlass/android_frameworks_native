/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * OpenMyGlass: host test for glass_dex2oat.c. Build and run:
 *   cc -Wall -Wextra -Werror -I cmds/installd -o /tmp/t \
 *      cmds/installd/glass_dex2oat.c cmds/installd/tests/glass_dex2oat_test.c && /tmp/t
 */
#include "glass_dex2oat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { if (!(cond)) { \
    fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static FILE *file_with(const char *text)
{
    FILE *f = tmpfile();
    if (f == NULL) {
        perror("tmpfile");
        exit(2);
    }
    fputs(text, f);
    rewind(f);
    return f;
}

static void check_key(const char *path, const char *want)
{
    char key[64];
    int r = glass_d2o_package_key(path, key, sizeof(key));
    if (want == NULL) {
        CHECK(r == -1);
    } else {
        CHECK(r == 0);
        if (r == 0 && strcmp(key, want) != 0) {
            fprintf(stderr, "key(%s) = %s, want %s\n", path, key, want);
            failures++;
        }
    }
}

/* Returns the picked filter, or "" when none applies. */
static const char *pick(const char *path, const char *sys, const char *usr, const char *file)
{
    static char out[GLASS_D2O_FILTER_MAX];
    FILE *f = file ? file_with(file) : NULL;
    int r;
    memset(out, 'x', sizeof(out));
    r = glass_d2o_pick_filter(path, sys, usr, f, out);
    if (f) fclose(f);
    if (!r) return "";
    return out;
}

int main(void)
{
    /* Filters accepted by 4.4's art/runtime/runtime.cc. */
    CHECK(glass_d2o_valid_filter("interpret-only"));
    CHECK(glass_d2o_valid_filter("space"));
    CHECK(glass_d2o_valid_filter("balanced"));
    CHECK(glass_d2o_valid_filter("speed"));
    CHECK(glass_d2o_valid_filter("everything"));
    CHECK(!glass_d2o_valid_filter(""));
    CHECK(!glass_d2o_valid_filter(NULL));
    CHECK(!glass_d2o_valid_filter("verify-none"));  /* a 5.0+ filter */
    CHECK(!glass_d2o_valid_filter("speed "));
    CHECK(strlen("interpret-only") + 1 <= GLASS_D2O_FILTER_MAX);

    /* Property names fit 4.4's PROP_NAME_MAX (32, including the NUL). */
    CHECK(strlen(GLASS_D2O_SYS_PROPERTY) <= 31);
    CHECK(strlen(GLASS_D2O_USR_PROPERTY) <= 31);

    check_key("/data/app/com.example.app-1.apk", "com.example.app");
    check_key("/data/app/com.example.app-12.apk", "com.example.app");
    check_key("/data/app/com.example.app.apk", "com.example.app");
    check_key("/data/app-private/com.paid.app-2.apk", "com.paid.app");
    check_key("/mnt/asec/com.example.sd-1/pkg.apk", "com.example.sd");
    check_key("/system/app/Settings.apk", "Settings");
    check_key("/system/framework/com.android.location.provider.jar",
              "com.android.location.provider");
    check_key("/system/app/Foo-2.apk", "Foo-2");      /* only /data/app names carry "-<n>" */
    check_key("/data/app/-1.apk", "-1");              /* nothing left before "-1": keep it */
    check_key("/data/app/.apk", ".apk");
    check_key("/data/app/", NULL);
    {
        char small[4];
        CHECK(glass_d2o_package_key("/data/app/com.example-1.apk", small, sizeof(small)) == -1);
    }

    /* Properties: system code versus everything else. */
    CHECK(strcmp(pick("/system/app/Settings.apk", "speed", "space", NULL), "speed") == 0);
    CHECK(strcmp(pick("/vendor/app/Foo.apk", "speed", "space", NULL), "speed") == 0);
    CHECK(strcmp(pick("/system/framework/x.jar", "balanced", "space", NULL), "balanced") == 0);
    CHECK(strcmp(pick("/data/app/com.a-1.apk", "speed", "space", NULL), "space") == 0);
    CHECK(strcmp(pick("/mnt/asec/com.a-1/pkg.apk", "speed", "space", NULL), "space") == 0);
    /* Unset or invalid property: no filter, dex2oat's default (speed). */
    CHECK(strcmp(pick("/data/app/com.a-1.apk", "speed", "", NULL), "") == 0);
    CHECK(strcmp(pick("/data/app/com.a-1.apk", "speed", "fast", NULL), "") == 0);

    /* Override file: package name or full path, last match wins, junk ignored. */
    CHECK(strcmp(pick("/data/app/com.a-1.apk", "speed", "space",
                      "com.a interpret-only\n"), "interpret-only") == 0);
    CHECK(strcmp(pick("/data/app/com.a-1.apk", "speed", "space",
                      "/data/app/com.a-1.apk speed\n"), "speed") == 0);
    CHECK(strcmp(pick("/data/app/com.a-1.apk", "speed", "space",
                      "# comment\n\n  com.a   speed  trailing\ncom.a\ncom.a bogus\n"
                      "com.b everything\n"), "speed") == 0);
    CHECK(strcmp(pick("/data/app/com.a-1.apk", "speed", "space",
                      "com.a speed\ncom.a interpret-only\n"), "interpret-only") == 0);
    CHECK(strcmp(pick("/data/app/com.ab-1.apk", "speed", "space", "com.a speed\n"),
                 "space") == 0);
    CHECK(strcmp(pick("/system/app/Settings.apk", "speed", "space", "Settings space"),
                 "space") == 0);
    /* An override applies even when the property is unset. */
    CHECK(strcmp(pick("/data/app/com.a-1.apk", "", "", "com.a balanced\n"), "balanced") == 0);
    {
        /* An overlong line is skipped as a whole; the next line still counts. */
        char buf[2048];
        memset(buf, 'a', 1000);
        strcpy(buf + 1000, " speed\ncom.a everything\n");
        CHECK(strcmp(pick("/data/app/com.a-1.apk", "", "space", buf), "everything") == 0);
    }
    CHECK(glass_d2o_pick_filter(NULL, "speed", "speed", NULL, (char[16]){0}) == 0);

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("glass_dex2oat: all checks passed\n");
    return 0;
}
