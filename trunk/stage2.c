/* Common stage 2 for ECM, P-1 and P+1 (improved standard continuation).

  Copyright (C) 2001 Paul Zimmermann,
  LORIA/INRIA Lorraine, zimmerma@loria.fr
  See http://www.loria.fr/~zimmerma/records/ecmnet.html

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

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "gmp.h"
#include "ecm.h"
#include "cputime.h"

#define DEBUG

void dickson_ui        (mpz_t r, unsigned int x, unsigned int n, int a);


#define INVF /* precompute 1/F for divisions by F */

void 
dickson_ui (mpz_t r, unsigned int x, unsigned int n, int a)
{
  unsigned int i, b = 0;
  mpz_t t, u;

  if (n == 0)
    {
      mpz_set_ui (r, 1);
      return;
    }
  
  while (n > 2 && (n & 1) == 0)
    {
      b++;
      n >>= 1;
    }
  
  mpz_set_ui (r, x);
  
  mpz_init(t);
  mpz_init(u);

  if (n > 1)
    {
      mpz_set_ui (r, x);
      mpz_mul_ui (r, r, x);
      mpz_sub_si (r, r, a);
      mpz_sub_si (r, r, a); /* r = dickson(x, 2, a) */
      
      mpz_set_ui (t, x);    /* t = dickson(x, 1, a) */
      
      for (i = 2; i < n; i++)
        {
          mpz_mul_si (u, t, a);
          mpz_set (t, r);     /* t = dickson(x, i, a) */
          mpz_mul_ui (r, r, x);
          mpz_sub (r, r, u);  /* r = dickson(x, i+1, a) */
        }
    }
  
  for ( ; b > 0; b--)
    {
      mpz_mul (t, r, r); /* t = dickson(x, n, a) ^ 2 */
      mpz_ui_pow_ui (u, abs (a), n);
      if (n & 1 && a < 0)
        mpz_neg (u, u);
      mpz_mul_2exp (u, u, 1); /* u = 2 * a^n */
      mpz_sub (r, t, u); /* r = dickson(x, 2*n, a) */
      n <<= 1;
    }
  
  mpz_clear(t);
  mpz_clear(u);
}


/* Init table to allow computation of

   Dickson_{E, a} (s + n*D), 

   for successive n, where Dickson_{E, a} is the Dickson polynomial 
   of degree E with parameter a. For a == 0, Dickson_{E, a} (x) = x^E .

   See Knuth, TAOCP vol.2, 4.6.4 and exercise 7 in 4.6.4, and
   "An FFT Extension of the Elliptic Curve Method of Factorization",
   Peter Montgomery, Dissertation, 1992, Chapter 5.
*/

void 
fin_diff_coeff (listz_t coeffs, unsigned int s, unsigned int D, 
                unsigned int E, int dickson_a)
{
  unsigned int i, k;
  
  for (i = 0; i <= E; i++)
    if (dickson_a != 0)         /* fd[i] = dickson_{E,a} (s+i*D) */
      dickson_ui (coeffs[i], s + i * D, E, dickson_a); 
    else                        /* fd[i] = (s+i*D)^E */
      mpz_ui_pow_ui (coeffs[i], s + i * D, E);
  
  for (k = 1; k <= E; k++)
    for (i = E; i >= k; i--)
      mpz_sub (coeffs[i], coeffs[i], coeffs[i-1]);
}


