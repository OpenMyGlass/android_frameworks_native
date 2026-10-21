/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * OpenMyGlass: dex2oat compiler filter selection for installd (Android 4.4, ART only).
 *
 * 4.4 has no dalvik.vm.dex2oat-filter property, so installd picks the filter itself:
 *   - persist.glass.d2o.sys for APKs and jars under /system/ or /vendor/ (system code),
 *   - persist.glass.d2o.usr for every other APK,
 *   - a per-package line in /data/system/glass-dex2oat-filters overrides both.
 * The property names fit 4.4's 31-character limit (PROP_NAME_MAX 32).
 *
 * Limits of 4.4's dex2oat and runtime:
 *   - dex2oat compiles a dex file of 900 methods or fewer with "speed" whatever the filter,
 *     unless the filter is "interpret-only" (dex2oat.cc, kDefaultNumDexMethodsThreshold), so
 *     "space" and "balanced" only change larger apps.
 *   - These settings apply to the compiles installd runs. The boot image, and compiles the
 *     runtime starts itself (class_linker.cc, e.g. after /data/dalvik-cache is wiped or for a
 *     DexClassLoader), use the runtime's own filter.
 *   - Override keys are package names for /data/app and /mnt/asec APKs, but file names for
 *     system APKs ("Settings" for /system/app/Settings.apk), since installd sees only the path.
 *
 * This file has no Android dependency so that it builds and is tested on a host.
 */
#ifndef GLASS_DEX2OAT_H
#define GLASS_DEX2OAT_H

#include <stddef.h>
#include <stdio.h>

#define GLASS_D2O_SYS_PROPERTY "persist.glass.d2o.sys"
#define GLASS_D2O_USR_PROPERTY "persist.glass.d2o.usr"
#define GLASS_D2O_OVERRIDES "/data/system/glass-dex2oat-filters"

/* Longest valid filter name ("interpret-only") plus NUL. */
#define GLASS_D2O_FILTER_MAX 16

/* Returns 1 if name is a compiler filter that 4.4's runtime.cc accepts. */
int glass_d2o_valid_filter(const char *name);

/*
 * Writes into out (size outlen) the key that override lines are matched against:
 *   /data/app/<pkg>-<n>.apk, /data/app-private/<pkg>-<n>.apk -> <pkg>
 *   /mnt/asec/<pkg>-<n>/pkg.apk                              -> <pkg>
 *   any other path, e.g. /system/app/Name.apk               -> Name
 * Returns 0 on success, -1 if the key does not fit or the path has no file name.
 */
int glass_d2o_package_key(const char *apk_path, char *out, size_t outlen);

/*
 * Picks the filter for apk_path. sys_value and usr_value are the property values ("" when
 * unset); overrides is the open override file or NULL. Override lines are
 * "<package or full apk path> <filter>"; blank lines and lines starting with '#' are skipped,
 * and the last matching line wins. An invalid filter anywhere is ignored.
 * Writes the filter into out (GLASS_D2O_FILTER_MAX bytes) and returns 1, or returns 0 when no
 * valid filter applies, in which case installd passes none and dex2oat uses its default.
 */
int glass_d2o_pick_filter(const char *apk_path, const char *sys_value, const char *usr_value,
                          FILE *overrides, char *out);

#endif
