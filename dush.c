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

#define _GNU_SOURCE /* we need it for glibc specific nftw stuff */
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

/* globals */
unsigned int width = 77; /* 80 - 'space' -  [] */
const char *name = "dush";

enum dispsize {G, M, K, B};
const char * const dispsizemap[] = {"GB", "MB", "KB", "B"};

struct args {
   long nbiggest;
   bool full, graph, list;
   enum dispsize size;
   const char *path;
} args;

struct finfo {
   char *name;
   off_t size;
} *nbiggest = NULL;

/* prototypes */
static void parse_args(int, char **, struct args *);
static int ignore(const char *, const char *);
static void freeres(void);
static void usage(void);

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

   if (typeflag == FTW_D)
   {
      unsigned int skip = 0;

      /* hardcoded, maybe add an option or a .config */
      skip += ignore(path, ".git");
      skip += ignore(path, ".hg");
      skip += ignore(path, ".svn");

      if (skip)
         return FTW_SKIP_SUBTREE;
   }

   if ( (typeflag == FTW_F) && (nbiggest[0].size < sb->st_size) )
   {
      nbiggest[0].size = sb->st_size;
      nbiggest[0].name = strdup(path);

      /* bubble up */
      for (int i = 0; i < args.nbiggest - 1; ++i)
      {
         if (nbiggest[i].size > nbiggest[i+1].size)
         {
            struct finfo tmp;
            
            tmp = nbiggest[i];
            nbiggest[i] = nbiggest[i+1];
            nbiggest[i+1] = tmp;
         }
         else 
            break;
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
   args.size = M;
   args.nbiggest = 10;
   args.path = ".";

   parse_args(argc, argv, &args);

   nbiggest = calloc(args.nbiggest, sizeof(*nbiggest));

   if (nbiggest == NULL)
   {
      perror(name);
      exit(EXIT_FAILURE);
   }

   nftw(args.path, walkdirs, 50, FTW_ACTIONRETVAL|FTW_PHYS);

   off_t max = nbiggest[args.nbiggest - 1].size;
   /* using it for division, just to be safe... */
   max = max > 0 ? max : 1;

   /* print result */

   /* get terminal width if possible */
   struct winsize w;
   if (ioctl(0, TIOCGWINSZ, &w) != -1)
      width = w.ws_col - 3; /* width - 'space' -  [] */

   for (int i = args.nbiggest - 1 ; i >= 0; --i)
   {
      if (nbiggest[i].name == NULL) break;

      /* do not calc size if in list mode */
      if (args.list)
      {
         puts(nbiggest[i].name);
         /* if in list mode, don't print anything else */
         continue;
      }

      off_t dsize = nbiggest[i].size;
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
            args.full ? nbiggest[i].name : basename(nbiggest[i].name),
            (long long int)dsize, dispsizemap[args.size]);

      if (args.graph)
      {
         fputs(" [", stdout);
         for (int j = 0; j < nbiggest[i].size * width / max; ++j)
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
   if (nbiggest != NULL)
   {
      for (int i = 0; i < args.nbiggest; ++i)
         if (nbiggest[i].name != NULL)
            free(nbiggest[i].name); /* strduped names */

      free(nbiggest);
   }
}

static void parse_args(int argc, char **argv, struct args *args)
{
   int opt;
   long int optn;
   char *endptr;

   while ((opt = getopt(argc, argv, "hlvgmkbfn:")) != -1)
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
         case 'l':
            args->list = true;
            break;
         case 'f':
            args->full = true;
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
         "  -n: NUM biggest files\n", name);
   exit(EXIT_FAILURE);
}
