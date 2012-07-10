/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <getopt.h>
#include <limits.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <ftw.h>

/* "external" sources */
#include <uthash.h>

/* globals */
unsigned int width = 77; /* 80 - "FILE MODE" (1 char) -  [] */
const char *name = "dush";
unsigned int fcount = 0;

enum dispsize {G, M, K, B};
const char * const dispsizemap[] = {"GB", "MB", "KB", "B"};

struct args {
   long nbiggest;
   bool full, graph, list, dirs, count;
   enum dispsize size;
   const char *path;
} args;

struct finfo {
   char *name; /* key for dirs */
   unsigned long long size;
   UT_hash_handle hh; /* makes this structure hashable */
};

struct finfo *nbiggestf = NULL;
struct finfo *biggest_dirs = NULL;

/* prototypes */
static void parse_args(int, char **, struct args *);
static int ignore(const char *, const char *);
static void freeres(void);
static void usage(void);

static int size_sort(struct finfo *a, struct finfo *b)
{
   return (b->size - a->size);
}

static void insert_sorted(struct finfo *list, const char *name, unsigned long long size)
{
   /* free old name */
   if (list[0].name != NULL)
      free(list[0].name);

   list[0].size = size;
   list[0].name = strdup(name);

   /* bubble up */
   for (int i = 0; i < args.nbiggest - 1; ++i)
   {
      if (list[i].size > list[i+1].size)
      {
         struct finfo tmp;

         tmp = list[i];
         list[i] = list[i+1];
         list[i+1] = tmp;
      }
      else 
         break;
   }
}

static int walkdirs(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
#if 0
   if ((sb->st_mode & S_IFMT) == S_IFREG)
      printf("R ");
   else if ((sb->st_mode & S_IFMT) == S_IFDIR)
      printf("D ");
   else
      printf("- ");
   printf("%s: %d\n", path, (int)sb->st_size);
#endif

   ++fcount;
   if (args.count && fcount % 10 == 0)
   {
      printf("\r%d", fcount);
      fflush(stdout);
   }

   if (typeflag == FTW_D)
   {
      unsigned int skip = 0;

      /* hardcoded, maybe add an option or a .config */
      skip += ignore(path, ".git");
      skip += ignore(path, ".hg");
      skip += ignore(path, ".svn");

      if (skip != 0)
         return FTW_SKIP_SUBTREE;

      if (args.dirs)
      {
         struct finfo *newdir = malloc(sizeof(*newdir));
         newdir->size = 0;
         newdir->name = strdup(path);
         HASH_ADD_KEYPTR(hh, biggest_dirs, newdir->name, strlen(newdir->name), newdir);
      }
   }

   if (typeflag == FTW_F) 
   {
      if (nbiggestf[0].size < sb->st_size) /* add it to list of biggest files */
         insert_sorted(nbiggestf, path, sb->st_size);

      if (args.dirs)
      {
         struct finfo *dir;
         char *duppath = strdup(path);
         char *dname = dirname(duppath);

         HASH_FIND_STR(biggest_dirs, dname, dir);

         if (dir) 
            dir->size += sb->st_size;
         else 
            fprintf(stderr, "something went wrong (%s)\n", path);

         free(duppath);
      }
   }

   return FTW_CONTINUE;
}

int main(int argc, char **argv)
{
   name = argv[0];

   /* set defaults */
   args.full = false;
   args.graph = false;
   args.list = false;
   args.dirs = false;
   args.count = false;
   args.size = M;
   args.nbiggest = 10;
   args.path = ".";

   parse_args(argc, argv, &args);

   if (args.list) args.count = false;

   nbiggestf = calloc(args.nbiggest, sizeof(*nbiggestf));

   if (nbiggestf == NULL)
   {
      perror(name);
      exit(EXIT_FAILURE);
   }
   
   nftw(args.path, walkdirs, 50, FTW_ACTIONRETVAL|FTW_PHYS);

   if (args.dirs)
   {
      HASH_SORT(biggest_dirs, size_sort);
      struct finfo *s, *tmp;
      HASH_ITER(hh, biggest_dirs, s, tmp)
      {
         if (s->size > nbiggestf[0].size)
            insert_sorted(nbiggestf, s->name, s->size);
         else
            break;
         /* printf("%s %lld\n", s->name, s->size); */
      }
   }

   unsigned long long max = nbiggestf[args.nbiggest - 1].size;

   /* print result */
   if (args.count) printf("\r%d files/directories\n", fcount);

   /* get terminal width if possible */
   struct winsize w;
   if (ioctl(1, TIOCGWINSZ, &w) != -1)
      width = w.ws_col - 3; /* width - MODE -  [] */

   for (int i = args.nbiggest - 1 ; i >= 0; --i)
   {
      if (nbiggestf[i].name == NULL) break;

      /* do not calc size if in list mode */
      if (args.list)
      {
         puts(nbiggestf[i].name);
         /* if in list mode, don't print anything else */
         continue;
      }

      unsigned long long dsize = nbiggestf[i].size;
      switch (args.size)
      {
         case G:
            dsize /= 1024;
            /* fall through */
         case M:
            dsize /= 1024;
            /* fall through */
         case K:
            dsize /= 1024;
            /* fall through */
         case B:
            break;
      }

      /* non list-mode output */
      printf("%s: %lld %s\n",
            args.full ? nbiggestf[i].name : basename(nbiggestf[i].name),
            (long long int)dsize, dispsizemap[args.size]);

      if (args.graph)
      {
         struct stat sb;

         if (stat(nbiggestf[i].name, &sb) == -1)
         {
            freeres();
            exit(EXIT_FAILURE);
         }

         if (S_ISREG(sb.st_mode))
            putchar('R');
         else if (S_ISDIR(sb.st_mode))
            putchar('D');
         else
            putchar('-');

         fputs("[", stdout);
         for (unsigned long long j = 0; j < (unsigned long long)nbiggestf[i].size * width; j += max)
            putchar('#');
         puts("]");
      }
   }

   freeres();

   return 0;
}