/* Input:  X is the point at end of stage 1
           n is the number to factor
           B2 is the stage 2 bound
           k is the number of blocks
           S is the exponent for Brent-Suyama's extension
           verbose is the verbose level
           invtrick is non-zero iff one uses x+1/x instead of x.
           method: EC_METHOD, PM1_METHOD or PP1_METHOD
           Cf "Speeding the Pollard and Elliptic Curve Methods
               of Factorization", Peter Montgomery, Math. of Comp., 1987,
               page 257: using x^(i^e)+1/x^(i^e) instead of x^(i^(2e))
               reduces the cost of Brent-Suyama's extension from 2*e
               to e+3 multiplications per value of i.
   Output: f is the factor found
   Return value: non-zero iff a factor was found.
*/
int
stage2 (mpz_t f, void *X, mpmod_t modulus, double B2, unsigned int k,
        unsigned int S, int verbose, int method, double B1)
{
  double b2;
  unsigned int i, d, dF, sizeT;
  unsigned long muls;
  mpz_t n;
  listz_t F, G, H, T;
  int youpi = 0, st, st0;
  void *rootsG_state = NULL;
  listz_t *Tree = NULL; /* stores the product tree for F */
#ifdef POLYEVAL
  unsigned int lgk; /* ceil(log(k)/log(2)) */
#else
  polyz_t polyF, polyT;
#endif
#ifdef INVF
  listz_t invF = NULL;
#endif

  if (B2 <= B1)
    return 0;

  st0 = cputime ();

  mpz_init_set(n, modulus->orig_modulus);

/*
  if (verbose >= 2)
    {
      printf ("starting stage 2 with x=");
      mpres_out_str (stdout, 10, X->x, modulus);
      putchar ('\n');
    }
*/

  b2 = ceil(B2 / k); /* b2 = ceil(B2/k): small block size */

  d = bestD (b2);

#if 0
  if (2.0 * (double) d > B1)
    {
      fprintf (stderr, "Error: 2*d > B1\n");
      exit (1);
    }
#endif

  b2 = block_size (d);

  B2 = (double) k * b2;

  dF = phi (d) / 2;

  if (verbose >= 2)
    printf ("B2=%1.0f k=%u b2=%1.0f d=%u dF=%u\n", B2, k, b2, d, dF);

  F = init_list (dF + 1);

  sizeT = 3 * dF - 1 + list_mul_mem (dF);
#ifdef INVF
  if (dF > 3)
    sizeT += dF - 3;
#endif
  T = init_list (sizeT);
  H = T;

  /* needs dF+1 cells in T */
  if (method == PM1_METHOD)
    youpi = pm1_rootsF (f, F, d, X, T, S, modulus, verbose);
  else if (method == PP1_METHOD)
    youpi = pp1_rootsF (F, d, X, T, modulus, verbose);
  else 
    youpi = ecm_rootsF (f, F, d, X, T, S, modulus, verbose);

  if (youpi)
    {
      youpi = 2;
      goto clear_F;
    }

  /* ----------------------------------------------
     |   F    |  invF  |   G   |         T        |
     ----------------------------------------------
     | rootsF |  ???   |  ???  |      ???         |
     ---------------------------------------------- */

#ifdef POLYEVAL
  lgk = ceil_log2 (dF);
  Tree = (listz_t*) malloc (lgk * sizeof(listz_t));
  for (i = 0; i < lgk; i++)
    Tree[i] = init_list (dF);
  list_set (Tree[lgk - 1], F, dF);
#endif

  PolyFromRoots (F, F, dF, T, verbose | 1, n, 'F', Tree, 0);

  /* needs dF+list_mul_mem(dF/2) cells in T */

  mpz_set_ui (F[dF], 1); /* the leading monic coefficient needs to be stored
                             explicitely for PrerevertDivision and polygcd */

  /* ----------------------------------------------
     |   F    |  invF  |   G   |         T        |
     ----------------------------------------------
     |  F(x)  |  ???   |  ???  |      ???         |
     ---------------------------------------------- */

#ifdef INVF
  /* G*H has degree 2*dF-2, hence we must cancel dF-1 coefficients
     to get degree dF-1 */
  if (dF > 1)
    {
      invF = init_list (dF - 1);
      st = cputime ();
#if 0
      list_zero (T, 2 * dF - 3);
      mpz_set_ui (T[2 * dF - 3], 1); /* T[0..2dF-3] = x^(2dF-3) */
      muls = RecursiveDivision (invF, T, T, F + 1, dF - 1, T + 2 * dF - 2, n);
#else
      muls = PolyInvert (invF, F + 2, dF - 1, T, n);
#endif
      /* now invF[0..K-2] = Quo(x^(2dF-3), F) */
      if (verbose >= 2)
        printf ("Computing 1/F took %ums and %lumuls\n", cputime() - st, muls);
      
      /* ----------------------------------------------
         |   F    |  invF  |   G   |         T        |
         ----------------------------------------------
         |  F(x)  | 1/F(x) |  ???  |      ???         |
         ---------------------------------------------- */
    }
#endif

  G = init_list (dF);
  st = cputime ();
  if (method == PM1_METHOD)
    rootsG_state = pm1_rootsG_init (X, 2*d, d, S, modulus);
  else if (method == PP1_METHOD)
    rootsG_state = pp1_rootsG_init (X, 2*d, d, modulus);
  else /* EC_METHOD */
    if ((rootsG_state = ecm_rootsG_init (f, X, 2*d, d, S, modulus)) == NULL)
      {
        youpi = 2;
        goto clear_G;
      };
  
  if (verbose >= 2)
    printf ("Initializing table of differences for G took %dms\n", cputime () - st);

  for (i=0; i<k; i++)
    {
      st = cputime ();
      
      /* needs dF+1 cells in T+dF */
      if (method == PM1_METHOD)
        youpi = pm1_rootsG (f, G, dF, (mpres_t *) rootsG_state, T + dF, S, 
                            modulus, verbose);
      else if (method == PP1_METHOD)
        youpi = pp1_rootsG (G, dF, (mpres_t *) rootsG_state, modulus,
                            verbose);
      else
        youpi = ecm_rootsG (f, G, dF, (point *) rootsG_state, T + dF, S, 
                            modulus, verbose);
      
      if (verbose >= 2)
        printf ("Computing roots of G took %dms\n", cputime () - st);

      if (youpi)
        goto clear_fd;

  /* -----------------------------------------------
     |   F    |  invF  |   G    |         T        |
     -----------------------------------------------
     |  F(x)  | 1/F(x) | rootsG |      ???         |
     ----------------------------------------------- */

      PolyFromRoots (G, G, dF, T + dF, verbose, n, 'G', NULL, 0);
      /* needs 2*dF+list_mul_mem(dF/2) cells in T */

  /* -----------------------------------------------
     |   F    |  invF  |   G    |         T        |
     -----------------------------------------------
     |  F(x)  | 1/F(x) |  G(x)  |      ???         |
     ----------------------------------------------- */

      if (i == 0)
        {
          list_sub (H, G, F, dF); /* coefficients 1 of degree cancel,
                                     thus T is of degree < dF */
          /* ------------------------------------------------
             |   F    |  invF  |    G    |         T        |
             ------------------------------------------------
             |  F(x)  | 1/F(x) |  ???    |G(x)-F(x)|  ???   |
             ------------------------------------------------ */
        }
      else
	{
          /* since F and G are monic of same degree, G mod F = G - F */
          list_sub (G, G, F, dF);

          /* ------------------------------------------------
             |   F    |  invF  |    G    |         T        |
             ------------------------------------------------
             |  F(x)  | 1/F(x) |G(x)-F(x)|  H(x)  |         |
             ------------------------------------------------ */

	  st = cputime ();
	  /* previous G mod F is in H, with degree < dF, i.e. dF coefficients:
	     requires 3dF-1+list_mul_mem(dF) cells in T */
	  muls = list_mulmod2 (H, T + dF, G, H, dF, T + 3 * dF - 1, n);
          if (verbose >= 2)
            printf ("Computing G * H took %ums and %lumuls\n", cputime() - st,
                    muls);

          /* ------------------------------------------------
             |   F    |  invF  |    G    |         T        |
             ------------------------------------------------
             |  F(x)  | 1/F(x) |G(x)-F(x)| G * H  |         |
             ------------------------------------------------ */

	  st = cputime ();
#ifdef INVF
          muls = PrerevertDivision (H, F, invF, dF, T + 2 * dF - 1, n);
#else
          mpz_set_ui (T[2*dF-1], 0); /* since RecursiveDivision expects a
                                        dividend of 2*dF coefficients */
	  muls = RecursiveDivision (G, H, H, F, dF, T + 2 * dF, n);
#endif
          if (verbose >= 2)
            printf ("Reducing G * H mod F took %ums and %lumuls\n",
                    cputime() - st, muls);
	}
    }

#ifdef POLYEVAL
  st = cputime ();
  polyeval (T, dF, Tree, T + dF + 1, n, verbose, 0);
  if (verbose >= 2)
    printf ("Computing polyeval(F,G) took %dms\n", cputime() - st);
  youpi = list_gcd (f, T, dF, n) ? 2 : 0;
  for (i = 0; i < lgk; i++)
    clear_list (Tree[i], dF);
  free (Tree);
#else
  st = cputime ();
  init_poly_list (polyF, dF, F);
  init_poly_list (polyT, dF - 1, T);
  if ((youpi = poly_gcd (f, polyF, polyT, n, T + dF)))
    NTL_get_factor (f);
  if (verbose >= 2)
    printf ("Computing gcd of F and G took %dms\n", cputime() - st);
#endif

 clear_fd:
  if (method == PM1_METHOD)
    pm1_rootsG_clear ((mpres_t *) rootsG_state, S, modulus);
  else if (method == PP1_METHOD)
    pp1_rootsG_clear ((mpres_t *) rootsG_state, modulus);
  else /* EC_METHOD */
    ecm_rootsG_clear ((point *) rootsG_state, S, modulus);

clear_G:
  clear_list (G, dF);

#ifdef INVF
  if (dF > 1)
    clear_list (invF, dF - 1);
#endif

 clear_F:
  clear_list (T, sizeT);
  clear_list (F, dF + 1);

  if (verbose >= 1)
    printf ("Stage 2 took %dms\n", cputime() - st0);

  return youpi;
}
