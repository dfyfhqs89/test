#include "ntt-impl.h"

#define NC 6

static uint32_t 
ntt5_get_num_const(void)
{
  return NC;
}

void
ntt5_init(spv_t out, sp_t p, sp_t d,
	  sp_t primroot, sp_t order)
{
  sp_t w1, w2, w3, w4;
  sp_t h1, h2, h3, h4, h5;
  sp_t t1, t2, t3, t4;
  sp_t inv4 = sp_inv(4, p, d);
    
  h1 = sp_pow(primroot, order / 5, p, d);
  h2 = sp_mul(h1, h1, p, d);
  h3 = sp_mul(h2, h1, p, d);
  h4 = sp_mul(h3, h1, p, d);

  w1 = h3;
  w2 = h4;
  w3 = h2;
  w4 = h1;

  t1 = sp_add(w1, w2, p);
  t1 = sp_add(t1, w3, p);
  t1 = sp_add(t1, w4, p);

  t2 = sp_sub(w1, w2, p);
  t2 = sp_add(t2, w3, p);
  t2 = sp_sub(t2, w4, p);

  t3 = sp_add(w1, w1, p);
  t3 = sp_sub(t3, w3, p);
  t3 = sp_sub(t3, w3, p);

  t4 = sp_add(w2, w2, p);
  t4 = sp_sub(t4, w4, p);
  t4 = sp_sub(t4, w4, p);

  h1 = t1;
  h2 = t2;
  h3 = sp_sub(t3, t4, p);
  h4 = sp_neg(sp_add(t3, t4, p), p);
  h5 = t4;

  out[0] = 1;
  out[1] = sp_sub(sp_mul(h1, inv4, p, d), 1, p);
  out[2] = sp_mul(h2, inv4, p, d);
  out[3] = sp_mul(h3, inv4, p, d);
  out[4] = sp_mul(h4, inv4, p, d);
  out[5] = sp_mul(h5, inv4, p, d);
}

static void
ntt5_run(spv_t x, spv_size_t stride,
	  sp_t p, spv_t ntt_const)
{
  sp_t p0, p1, p2, p3, p4, p5;
  sp_t x0, x1, x2, x3, x4;
  sp_t     t1, t2, t3, t4;

  x0 = x[0 * stride];
  x1 = x[1 * stride];
  x4 = x[2 * stride];
  x2 = x[3 * stride];
  x3 = x[4 * stride];

  t1 = sp_add(x1, x3, p);
  t3 = sp_sub(x1, x3, p);
  t2 = sp_add(x2, x4, p);
  t4 = sp_sub(x2, x4, p);

  p1 = sp_add(t1, t2, p);
  p2 = sp_sub(t1, t2, p);
  p3 = t3;
  p4 = t4;
  p5 = sp_add(t3, t4, p);

  p0 = sp_add(x0, p1, p);

  p1 = sp_ntt_mul(p1, ntt_const[1], ntt_const[NC+1], p);
  p2 = sp_ntt_mul(p2, ntt_const[2], ntt_const[NC+2], p);
  p3 = sp_ntt_mul(p3, ntt_const[3], ntt_const[NC+3], p);
  p4 = sp_ntt_mul(p4, ntt_const[4], ntt_const[NC+4], p);
  p5 = sp_ntt_mul(p5, ntt_const[5], ntt_const[NC+5], p);

  p1 = sp_add(p0, p1, p);

  t1 = sp_add(p1, p2, p);
  t2 = sp_sub(p1, p2, p);
  t3 = sp_add(p3, p5, p);
  t4 = sp_add(p4, p5, p);

  p1 = sp_add(t1, t3, p);
  p2 = sp_add(t2, t4, p);
  p3 = sp_sub(t1, t3, p);
  p4 = sp_sub(t2, t4, p);

  x[0 * stride] = p0;
  x[1 * stride] = p4;
  x[2 * stride] = p3;
  x[3 * stride] = p1;
  x[4 * stride] = p2;
}

