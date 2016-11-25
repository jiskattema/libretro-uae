#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "sysdeps.h"
#include "uae_types.h"
#include "options.h"
#include "crc32.h"
#include "rommgr.h"
#include <dirent.h>

#include "libretro-glue.h"

#define MYBUFSIZ (524288 * 2)

void check_roms(char *path)
{
    DIR* FD;
    struct dirent* in_file;
    FILE    *entry_file;
    char     fullname[256];
    char    buffer[MYBUFSIZ];
    size_t  size;
    uae_u32 crc;
    struct romdata *rd;

    FD = opendir (path);
    if(! FD) 
    {
        fprintf(stderr, "Cannot read kickstart dir '%s', check RETRO_SYSTEM_DIR\n", path);
    }

    while ((in_file = readdir(FD))) 
    {
        /* On linux/Unix we don't want current and parent directories
         * If you're on Windows machine remove this two lines
         */
        if (!strcmp (in_file->d_name, "."))
            continue;
        if (!strcmp (in_file->d_name, ".."))
            continue;

        /* Open directory entry file for common operation */
        strcpy(fullname, path);
        strcat(fullname, "/");
        strcat(fullname, in_file->d_name);
        entry_file = fopen(fullname, "r");
        if (entry_file == NULL)
        {
            fprintf(stderr, "Error : Failed to open entry file %s\n", fullname);
        }
        else
        {
            fseek(entry_file, 0, SEEK_END);
            size = ftell(entry_file);
            if (size > MYBUFSIZ) {
                fprintf(stderr, "Error: file too big, skipping: %s\n", in_file->d_name);
                fclose(entry_file);
                continue;
            }

            fseek(entry_file, 0, SEEK_SET);
            fread(buffer, 1, size, entry_file);
            crc = get_crc32(buffer, size);

            rd = getromdatabycrc(crc);
            if(rd)
            { 
                // fprintf(stderr, "[OK] Identified rom   '%s' as '%s'\n", in_file->d_name, rd->name);
                romlist_add (fullname, rd);
            }
            else
            {
                // fprintf(stderr, "[..] Unidentified rom '%s'\n", in_file->d_name);
            }

            /* When you finish with the file, close it */
            fclose(entry_file);
        }
    }
}
