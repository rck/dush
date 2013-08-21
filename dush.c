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
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <ftw.h>

/* "external" sources */
#include <uthash.h>

#include "config.h"

#define COUNT_OF(x) (sizeof(x)/sizeof(x[0]))

/* globals */
static unsigned int width = 77; /* 80 - "FILE MODE" (1 char) -  [] */
static const char *name = "dush";
static unsigned int fcount = 0;
static unsigned long long dirsize = 4096; /* gets calculated later */
static volatile bool terminating = false;
static bool top_dir = true;

enum dispsize {G, M, K, B};
static const char * const dispsizemap[] = {"GB", "MB", "KB", "B"};

static struct args {
   long nbiggest;
   bool full, graph, list, dirs, count, subdirs;
   enum dispsize size;
   const char *path;
} args = {0};

struct finfo {
   char *name; /* key for dirs */
   unsigned long long size;
};

struct dinfo {
   struct finfo finfo;
   struct dinfo *parent, *child;
   bool donated;
   UT_hash_handle hh; /* makes this structure hashable */
};

struct exclinfo {
   char *name;
   size_t length;
};

static struct finfo *nbiggestf = NULL;
static struct dinfo *all_dirs = NULL;

static unsigned int excludecount = 0;
static unsigned int excludeavail = 3;
static struct exclinfo *excludes = NULL;


/* prototypes */
static void parse_args(int, char **, struct args *);
static bool ignore_dir(const char *);
static void freeres(void);
static void usage(void);
static void signal_handler(int);

static int size_sort(struct dinfo *a, struct dinfo *b)
{
   return (b->finfo.size - a->finfo.size);
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

   if (terminating) return FTW_STOP;

   ++fcount;
   if (args.count && fcount % 10 == 0)
   {
      printf("\r%d", fcount);
      fflush(stdout);
   }

   if (typeflag == FTW_D)
   {
      if (excludecount > 0  && ignore_dir(path))
         return FTW_SKIP_SUBTREE;

      if (args.dirs)
      {
         struct dinfo *newdir = malloc(sizeof(*newdir));
         newdir->finfo.size = dirsize;
         newdir->finfo.name = strdup(path);
         newdir->parent = NULL;
         newdir->child = NULL;
         newdir->donated = false;
         HASH_ADD_KEYPTR(hh, all_dirs, newdir->finfo.name, strlen(newdir->finfo.name), newdir);

         if (args.subdirs)
         {
            if (!top_dir) /* get parent */
            {
               struct dinfo *dir;
               char *duppath = strdup(path);
               char *dname = dirname(duppath);

               HASH_FIND_STR(all_dirs, dname, dir);

               if (dir) 
               {
                  dir->child = newdir;
                  newdir->parent = dir;
               }
               else 
                  fprintf(stderr, "something went wrong (%s)\n", path);

               free(duppath);
            }
            else
               top_dir = false;
         }
      }
   }

   if (typeflag == FTW_F) 
   {
      if (nbiggestf[0].size < sb->st_size) /* add it to list of biggest files */
         insert_sorted(nbiggestf, path, sb->st_size);

      if (args.dirs)
      {
         struct dinfo *dir;
         char *duppath = strdup(path);
         char *dname = dirname(duppath);

         HASH_FIND_STR(all_dirs, dname, dir);

         if (dir) 
            dir->finfo.size += sb->st_size;
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
   args.subdirs = false;
   args.count = false;
   args.size = M;
   args.nbiggest = 10;
   args.path = ".";

   /* setup signal handler */
   sigset_t blocked_signals;

   if(sigfillset(&blocked_signals) == -1) 
      exit(EXIT_FAILURE);
   else
   {
      const int signals[] = { SIGINT, SIGQUIT, SIGTERM };
      struct sigaction s;
      s.sa_handler = signal_handler;
      (void) memcpy(&s.sa_mask, &blocked_signals, sizeof(s.sa_mask));
      s.sa_flags = SA_RESTART;
      for(int i = 0; i < COUNT_OF(signals); ++i) 
         if (sigaction(signals[i], &s, NULL) < 0) 
         {
            perror(name);
            exit(EXIT_FAILURE);
         }
   }

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
      struct dinfo *s, *tmp;

      if (args.subdirs)
      {
         HASH_ITER(hh, all_dirs, s, tmp)
         {
            if (s->child == NULL) /* at a leaf, push size */
            {
               unsigned long long pushsize = 0;
               while (s->parent != NULL)
               {
                  if (!s->donated)
                  {
                     pushsize = s->finfo.size;
                     s->donated = true;
                  }
                  s->parent->finfo.size += pushsize;
                  s = s->parent;
               }
            }
         }
      }

      HASH_SORT(all_dirs, size_sort);

      HASH_ITER(hh, all_dirs, s, tmp)
      {
         if (s->finfo.size > nbiggestf[0].size)
            insert_sorted(nbiggestf, s->finfo.name, s->finfo.size);
         else
            break;
      }
   }

   unsigned long long max = nbiggestf[args.nbiggest - 1].size;

   /* print result */
   if (args.count) printf("\r%d files/directories\n", fcount);

   /* get terminal width if possible */
#ifdef HAVE_STRUCT_WINSIZE
   struct winsize w;
   if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != -1)
      width = w.ws_col - 3; /* width - MODE -  [] */
#endif /* else there is a a global default */

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
            dsize, dispsizemap[args.size]);

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
   if (excludes != NULL)
      free(excludes);

   if (nbiggestf != NULL)
   {
      for (int i = 0; i < args.nbiggest; ++i)
         if (nbiggestf[i].name != NULL)
            free(nbiggestf[i].name); /* strduped names */

      free(nbiggestf);
   }

   if (args.dirs)
   {
      struct dinfo *c, *tmp;

      HASH_ITER(hh, all_dirs, c, tmp)
      {
         HASH_DEL(all_dirs, c);
         free(c->finfo.name);
         free(c);
      }
   }
}