#ifdef HAVE_SSE2
static void
ntt5_run_simd(spv_t x, spv_size_t stride,
	  sp_t p, spv_t ntt_const)
{
  sp_simd_t p0, p1, p2, p3, p4, p5;
  sp_simd_t x0, x1, x2, x3, x4;
  sp_simd_t     t1, t2, t3, t4;

  x0 = sp_simd_gather(x + 0 * stride);
  x1 = sp_simd_gather(x + 1 * stride);
  x4 = sp_simd_gather(x + 2 * stride);
  x2 = sp_simd_gather(x + 3 * stride);
  x3 = sp_simd_gather(x + 4 * stride);

  t1 = sp_simd_add(x1, x3, p);
  t3 = sp_simd_sub(x1, x3, p);
  t2 = sp_simd_add(x2, x4, p);
  t4 = sp_simd_sub(x2, x4, p);

  p1 = sp_simd_add(t1, t2, p);
  p2 = sp_simd_sub(t1, t2, p);
  p3 = t3;
  p4 = t4;
  p5 = sp_simd_add(t3, t4, p);

  p0 = sp_simd_add(x0, p1, p);

  p1 = sp_simd_ntt_mul(p1, ntt_const[1], ntt_const[NC+1], p);
  p2 = sp_simd_ntt_mul(p2, ntt_const[2], ntt_const[NC+2], p);
  p3 = sp_simd_ntt_mul(p3, ntt_const[3], ntt_const[NC+3], p);
  p4 = sp_simd_ntt_mul(p4, ntt_const[4], ntt_const[NC+4], p);
  p5 = sp_simd_ntt_mul(p5, ntt_const[5], ntt_const[NC+5], p);

  p1 = sp_simd_add(p0, p1, p);

  t1 = sp_simd_add(p1, p2, p);
  t2 = sp_simd_sub(p1, p2, p);
  t3 = sp_simd_add(p3, p5, p);
  t4 = sp_simd_add(p4, p5, p);

  p1 = sp_simd_add(t1, t3, p);
  p2 = sp_simd_add(t2, t4, p);
  p3 = sp_simd_sub(t1, t3, p);
  p4 = sp_simd_sub(t2, t4, p);

  sp_simd_scatter(p0, x + 0 * stride);
  sp_simd_scatter(p4, x + 1 * stride);
  sp_simd_scatter(p3, x + 2 * stride);
  sp_simd_scatter(p1, x + 3 * stride);
  sp_simd_scatter(p2, x + 4 * stride);
}
#endif


static void
ntt5_twiddle_run(spv_t x, spv_size_t stride,
	  spv_size_t num_transforms,
	  sp_t p, spv_t ntt_const)
{
  spv_size_t i = 0;

#ifdef HAVE_SSE2
  spv_size_t num_simd = SP_SIMD_VSIZE * (num_transforms / SP_SIMD_VSIZE);

  for (i = 0; i < num_simd; i += SP_SIMD_VSIZE)
      ntt5_run_simd(x + i, stride, p, ntt_const);
#endif

  for (; i < num_transforms; i++)
    ntt5_run(x + i, stride, p, ntt_const);
}

static void
ntt5_pfa_run_core(spv_t x, spv_size_t start,
	  spv_size_t inc, spv_size_t n,
	  sp_t p, spv_t ntt_const)
{
  spv_size_t j0, j1, j2, j3, j4;
  sp_t p0, p1, p2, p3, p4, p5;
  sp_t x0, x1, x2, x3, x4;
  sp_t     t1, t2, t3, t4;

  j0 = start;
  j1 = sp_array_inc(j0, inc, n);
  j2 = sp_array_inc(j0, 2 * inc, n);
  j3 = sp_array_inc(j0, 3 * inc, n);
  j4 = sp_array_inc(j0, 4 * inc, n);

  x0 = x[j0];
  x1 = x[j1];
  x4 = x[j2];
  x2 = x[j3];
  x3 = x[j4];

  t1 = sp_add(x1, x3, p);
  t3 = sp_sub(x1, x3, p);
  t2 = sp_add(x2, x4, p);
  t4 = sp_sub(x2, x4, p);

  p1 = sp_add(t1, t2, p);
  p2 = sp_sub(t1, t2, p);
  p3 = t3;
  p4 = t4;
  p5 = sp_add(t3, t4, p);

  p0 = sp_add(x0, p1, p);

  p1 = sp_ntt_mul(p1, ntt_const[1], ntt_const[NC+1], p);
  p2 = sp_ntt_mul(p2, ntt_const[2], ntt_const[NC+2], p);
  p3 = sp_ntt_mul(p3, ntt_const[3], ntt_const[NC+3], p);
  p4 = sp_ntt_mul(p4, ntt_const[4], ntt_const[NC+4], p);
  p5 = sp_ntt_mul(p5, ntt_const[5], ntt_const[NC+5], p);

  p1 = sp_add(p0, p1, p);

  t1 = sp_add(p1, p2, p);
  t2 = sp_sub(p1, p2, p);
  t3 = sp_add(p3, p5, p);
  t4 = sp_add(p4, p5, p);

  p1 = sp_add(t1, t3, p);
  p2 = sp_add(t2, t4, p);
  p3 = sp_sub(t1, t3, p);
  p4 = sp_sub(t2, t4, p);

  x[j0] = p0;
  x[j1] = p4;
  x[j2] = p3;
  x[j3] = p1;
  x[j4] = p2;
}

