/* GMP-ECM -- Integer factorization with ECM and Pollard 'P-1' methods.

  Copyright 2001, 2002, 2003 Alexander Kruppa and Paul Zimmermann.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation; either version 2 of the License, or (at your
  option) any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along
  with this program; see the file COPYING.  If not, write to the Free
  Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
  02111-1307, USA.
*/

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "gmp.h"
#include "ecm.h"

/* people keeping track of champions and corresponding url's */
unsigned int champion_digits[3] = { 51, 37, 34 };
char *champion_keeper[3] =
{ "Richard Brent <rpb@comlab.ox.ac.uk>",
  "Paul Zimmermann <zimmerma@loria.fr>",
  "Paul Zimmermann <zimmerma@loria.fr>"};
char *champion_url[3] =
{"ftp://ftp.comlab.ox.ac.uk/pub/Documents/techpapers/Richard.Brent/champs.txt",
 "http://www.loria.fr/~zimmerma/records/Pminus1.html",
 "http://www.loria.fr/~zimmerma/records/Pplus1.html"};

int read_number (mpz_t, FILE *);

/* Tries to read a number from a line from fd and stores it in r.
   Keeps reading lines until a number is found. Lines beginning with "#"
     are skipped.
   Returns 1 if a number was successfully read, 0 if no number can be read 
     (i.e. at EOF)
*/
int 
read_number (mpz_t r, FILE *fd)
{
  int c;
  
new_line:
  c = fgetc (fd);
  
  /* Skip comment lines beginning with '#' */
  if (c == '#')
    {
      do
        c = fgetc (fd);
      while (c != EOF && c != '\n');
      if (c == '\n')
        goto new_line;
    }
  else
    { /* Look for a digit character */
      while (c != EOF && c != '\n' && ! isdigit (c))
        c = fgetc (fd);
      if (c == '\n')
        goto new_line;
    }  
  
  if (c == EOF)
    return 0;
  
  ungetc (c, fd);

  mpz_inp_str (r, fd, 0);
  
  /* Consume remainder of the line */
  do
    c = fgetc (fd);
  while (c != EOF && c != '\n');

  return 1;
}

/******************************************************************************
*                                                                             *
*                                Main program                                 *
*                                                                             *
******************************************************************************/

