/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

#include "g_common.h"

/* Limit for the number of names */
#define NAMES_MAX 128

/* Type for a name */
typedef struct name {
        char s[16];
        int count;
} name_t;

/* Structure for a name database */
typedef struct {
        name_t names[NAMES_MAX];
        int size;
        const char *keyword;
} name_list_t;

static name_list_t lists[G_NAME_TYPES];

/******************************************************************************\
 Callback for parsing a name pair.
\******************************************************************************/
static int name_pair(const char *key, const char *value)
{
        int i;

        for (i = 0; i < G_NAME_TYPES; i++)
                if (!strcasecmp(key, lists[i].keyword)) {
                        name_t *name;

                        if (lists[i].size >= NAMES_MAX) {
                                C_warning("Name list '%s' full", key);
                                return FALSE;
                        }
                        name = lists[i].names + lists[i].size++;
                        name->count = 0;
                        C_strncpy_buf(name->s, value);
                        return TRUE;
                }
        C_warning("Name list '%s' not found", key);
        return TRUE;
}

/******************************************************************************\
 Load name config.
\******************************************************************************/
void G_load_names(void)
{
        c_token_file_t tf;

        /* Keywords */
        lists[G_NT_SHIP].keyword = "ship";
        lists[G_NT_ISLAND].keyword = "island";

        /* Parse names config */
        C_token_file_init(&tf, "configs/names.cfg");
        C_token_file_parse_pairs(&tf, (c_key_value_f)name_pair);
        C_token_file_cleanup(&tf);
}

/******************************************************************************\
 Generate a new name.
\******************************************************************************/
void G_get_name(g_name_type_t type, char *buf, int size)
{
        int i, start, low;

        C_assert(type >= 0 && type < G_NAME_TYPES);
        if (lists[type].size < 1)
                return;
        start = rand() % lists[type].size;

        /* Find the least-used name */
        for (i = (low = start) + 1; i < lists[type].size; i++)
                if (lists[type].names[i].count < lists[type].names[low].count)
                        low = i;
        for (i = 0; i < start; i++)
                if (lists[type].names[i].count < lists[type].names[low].count)
                        low = i;
        C_strncpy(buf, lists[type].names[low].s, size);

        /* Append a number if its been used already */
        if (lists[type].names[low].count++ > 0)
                C_suffix(buf, C_va(" %d", lists[type].names[low].count), size);
}

/******************************************************************************\
 Count a name if it was used by someone else.
\******************************************************************************/
void G_count_name(g_name_type_t type, const char *name)
{
        int i;

        C_assert(type >= 0 && type < G_NAME_TYPES);
        for (i = 0; i < lists[type].size; i++)
                if (!strcasecmp(lists[type].names[i].s, name)) {
                        lists[type].names[i].count++;
                        return;
                }
}

/******************************************************************************\
 Reset name counts.
\******************************************************************************/
void G_reset_name_counts(void)
{
        int i, j;

        for (i = 0; i < G_NAME_TYPES; i++)
                for (j = 0; j < lists[i].size; j++)
                        lists[i].names[j].count = 0;
}