#ifdef HAVE_SSE2
static void
ntt5_pfa_run_core_simd(spv_t x, spv_size_t start,
	  spv_size_t inc, spv_size_t inc2, spv_size_t n,
	  sp_t p, spv_t ntt_const)
{
  spv_size_t j0, j1, j2, j3, j4;
  sp_simd_t p0, p1, p2, p3, p4, p5;
  sp_simd_t x0, x1, x2, x3, x4;
  sp_simd_t     t1, t2, t3, t4;

  j0 = start;
  j1 = sp_array_inc(j0, inc, n);
  j2 = sp_array_inc(j0, 2 * inc, n);
  j3 = sp_array_inc(j0, 3 * inc, n);
  j4 = sp_array_inc(j0, 4 * inc, n);

  x0 = sp_simd_pfa_gather(x, j0, inc2, n);
  x1 = sp_simd_pfa_gather(x, j1, inc2, n);
  x4 = sp_simd_pfa_gather(x, j2, inc2, n);
  x2 = sp_simd_pfa_gather(x, j3, inc2, n);
  x3 = sp_simd_pfa_gather(x, j4, inc2, n);

  t1 = sp_simd_add(x1, x3, p);
  t3 = sp_simd_sub(x1, x3, p);
  t2 = sp_simd_add(x2, x4, p);
  t4 = sp_simd_sub(x2, x4, p);

  p1 = sp_simd_add(t1, t2, p);
  p2 = sp_simd_sub(t1, t2, p);
  p3 = t3;
  p4 = t4;
  p5 = sp_simd_add(t3, t4, p);

  p0 = sp_simd_add(x0, p1, p);

  p1 = sp_simd_ntt_mul(p1, ntt_const[1], ntt_const[NC+1], p);
  p2 = sp_simd_ntt_mul(p2, ntt_const[2], ntt_const[NC+2], p);
  p3 = sp_simd_ntt_mul(p3, ntt_const[3], ntt_const[NC+3], p);
  p4 = sp_simd_ntt_mul(p4, ntt_const[4], ntt_const[NC+4], p);
  p5 = sp_simd_ntt_mul(p5, ntt_const[5], ntt_const[NC+5], p);

  p1 = sp_simd_add(p0, p1, p);

  t1 = sp_simd_add(p1, p2, p);
  t2 = sp_simd_sub(p1, p2, p);
  t3 = sp_simd_add(p3, p5, p);
  t4 = sp_simd_add(p4, p5, p);

  p1 = sp_simd_add(t1, t3, p);
  p2 = sp_simd_add(t2, t4, p);
  p3 = sp_simd_sub(t1, t3, p);
  p4 = sp_simd_sub(t2, t4, p);

  sp_simd_pfa_scatter(p0, x, j0, inc2, n);
  sp_simd_pfa_scatter(p4, x, j1, inc2, n);
  sp_simd_pfa_scatter(p3, x, j2, inc2, n);
  sp_simd_pfa_scatter(p1, x, j3, inc2, n);
  sp_simd_pfa_scatter(p2, x, j4, inc2, n);
}
#endif

static void
ntt5_pfa_run(spv_t x, spv_size_t stride,
	  spv_size_t cofactor,
	  sp_t p, spv_t ntt_const)
{
  spv_size_t i = 0;
  spv_size_t incstart = 0;
  spv_size_t n = 5 * cofactor * stride;
  spv_size_t inc = cofactor * stride;
  spv_size_t inc2 = 5 * stride;

#ifdef HAVE_SSE2
  spv_size_t num_simd = SP_SIMD_VSIZE * (cofactor / SP_SIMD_VSIZE);

  for (i = 0; i < num_simd; i += SP_SIMD_VSIZE)
    {
      ntt5_pfa_run_core_simd(x, incstart, inc, inc2, n, p, ntt_const);
      incstart += SP_SIMD_VSIZE * inc2;
    }
#endif

  for (; i < cofactor; i++, incstart += inc2)
    ntt5_pfa_run_core(x, incstart, inc, n, p, ntt_const);
}

const nttconfig_t ntt5_config = 
{
  5,
  ntt5_get_num_const,
  ntt5_init,
  ntt5_run,
  ntt5_pfa_run,
  ntt5_twiddle_run
};