static void parse_args(int argc, char **argv, struct args *args)
{
   int opt;
   long int optn;
   char *endptr;

#ifdef HAVE_GETOPT_LONG
   static struct option longopts[] = {
      { "count",     no_argument,         NULL, 'c' },
      { "list",      no_argument,         NULL, 'l' },
      { "full",      no_argument,         NULL, 'f' },
      { "dirs",      no_argument,         NULL, 'd' },
      { "subdirs",   no_argument,         NULL, 's' },
      { "graph",     no_argument,         NULL, 'v' },
      { "num",       required_argument,   NULL, 'n' },
      { "exclude",   required_argument,   NULL, 'e' },
      { "help",      no_argument,         NULL, 'h' },
      { NULL,        0,                   NULL, 0 }
   };
#endif


#ifdef HAVE_GETOPT_LONG
   while ((opt = getopt_long(argc, argv, "dschlvgmkbfn:e:", longopts, NULL)) != -1)
#else
   while ((opt = getopt(argc, argv, "dschlvgmkbfn:e:")) != -1)
#endif
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
         case 's':
            args->subdirs = true;
            /* fall through, enable dirs */
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
         case 'e':
            if (excludecount == 0)
               excludes = malloc(excludeavail * sizeof(*excludes));

            if (excludes == NULL)
            {
               perror(name);
               exit(EXIT_FAILURE);
            }

            if (excludecount == excludeavail)
            {
               excludeavail *= 2;
               excludes = realloc(excludes, excludeavail * sizeof(*excludes));
               if (excludes == NULL)
               {
                  perror(name);
                  exit(EXIT_FAILURE);
               }
            }
            excludes[excludecount].name = optarg;
            excludes[excludecount].length = strlen(optarg);
            excludecount++;
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
      {
         args->path = path;
         dirsize = sb.st_size;
      }
      else 
      {
         fprintf(stderr, "%s is not a directory\n", path);
         exit(EXIT_FAILURE);
      }
   }
   /* else it already defaults to "." */
}

static bool ignore_dir(const char *path)
{
   size_t np = strlen(path);

   for (int i = 0; i < excludecount; ++i)
   {
      size_t nn = excludes[i].length;
      if (np < nn)
         return false;

      const char *needle = excludes[i].name;
      const char *pos = path + np - nn;

      if (strncmp(pos, needle, nn) == 0)
      {
         return true;
      }
   }

   return false;
}

static void usage(void)
{
   fprintf(stderr, 
         "%s Version %d.%d:\n\n"
         "Usage: %s [OPTION]... [PATH]\n"
         "  -h: print this help\n"
         "  -b|k|m|g: size in B, KB, MB, GB\n"
         "  -e: exclude directories with the given extension\n"
         "      e.g.: dush -e .git -e .svn ~\n"
         "  -v: print a graph\n"
         "  -l: print a list without any additional infos\n"
         "  -f: display full path (not only the basename)\n"
         "  -d: include stats for directories\n"
         "  -s: account size of subdirectories\n"
         "  -c: enable count while reading\n"
         "  -n: NUM biggest files\n"
#ifdef HAVE_GETOPT_LONG
         "\nFor long options please refere to the man page\n"
#endif
         , name, VERSION_MAJOR, VERSION_MINOR, name);
   exit(EXIT_FAILURE);
}

static void signal_handler(int sig)
{
   terminating = true;
}