int
main (int argc, char *argv[])
{
  mpz_t x, sigma, A, n, f, orig_x0;
  mpq_t rat_x0;
  double B1, B1done, B2, B2min;
  int result = 0;
  int verbose = 1; /* verbose level */
  int method = EC_METHOD;
  int specific_x0 = 0; /* 1=starting point supplied by user, 0=random or */
                       /* compute from sigma */
  int factor_is_prime, cofactor_is_prime;
        /* If a factor was found, indicate whether factor, cofactor are */
        /* prime. If no factor was found, both are zero. */
  int repr = 0;
#ifdef POLYEVAL
  int k = 5;
#else /* POLYGCD is more expensive -> perform more blocks */
  int k = 8; /* default number of blocks in stage 2 */
#endif
  int S = 0; /* Degree for Brent-Suyama extension requested by user */
             /* Positive value: use S-th power, */
             /* negative: use degree |S| Dickson poly */
  gmp_randstate_t randstate;
  char *savefilename = NULL, *resumefilename = NULL;
  char *endptr[1]; /* to parse B2 or B2min-B2max */
  char rtime[256], who[256], comment[256], program[256];
  FILE *savefile = NULL, *resumefile = NULL;

#ifdef MEMORY_DEBUG
  tests_memory_start ();
#endif

  /* Init variables we might need to store options */
  mpz_init (sigma);
  mpz_init (A);
  mpq_init (rat_x0);

  /* first look for options */
  while ((argc > 1) && (argv[1][0] == '-'))
    {
      if (strcmp (argv[1], "-pm1") == 0)
	{
	  method = PM1_METHOD;
	  argv++;
	  argc--;
	}
      else if (strcmp (argv[1], "-pp1") == 0)
	{
	  method = PP1_METHOD;
	  argv++;
	  argc--;
	}
      else if (strcmp (argv[1], "-q") == 0)
	{
	  verbose = 0;
	  argv++;
	  argc--;
	}
      else if (strcmp (argv[1], "-v") == 0)
	{
	  verbose = 2;
	  argv++;
	  argc--;
	}
      else if (strcmp (argv[1], "-mpzmod") == 0)
        {
          repr = 1;
	  argv++;
	  argc--;
        }
      else if (strcmp (argv[1], "-modmuln") == 0)
        {
          repr = 2;
	  argv++;
	  argc--;
        }
      else if (strcmp (argv[1], "-redc") == 0)
        {
          repr = 3;
	  argv++;
	  argc--;
        }
      else if (strcmp (argv[1], "-nobase2") == 0)
        {
          repr = -1;
	  argv++;
	  argc--;
        }
      else if ((argc > 2) && (strcmp (argv[1], "-x0")) == 0)
        {
          if (mpq_set_str (rat_x0, argv[2], 0))
            {
              fprintf (stderr, "Error, invalid starting point: %s\n", argv[2]);
              exit (1);
            }
          specific_x0 = 1;
	  argv += 2;
	  argc -= 2;
        }
      else if ((argc > 2) && (strcmp (argv[1], "-sigma")) == 0)
        {
          mpz_set_str (sigma, argv[2], 0);
	  argv += 2;
	  argc -= 2;
        }
      else if ((argc > 2) && (strcmp (argv[1], "-A")) == 0)
        {
          mpz_set_str (A, argv[2], 0);
	  argv += 2;
	  argc -= 2;
        }
      else if ((argc > 2) && (strcmp (argv[1], "-power")) == 0)
        {
          S = abs (atoi (argv[2]));
	  argv += 2;
	  argc -= 2;
        }
      else if ((argc > 2) && (strcmp (argv[1], "-dickson") == 0))
        {
          S = - abs( atoi (argv[2]));
	  argv += 2;
	  argc -= 2;
        }
      else if ((argc > 2) && (strcmp (argv[1], "-k") == 0))
	{
	  k = atoi (argv[2]);
	  argv += 2;
	  argc -= 2;
	}
      else if ((argc > 2) && (strcmp (argv[1], "-save") == 0))
	{
	  savefilename = argv[2];
	  argv += 2;
	  argc -= 2;
	}
      else if ((argc > 2) && (strcmp (argv[1], "-resume") == 0))
	{
	  resumefilename = argv[2];
	  argv += 2;
	  argc -= 2;
	}
      else
	{
	  fprintf (stderr, "Unknown option: %s\n", argv[1]);
	  exit (1);
	}
    }

  if (argc < 2)
    {
      fprintf (stderr, "Usage: ecm B1 [[B2min-]B2] < file\n");
      fprintf (stderr, "\nParameters:\n");
      fprintf (stderr, "  B1           stage 1 bound\n");
      fprintf (stderr, "  B2           stage 2 bound (or interval B2min-B2max)\n");
      fprintf (stderr, "\nOptions:\n");
      fprintf (stderr, "  -x0 x        use x as initial point\n"); 
      fprintf (stderr, "  -sigma s     use s as curve generator [ecm]\n");
      fprintf (stderr, "  -A a         use a as curve parameter [ecm]\n");
      fprintf (stderr, "  -k n         perform n steps in stage 2\n");
      fprintf (stderr, "  -power n     use x^n for Brent-Suyama's extension\n");
      fprintf (stderr, "  -dickson n   use n-th Dickson's polynomial for Brent-Suyama's extension\n");
      fprintf (stderr, "  -pm1         perform P-1 instead of ECM\n");
      fprintf (stderr, "  -pp1         perform P+1 instead of ECM\n");
      fprintf (stderr, "  -q           quiet mode\n");
      fprintf (stderr, "  -v           verbose mode\n");
      fprintf (stderr, "  -mpzmod      use GMP's mpz_mod for mod reduction\n");
      fprintf (stderr, "  -modmuln     use Montgomery's MODMULN for mod reduction\n");
      fprintf (stderr, "  -redc        use Montgomery's REDC for mod reduction\n");
      fprintf (stderr, "  -nobase2     disable special base-2 code\n");
      fprintf (stderr, "  -save file   save residues at end of stage 1 to file\n");
      fprintf (stderr, "  -resume file resume residues from file, reads from stdin if file is \"-\"\n");
      exit (1);
    }

  if (verbose >= 1)
    {
      printf ("GMP-ECM %s [powered by GMP %s", ECM_VERSION, gmp_version);
#ifdef POLYGCD
      printf (" and NTL %u.%u", NTL_major_version (), NTL_minor_version ());
#endif
      printf ("]\n");
    }

  /* set first stage bound B1 */
  B1 = strtod (argv[1], &argv[1]);
  if (*argv[1] == '-')
    {
      B1done = B1;
      B1 = strtod (argv[1] + 1, NULL);
    }
  else
    B1done = 1.0;
  B2min = B1;

  if (B1 < 0 || B1done < 0)
    {
      fprintf(stderr, "Bound values must be positive\n");
      exit(EXIT_FAILURE);
    }

  /* check B1 is not too large */
  if (B1 > MAX_B1)
    {
      fprintf (stderr, "Too large stage 1 bound, limit is %f\n", MAX_B1);
      exit (1);
    }

#ifdef POLYGCD
  NTL_init ();
#endif

  B2 = 0.0;
  /* parse B2 or B2min-B2max */
  if (argc >= 3)
    {
      B2 = strtod (argv[2], endptr);
      if (*endptr == argv[2])
        {
          fprintf (stderr, "Error: B2 or B2min-B2max expected: %s\n", argv[2]);
          exit (1);
        }
      if (**endptr == '-')
        {
          B2min = B2;
          B2 = atof (*endptr + 1);
        }
    }

  /* Open save file for writing, if saving is requested */
  /* FIXME: append by default ? */
  if (savefilename != NULL)
    {
      savefile = fopen (savefilename, "w");
      if (savefile == NULL)
        {
          fprintf (stderr, "Could not open file %s for writing\n", savefilename);
          exit (EXIT_FAILURE);
        }
    }

  /* Open resume file for reading, if resuming is requested */
  if (resumefilename != NULL)
    {
      if (strcmp (resumefilename, "-") == 0)
        resumefile = stdin;
      else
        resumefile = fopen (resumefilename, "r");
      
      if (resumefile == NULL)
        {
          fprintf (stderr, "Could not open file %s for reading\n", 
                   resumefilename);
          exit (EXIT_FAILURE);
        }
    }

  if (resumefile && (mpz_sgn (sigma) != 0 || mpz_sgn (A) || specific_x0))
    {
      fprintf (stderr, "Warning: -sigma, -A and -x0 parameters are ignored when resuming from\nsave files.\n");
      mpz_set_ui (sigma, 0);
      mpz_set_ui (A, 0);
      specific_x0 = 0;
    }

  mpz_init (n); /* number(s) to factor */
  mpz_init (f); /* factor found */
  mpz_init (x); /* stage 1 residue */
  mpz_init (orig_x0); /* starting point, for save file */

  /* We may need random numbers for sigma/starting point */
  /* todo: seed needs higher resolution */
  gmp_randinit_default (randstate);
  gmp_randseed_ui (randstate, time (NULL) + getpid ());

  /* loop for number in standard input or file */
  while (feof (stdin) == 0)
    {
      if (resumefile != NULL)
        {
          if (!read_resumefile_line (&method, x, n, sigma, A, orig_x0, 
                &B1done, program, who, rtime, comment, resumefile))
            break;
          
          if (verbose > 0)
            {
              printf ("Resuming ");
              if (method == EC_METHOD)
                printf ("ECM");
              else if (method == PM1_METHOD)
                printf ("P-1");
              else if (method == PP1_METHOD)
                printf ("P+1");
              printf (" residue ");
              if (program[0] || who[0] || rtime[0])
                printf ("saved ");
              if (who[0])
                printf ("by %s ", who);
              if (program[0])
                printf ("with %s ", program);
              if (rtime[0])
                printf ("on %s ", rtime);
              if (comment[0])
                printf ("(%s)", comment);
              printf("\n");
            }
        }
      else
        {
          if (!read_number (n, stdin))
            break;

          /* Set effective seed for factoring attempt on this number */

          if (specific_x0) /* convert rational value to integer */
            {
              mpz_t inv;

              mpz_init (inv);
              mpz_invert (inv, mpq_denref (rat_x0), n);
              mpz_mul (inv, mpq_numref (rat_x0), inv);
              mpz_mod (x, inv, n);
              mpz_clear (inv);
            }
          else /* Make a random starting point for P-1 and P+1. ECM will */
               /* compute a suitable value from sigma or A if x is zero */
            {
              if (method == EC_METHOD)
                mpz_set_ui (x, 0);
              if (method == PP1_METHOD)
                pp1_random_seed (x, n, randstate);
              if (method == PM1_METHOD)
                pm1_random_seed (x, n, randstate);
            }
         
          if (B1done <= 1.0)
            mpz_set (orig_x0, x);
          
          /* Make a random sigma if we have neither sigma nor A given */
          if (method == EC_METHOD && !mpz_sgn (sigma) && !mpz_sgn (A))
            {
              /* Make random sigma, 0 < sigma <= 2^32 */
              mpz_urandomb (sigma, randstate, 32);
              mpz_add_ui (sigma, sigma, 1);
            }
        }

      if (verbose > 0)
	{
	  if (mpz_sizeinbase (n, 10) < 1000)
	    {
	      char *str;
	      str = mpz_get_str (NULL, 10, n);
	      printf ("Input number is %s (%u digits)\n", str,
		      (unsigned) strlen (str));
	      fflush (stdout);
	      __gmp_free_func (str, strlen (str) + 1);
	    }
	  else
	    printf ("Input number has around %u digits\n", (unsigned) 
		    mpz_sizeinbase (n, 10));
	}

      factor_is_prime = cofactor_is_prime = 0;

      if (method == PM1_METHOD)
        result = pm1 (f, x, n, B1done, B1, B2min, B2, k, S, verbose, repr);
      else if (method == PP1_METHOD)
        result = pp1 (f, x, n, B1done, B1, B2min, B2, k, S, verbose, repr);
      else /* ECM */
        if (mpz_sgn (sigma) == 0) /* If sigma is zero, then we use the A value instead */
          result = ecm (f, x, A, n, B1done, B1, B2min, B2, k, S, verbose, repr, 1);
        else
          result = ecm (f, x, sigma, n, B1done, B1, B2min, B2, k, S, verbose, repr, 0);

      if (result != 0)
	{
          printf ("********** Factor found in step %u: ", result);
          mpz_out_str (stdout, 10, f);
          printf ("\n");
	  if (mpz_cmp (f, n))
	    {
	      /* prints factor found and cofactor on standard error. */
	      factor_is_prime = mpz_probab_prime_p (f, 25);
	      printf ("Found %s factor of %u digits: ", 
	              factor_is_prime ? "probable prime" : "composite",
	              nb_digits (f));
	      mpz_out_str (stdout, 10, f);
	      printf ("\n");

	      mpz_divexact (n, n, f);
	      cofactor_is_prime = mpz_probab_prime_p (n, 25);
	      printf ("%s cofactor ", 
	              cofactor_is_prime ? "Probable prime" : "Composite");
	      if (verbose > 0)
		mpz_out_str (stdout, 10, n);
	      printf (" has %u digits\n", nb_digits (n));
	      
              /* check for champions (top ten for each method) */
	      if (factor_is_prime && nb_digits (f) >= champion_digits[method])
                    {
                      printf ("Report your potential champion to %s\n",
                              champion_keeper[method]);
                      printf("(see %s)\n", champion_url[method]);
                    }
            }
	  else
	    printf ("Found input number N\n");
	  fflush (stdout);
	}
      
      /* if quiet, prints composite cofactors on standard output. */
      if ((verbose == 0) && !cofactor_is_prime)
	{
	  mpz_out_str (stdout, 10, n);
	  putchar ('\n');
	  fflush (stdout);
	}

      /* Write composite cofactors to savefile if requested */
      /* If no factor was found, we consider cofactor composite and write it */
      if (savefile != NULL && !cofactor_is_prime)
        {
          mpz_mod (x, x, n); /* Reduce stage 1 residue wrt new cofactor, in
                               case a factor was found */
          write_resumefile_line (savefile, method, B1, sigma, A, x, n, 
                                 orig_x0, comment);
        }
    }

#ifdef POLYGCD
  NTL_clear ();
#endif

  gmp_randclear (randstate);

  mpz_clear (orig_x0);
  mpz_clear (x);
  mpz_clear (f);
  mpz_clear (n);
  mpz_clear (sigma);
  mpz_clear (A);
  mpq_clear (rat_x0);

#ifdef MEMORY_DEBUG
  tests_memory_end ();
#endif

  /* exit 0 iff a factor was found for the last input */
  return result ? 0 : 1;
}
