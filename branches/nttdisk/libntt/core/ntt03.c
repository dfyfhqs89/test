#include "ntt-impl.h"

#define NC 3

static const uint8_t fixed_const[NC] = {1};

static const uint8_t *
ntt3_get_fixed_ntt_const(void)
{
  return fixed_const;
}

void
ntt3_init(spv_t out, sp_t p, sp_t d, 
	  sp_t primroot, sp_t order)
{
  nttdata_init_generic(&ntt3_config, out, p, d, primroot, order);
}

static void
ntt3_run(spv_t x, spv_size_t stride,
	  sp_t p, spv_t ntt_const)
{
  sp_t p0, p1, p2;
  sp_t x0, x1, x2;
  sp_t     t1, t2;

  x0 = x[0 * stride];
  x1 = x[1 * stride];
  x2 = x[2 * stride];

  t1 = sp_ntt_add(x1, x2, p);
  t2 = sp_ntt_sub_partial(x1, x2, p);

  p0 = sp_ntt_add(x0, t1, p);

  p1 = sp_ntt_mul(t1, ntt_const[1], ntt_const[NC+1], p);
  p2 = sp_ntt_mul(t2, ntt_const[2], ntt_const[NC+2], p);

  p1 = sp_ntt_add(p0, p1, p);

  t1 = sp_ntt_add(p1, p2, p);
  t2 = sp_ntt_sub(p1, p2, p);

  x[0 * stride] = p0;
  x[1 * stride] = t1;
  x[2 * stride] = t2;
}

#ifdef HAVE_SSE2
static void
ntt3_run_simd(spv_t x, spv_size_t stride,
	  sp_t p, spv_t ntt_const)
{
  sp_simd_t p0, p1, p2;
  sp_simd_t x0, x1, x2;
  sp_simd_t     t1, t2;

  x0 = sp_simd_gather(x + 0 * stride);
  x1 = sp_simd_gather(x + 1 * stride);
  x2 = sp_simd_gather(x + 2 * stride);

  t1 = sp_ntt_add_simd(x1, x2, p);
  t2 = sp_ntt_sub_partial_simd(x1, x2, p);

  p0 = sp_ntt_add_simd(x0, t1, p);

  p1 = sp_ntt_mul_simd(t1, ntt_const[1], ntt_const[NC+1], p);
  p2 = sp_ntt_mul_simd(t2, ntt_const[2], ntt_const[NC+2], p);

  p1 = sp_ntt_add_simd(p0, p1, p);

  t1 = sp_ntt_add_simd(p1, p2, p);
  t2 = sp_ntt_sub_simd(p1, p2, p);

  sp_simd_scatter(p0, x + 0 * stride);
  sp_simd_scatter(t1, x + 1 * stride);
  sp_simd_scatter(t2, x + 2 * stride);
}
#endif

static void
ntt3_twiddle_run(spv_t x, spv_size_t stride,
	  spv_size_t num_transforms,
	  sp_t p, spv_t ntt_const)
{
  spv_size_t i = 0;

#ifdef HAVE_SSE2
  spv_size_t num_simd = SP_SIMD_VSIZE * (num_transforms / SP_SIMD_VSIZE);

  for (i = 0; i < num_simd; i += SP_SIMD_VSIZE)
      ntt3_run_simd(x + i, stride, p, ntt_const);
#endif

  for (; i < num_transforms; i++)
    ntt3_run(x + i, stride, p, ntt_const);
}


static void
ntt3_pfa_run_core(spv_t x, spv_size_t start,
	  spv_size_t inc, spv_size_t n,
	  sp_t p, spv_t ntt_const)
{
  spv_size_t j0, j1, j2;
  sp_t p0, p1, p2;
  sp_t x0, x1, x2;
  sp_t     t1, t2;

  j0 = start;
  j1 = sp_array_inc(j0, inc, n);
  j2 = sp_array_inc(j0, 2 * inc, n);

  x0 = x[j0];
  x1 = x[j1];
  x2 = x[j2];

  t1 = sp_ntt_add(x1, x2, p);
  t2 = sp_ntt_sub_partial(x1, x2, p);

  p0 = sp_ntt_add(x0, t1, p);

  p1 = sp_ntt_mul(t1, ntt_const[1], ntt_const[NC+1], p);
  p2 = sp_ntt_mul(t2, ntt_const[2], ntt_const[NC+2], p);

  p1 = sp_ntt_add(p0, p1, p);

  t1 = sp_ntt_add(p1, p2, p);
  t2 = sp_ntt_sub(p1, p2, p);

  x[j0] = p0;
  x[j1] = t1;
  x[j2] = t2;
}

#ifdef HAVE_SSE2
static void
ntt3_pfa_run_core_simd(spv_t x, spv_size_t start,
	  spv_size_t inc, spv_size_t inc2, spv_size_t n,
	  sp_t p, spv_t ntt_const)
{
  spv_size_t j0, j1, j2;
  sp_simd_t p0, p1, p2;
  sp_simd_t x0, x1, x2;
  sp_simd_t     t1, t2;

  j0 = start;
  j1 = sp_array_inc(j0, inc, n);
  j2 = sp_array_inc(j0, 2 * inc, n);

  x0 = sp_simd_pfa_gather(x, j0, inc2, n);
  x1 = sp_simd_pfa_gather(x, j1, inc2, n);
  x2 = sp_simd_pfa_gather(x, j2, inc2, n);

  t1 = sp_ntt_add_simd(x1, x2, p);
  t2 = sp_ntt_sub_partial_simd(x1, x2, p);

  p0 = sp_ntt_add_simd(x0, t1, p);

  p1 = sp_ntt_mul_simd(t1, ntt_const[1], ntt_const[NC+1], p);
  p2 = sp_ntt_mul_simd(t2, ntt_const[2], ntt_const[NC+2], p);

  p1 = sp_ntt_add_simd(p0, p1, p);

  t1 = sp_ntt_add_simd(p1, p2, p);
  t2 = sp_ntt_sub_simd(p1, p2, p);

  sp_simd_pfa_scatter(p0, x, j0, inc2, n);
  sp_simd_pfa_scatter(t1, x, j1, inc2, n);
  sp_simd_pfa_scatter(t2, x, j2, inc2, n);
}
#endif

static void
ntt3_pfa_run(spv_t x, spv_size_t cofactor,
	  sp_t p, spv_t ntt_const)
{
  spv_size_t i = 0;
  spv_size_t incstart = 0;
  spv_size_t n = 3 * cofactor;
  spv_size_t inc = cofactor;
  spv_size_t inc2 = 3;

#ifdef HAVE_SSE2
  spv_size_t num_simd = SP_SIMD_VSIZE * (cofactor / SP_SIMD_VSIZE);

  for (i = 0; i < num_simd; i += SP_SIMD_VSIZE)
    {
      ntt3_pfa_run_core_simd(x, incstart, inc, inc2, n, p, ntt_const);
      incstart += SP_SIMD_VSIZE * inc2;
    }
#endif

  for (; i < cofactor; i++, incstart += inc2)
    ntt3_pfa_run_core(x, incstart, inc, n, p, ntt_const);

}

const nttconfig_t ntt3_config = 
{
  3,
  NC,
  ntt3_get_fixed_ntt_const,
  ntt3_init,
  ntt3_run,
  ntt3_pfa_run,
  ntt3_twiddle_run
};