static void freeres(void)
{
   /* free stuff */
   if (nbiggestf != NULL)
   {
      for (int i = 0; i < args.nbiggest; ++i)
         if (nbiggestf[i].name != NULL)
            free(nbiggestf[i].name); /* strduped names */

      free(nbiggestf);
   }

   if (args.dirs)
   {
      struct finfo *c, *tmp;

      HASH_ITER(hh, biggest_dirs, c, tmp)
      {
         HASH_DEL(biggest_dirs, c);
         /* printf("c->name: %p\n", c->name); */
         /* printf("c->name: %s\n", c->name); */
         /* printf("c->size: %lld\n", c->size); */
         /* printf("c: %p\n", c); */
         free(c->name);
         free(c);
      }
   }
}

static void parse_args(int argc, char **argv, struct args *args)
{
   int opt;
   long int optn;
   char *endptr;

   while ((opt = getopt(argc, argv, "dchlvgmkbfn:")) != -1)
   {
      switch (opt)
      {
         case 'b':
            args->size = B;
            break;
         case 'k':
            args->size = K;
            break;
         case 'm':
            args->size = M;
            break;
         case 'g':
            args->size = G;
            break;
         case 'c':
            args->count = true;
            break;
         case 'l':
            args->list = true;
            break;
         case 'f':
            args->full = true;
            break;
         case 'd':
            args->dirs = true;
            break;
         case 'v':
            args->graph = true;
            break;
         case 'n':
            errno = 0;
            optn = strtol(optarg, &endptr, 10); 

            if ((errno == ERANGE && (optn == LONG_MAX || optn == LONG_MIN))
                  || (errno != 0 && optn == 0))
            {
               perror(name);
               exit(EXIT_FAILURE);
            }

            if (endptr == optarg)
            {
               fputs("No digits were found\n", stderr);
               exit(EXIT_FAILURE);
            }

            /* everything should be fine, we don't care about additional chars */
            if (optn <= 0) optn = 1;
            args->nbiggest = optn;
            break;
         case 'h':
            /* fall through */
         default: /* '?' */
            usage();
            break; /* never reached */
      }
   }

   if (optind != argc) /* user specified a path */
   {
      const char *path = argv[argc - 1];
      struct stat sb;

      if (stat(path, &sb) == -1)
      {
         perror(name);
         exit(EXIT_FAILURE);
      }
      
      if (S_ISDIR(sb.st_mode))
         args->path = path;
      else 
      {
         fprintf(stderr, "%s is not a directory\n", path);
         exit(EXIT_FAILURE);
      }
   }
   /* else it already defaults to "." */
}

static int ignore(const char *heystack, const char *needle)
{
   size_t n = strlen(needle);
   const char *pos = heystack + strlen(heystack) - n;

   if (strncmp(pos, needle, n) == 0)
      return 1;

   return 0;
}

static void usage(void)
{
   fprintf(stderr, 
         "Usage: %s [OPTION]... [PATH]\n"
         "  -h: print this help\n"
         "  -b|k|m|g: size in B, KB, MB, GB\n"
         "  -v: print a graph\n"
         "  -l: print a list without any additional infos\n"
         "  -f: display full path (not only the basename)\n"
         "  -d: include stats for directories\n"
         "  -c: enable count while reading\n"
         "  -n: NUM biggest files\n", name);
   exit(EXIT_FAILURE);
}
