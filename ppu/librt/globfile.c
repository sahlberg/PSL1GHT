#include <_ansi.h>
#include <_syslist.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reent.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/syslimits.h>
#include <sys/lv2errno.h>

#include <sys/file.h>

char *__cwd = NULL;

static void __glob_file_init(void)  __attribute__((constructor(105)));
static void __glob_file_init(void)
{
    __cwd = strdup("/");
}

static char* deletePathFromPath(char *dirname, int path_count)
{
    int i;

    if (dirname == NULL)
        return NULL;

    i = strlen(dirname);
    while (i >= 0) {
        if (dirname[i] == '/') {
            if (--path_count <= 0)
                break;
        }
        i--;
    }

    if (i > 0)
        dirname[i + 1] = 0;
    else
        strcpy(dirname, "");

    return dirname;
}

char* flattenDirectory(const char *dirname, const char *root)
{
    char *dir = NULL;
    int len, last_pos;
    int was_real_dir;
    char fpath[PATH_MAX + 1];

    if (dirname == NULL)
        return NULL;

    strncat(fpath, dirname, PATH_MAX);

    len = strlen(dirname);
    if (dirname[len - 1] != '/') {
        fpath[len++] = '/';
        fpath[len] = 0;
    }

    last_pos = 0;
    was_real_dir = 0;
    while (1) {
        char *subdir;
        int pos = last_pos;
        while (pos < len && fpath[pos] != '/')
            pos++;
        if (pos >= len)
            break;

        subdir = &fpath[last_pos];

        if (subdir[0] == '.' && subdir[1] == '.' && subdir[2] == '/') {
            if (was_real_dir) {
                dir = deletePathFromPath(dir, 2);
                was_real_dir = (dir != NULL && strlen(dir) > 0);
            } else {
                int copy_len = (pos - last_pos + 1);
                dir = realloc(dir, (dir != NULL ? strlen(dir) : 0) + copy_len);
                strncat(dir, subdir, copy_len);
                was_real_dir = 0;
            }
        } else if (subdir[0] == '/') {
            dir = strdup(root);
        } else if (!(subdir[0] == '.' && subdir[1] == '/')) {
            int copy_len = (pos - last_pos + 1);
            dir = realloc(dir, (dir != NULL ? strlen(dir) : 0) + copy_len);
            strncat(dir, subdir, copy_len);
            was_real_dir = 1;
        }

        fpath[pos] = 0;
        last_pos = pos + 1;
    }

    return dir;
}

int
_DEFUN(__librt_chdir_r, (r,dirname),
       struct _reent *r _AND
       const char *dirname)
{
    char *dir = flattenDirectory(dirname, "/");
    s32 fd, res = sysLv2FsOpenDir(dir, &fd);
    if (res == 0) {
        __cwd = dir;
        sysLv2FsCloseDir(fd);
        return 0;
    }

    free(dir);
    return lv2errno_r(r, res);
}

char*
_DEFUN(__librt_getcwd_r, (r,buf,size),
       struct _reent *r _AND
       char *buf _AND
       size_t size)
{
    size_t cwd_len = strlen(__cwd);
    if (buf == NULL || size <= cwd_len)
        return NULL;

    buf[0] = 0;
    strncat(buf, __cwd, size);

    return buf;
}
