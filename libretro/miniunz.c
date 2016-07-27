/*
   miniunz.c
   Version 1.1, February 14h, 2010
   sample part of the MiniZip project - ( http://www.winimage.com/zLibDll/minizip.html )

         Copyright (C) 1998-2010 Gilles Vollant (minizip) ( http://www.winimage.com/zLibDll/minizip.html )

         2016 Jisk Attema Stripped down to bare minimum for unzipping small archive to current directory
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
# include <direct.h>
# include <io.h>
#else
# include <unistd.h>
# include <utime.h>
#endif

#include "unzip.h"

#ifdef _WIN32
#define USEWIN32IOAPI
#include "iowin32.h"
#endif

#define WRITEBUFFERSIZE (8192)

#include "libretro.h"
struct retro_game_info game_info;

static int stripped_do_extract_currentfile(unzFile uf) {
    char filename_inzip[256];
    char filename_out[256];
    char* filename_withoutpath;
    char* p;
    int err=UNZ_OK;
    FILE *fout=NULL;
    void* buf;
    uInt size_buf;

    unz_file_info file_info;
    uLong ratio=0;
    err = unzGetCurrentFileInfo(uf,&file_info,filename_inzip,sizeof(filename_inzip),NULL,0,NULL,0);

    if (err!=UNZ_OK) {
        printf("error %d with zipfile in unzGetCurrentFileInfo\n",err);
        return err;
    }

    size_buf = WRITEBUFFERSIZE;
    buf = (void*)malloc(size_buf);
    if (buf==NULL) {
        printf("Error allocating memory\n");
        return UNZ_INTERNALERROR;
    }

    p = filename_withoutpath = filename_inzip;
    while ((*p) != '\0') {
        if (((*p)=='/') || ((*p)=='\\')) {
            filename_withoutpath = p+1;
        }
        p++;
    }
    strcpy(filename_out, "/dev/shm/");
    strncat(filename_out, filename_withoutpath, 255);
    filename_out[255] = '\0';

    err = unzOpenCurrentFilePassword(uf,NULL);
    if (err!=UNZ_OK) {
        printf("error %d with zipfile in unzOpenCurrentFilePassword\n",err);
    } else {

        fout=fopen(filename_out,"wb");
        // fprintf(stderr, "Extracting: %s\n", filename_out);
        if (fout==NULL) {
            fprintf(stderr, "error opening %s\n",filename_out);
        } else {
            // extracting

            do {
                err = unzReadCurrentFile(uf,buf,size_buf);
                if (err<0) {
                    printf("error %d with zipfile in unzReadCurrentFile\n",err);
                    break;
                }
                if (err>0) {
                    if (fwrite(buf,err,1,fout)!=1) {
                        printf("error in writing extracted file\n");
                        err=UNZ_ERRNO;
                        break;
                    }
                }
            } while (err>0);
            if (fout) {
                fclose(fout);
            }
        }
    }

    if (err==UNZ_OK) {
        err = unzCloseCurrentFile (uf);
        if (err!=UNZ_OK) {
            printf("error %d with zipfile in unzCloseCurrentFile\n",err);
        }
    } else {
        unzCloseCurrentFile(uf); /* don't lose the error */
    }

    free(buf);

    game_info.path = filename_out;
    retro_add_and_replace_image(&game_info);
    
    return err;
}


int stripped_miniunz(const char *zipfilename) {
    int ret_value=1;
    unzFile uf=NULL;

#ifdef USEWIN32IOAPI
    zlib_filefunc_def ffunc;
    fill_win32_filefuncA(&ffunc);
    uf = unzOpen2(zipfilename,&ffunc);
#else
    uf = unzOpen(zipfilename);
#endif
    if (uf==NULL) {
        fprintf(stderr, "Cannot open %s\n",zipfilename);
    } else {
        uLong i;
        unz_global_info gi;

        ret_value = unzGetGlobalInfo(uf,&gi);
        if (ret_value !=UNZ_OK) {
            printf("error %d with zipfile in unzGetGlobalInfo \n",ret_value);
        } else {
            for (i=0;i<gi.number_entry;i++) {
                if (stripped_do_extract_currentfile(uf) != UNZ_OK) {
                    break;
                }

                if ((i+1)<gi.number_entry) {
                    ret_value = unzGoToNextFile(uf);
                    if (ret_value!=UNZ_OK) {
                        printf("error %d with zipfile in unzGoToNextFile\n",ret_value);
                        break;
                    }
                }
            }
            unzClose(uf);
        }
    }
    return ret_value == UNZ_OK;
}
