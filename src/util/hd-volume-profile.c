/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2009 Nokia Corporation.
 *
 * Author: Kimmo Hamalainen <kimmo.hamalainen@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <profiled/libprofile.h>
#include <glib.h>
#include "hd-volume-profile.h"

#define SILENT_PROFILE "silent"
#define SYSTEM_SOUNDS_KEY "system.sound.level"

static gboolean silenced;
static int silent_profile = -1;
static int system_sounds = -1;

gboolean hd_volume_profile_is_silent(void)
{
        if (silent_profile < 0) {
                char *prof = profile_get_profile();
                if (silenced || (prof && strcmp(SILENT_PROFILE, prof)) == 0)
                        silent_profile = 1;
                else
                        silent_profile = 0;
        }
        if (system_sounds < 0) {
                char *val = profile_get_value(NULL, SYSTEM_SOUNDS_KEY);
                if (val)
                        system_sounds = atoi(val);
        }

        if (silenced || silent_profile || system_sounds == 0)
                return TRUE;
        else
                return FALSE;
}

void hd_volume_profile_set_silent(gboolean setting)
{
        silenced = setting;
}

static void track_active(const char *profile, const char *key,
                         const char *val, const char *type,
                         void *unused)
{
        if (key && strcmp(key, SYSTEM_SOUNDS_KEY) == 0) {
                if (val)
                        system_sounds = atoi(val);
        }
}

static void track_profile(const char *profile, void *unused)
{
        if (silenced || (profile && strcmp(SILENT_PROFILE, profile) == 0))
                silent_profile = 1;
        else
                silent_profile = 0;
}

void hd_volume_profile_init(void)
{
        if (g_file_test("/scratchbox", G_FILE_TEST_EXISTS)) {
                silent_profile = 1;
                system_sounds = 0;
        } else {
                profile_track_add_profile_cb(track_profile, NULL, NULL);
                profile_track_add_active_cb(track_active, NULL, NULL);
                profile_tracker_init();
        }
}

