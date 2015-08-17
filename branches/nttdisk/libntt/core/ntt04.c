#include "ntt/ntt-impl.h"

#define NC 4

static const uint8_t fixed_const[NC] = {1, 1, 1, 0};

static const uint8_t *
ntt4_get_fixed_ntt_const(void)
{
  return fixed_const;
}

void
X(ntt4_init)(spv_t out, sp_t p, sp_t d, 
	  sp_t primroot, sp_t order, sp_t perm)
{
  out[0] = 1;
  out[1] = 1;
  out[2] = 1;
  out[3] = sp_pow(primroot, order / 4, p, d);
}

static void 
ntt4_run_core(spv_t in, spv_size_t istride,
		spv_t out, spv_size_t ostride,
		sp_t p, spv_t ntt_const)
{
  sp_t x0, x1, x2, x3;
  sp_t t0, t1, t2, t3;
  sp_t p0, p1, p2, p3;

  x0 = in[0 * istride];
  x1 = in[1 * istride];
  x2 = in[2 * istride];
  x3 = in[3 * istride];

  t0 = sp_ntt_add(x0, x2, p);
  t2 = sp_ntt_sub(x0, x2, p);
  t1 = sp_ntt_add(x1, x3, p);
  t3 = sp_ntt_sub_partial(x1, x3, p);

  t3 = sp_ntt_mul(t3, ntt_const[3], ntt_const[NC+3], p);

  p0 = sp_ntt_add(t0, t1, p);
  p1 = sp_ntt_sub(t0, t1, p);
  p2 = sp_ntt_add(t2, t3, p);
  p3 = sp_ntt_sub(t2, t3, p);

  out[0 * ostride] = p0;
  out[1 * ostride] = p2;
  out[2 * ostride] = p1;
  out[3 * ostride] = p3;
}

#ifdef HAVE_SIMD
static void
ntt4_run_core_simd(spv_t in, spv_size_t istride, spv_size_t idist,
		spv_t out, spv_size_t ostride, spv_size_t odist,
		sp_t p, spv_t ntt_const, spv_size_t vsize)
{
  sp_simd_t x0, x1, x2, x3;
  sp_simd_t t0, t1, t2, t3;
  sp_simd_t p0, p1, p2, p3;

  x0 = sp_simd_gather(in + 0 * istride, idist, vsize);
  x1 = sp_simd_gather(in + 1 * istride, idist, vsize);
  x2 = sp_simd_gather(in + 2 * istride, idist, vsize);
  x3 = sp_simd_gather(in + 3 * istride, idist, vsize);

  t0 = sp_ntt_add_simd(x0, x2, p);
  t2 = sp_ntt_sub_simd(x0, x2, p);
  t1 = sp_ntt_add_simd(x1, x3, p);
  t3 = sp_ntt_sub_partial_simd(x1, x3, p);

  t3 = sp_ntt_mul_simd(t3, ntt_const[3], ntt_const[NC+3], p);

  p0 = sp_ntt_add_simd(t0, t1, p);
  p1 = sp_ntt_sub_simd(t0, t1, p);
  p2 = sp_ntt_add_simd(t2, t3, p);
  p3 = sp_ntt_sub_simd(t2, t3, p);

  sp_simd_scatter(p0, out + 0 * ostride, odist, vsize);
  sp_simd_scatter(p2, out + 1 * ostride, odist, vsize);
  sp_simd_scatter(p1, out + 2 * ostride, odist, vsize);
  sp_simd_scatter(p3, out + 3 * ostride, odist, vsize);
}
#endif

static void
ntt4_run(spv_t in, spv_size_t istride, spv_size_t idist,
    		spv_t out, spv_size_t ostride, spv_size_t odist,
    		spv_size_t num_transforms, sp_t p, spv_t ntt_const)
{
  spv_size_t i = 0;

  for (; i < num_transforms; i++)
    ntt4_run_core(in + i * idist, istride, 
                out + i * odist, ostride, p, ntt_const);
}


#ifdef HAVE_SIMD
static void
ntt4_run_simd(spv_t in, spv_size_t istride, spv_size_t idist,
    		spv_t out, spv_size_t ostride, spv_size_t odist,
    		spv_size_t num_transforms, sp_t p, spv_t ntt_const)
{
  spv_size_t i = 0;
  spv_size_t num_simd = SP_SIMD_VSIZE * (num_transforms / SP_SIMD_VSIZE);

  for (; i < num_simd; i += SP_SIMD_VSIZE)
    ntt4_run_core_simd(in + i * idist, istride, idist, 
                        out + i * odist, ostride, odist, p, 
			ntt_const, SP_SIMD_VSIZE);

  if (i < num_transforms)
    ntt4_run_core_simd(in + i * idist, istride, idist, 
                        out + i * odist, ostride, odist, p, 
			ntt_const, num_transforms - i);
}
#endif


static void
ntt4_twiddle_run_core(spv_t in, spv_size_t istride,
		spv_t out, spv_size_t ostride,
		spv_t w, sp_t p, spv_t ntt_const)
{
  sp_t x0, x1, x2, x3;
  sp_t t0, t1, t2, t3;
  sp_t p0, p1, p2, p3;

  x0 = in[0 * istride];
  x1 = in[1 * istride];
  x2 = in[2 * istride];
  x3 = in[3 * istride];

  t0 = sp_ntt_add(x0, x2, p);
  t2 = sp_ntt_sub(x0, x2, p);
  t1 = sp_ntt_add(x1, x3, p);
  t3 = sp_ntt_sub_partial(x1, x3, p);

  t3 = sp_ntt_mul(t3, ntt_const[3], ntt_const[NC+3], p);

  p0 = sp_ntt_add(t0, t1, p);
  p1 = sp_ntt_sub_partial(t0, t1, p);
  p2 = sp_ntt_add_partial(t2, t3, p);
  p3 = sp_ntt_sub_partial(t2, t3, p);

  p2 = sp_ntt_mul(p2, w[0], w[1], p);
  p1 = sp_ntt_mul(p1, w[2], w[3], p);
  p3 = sp_ntt_mul(p3, w[4], w[5], p);

  out[0 * ostride] = p0;
  out[1 * ostride] = p2;
  out[2 * ostride] = p1;
  out[3 * ostride] = p3;
}


#ifdef HAVE_SIMD
static void
ntt4_twiddle_run_core_simd(
        spv_t in, spv_size_t istride, spv_size_t idist,
		spv_t out, spv_size_t ostride, spv_size_t odist,
		sp_simd_t *w, sp_t p, spv_t ntt_const, spv_size_t vsize)
{
  sp_simd_t x0, x1, x2, x3;
  sp_simd_t t0, t1, t2, t3;
  sp_simd_t p0, p1, p2, p3;

  x0 = sp_simd_gather(in + 0 * istride, idist, vsize);
  x1 = sp_simd_gather(in + 1 * istride, idist, vsize);
  x2 = sp_simd_gather(in + 2 * istride, idist, vsize);
  x3 = sp_simd_gather(in + 3 * istride, idist, vsize);

  t0 = sp_ntt_add_simd(x0, x2, p);
  t2 = sp_ntt_sub_simd(x0, x2, p);
  t1 = sp_ntt_add_simd(x1, x3, p);
  t3 = sp_ntt_sub_partial_simd(x1, x3, p);

  t3 = sp_ntt_mul_simd(t3, ntt_const[3], ntt_const[NC+3], p);

  p0 = sp_ntt_add_simd(t0, t1, p);
  p1 = sp_ntt_sub_partial_simd(t0, t1, p);
  p2 = sp_ntt_add_partial_simd(t2, t3, p);
  p3 = sp_ntt_sub_partial_simd(t2, t3, p);

  p2 = sp_ntt_twiddle_mul_simd(p2, w + 0, p);
  p1 = sp_ntt_twiddle_mul_simd(p1, w + 2, p);
  p3 = sp_ntt_twiddle_mul_simd(p3, w + 4, p);

  sp_simd_scatter(p0, out + 0 * ostride, odist, vsize);
  sp_simd_scatter(p2, out + 1 * ostride, odist, vsize);
  sp_simd_scatter(p1, out + 2 * ostride, odist, vsize);
  sp_simd_scatter(p3, out + 3 * ostride, odist, vsize);
}
#endif

static void
ntt4_twiddle_run(spv_t in, spv_size_t istride, spv_size_t idist,
    			spv_t out, spv_size_t ostride, spv_size_t odist,
    			spv_t w, spv_size_t num_transforms, sp_t p, spv_t ntt_const)
{
  spv_size_t i = 0, j = 0;

  for (; i < num_transforms; i++, j += 2*(4-1))
    ntt4_twiddle_run_core(in + i * idist, istride, 
			out + i * odist, ostride,
			w + j, p, ntt_const);
}


#ifdef HAVE_SIMD
static void
ntt4_twiddle_run_simd(spv_t in, spv_size_t istride, spv_size_t idist,
    			spv_t out, spv_size_t ostride, spv_size_t odist,
    			spv_t w, spv_size_t num_transforms, sp_t p, spv_t ntt_const)
{
  spv_size_t i = 0, j = 0;
  spv_size_t num_simd = SP_SIMD_VSIZE * (num_transforms / SP_SIMD_VSIZE);

  for (; i < num_simd; i += SP_SIMD_VSIZE,
		  	j += 2*(4-1)*SP_SIMD_VSIZE)
    ntt4_twiddle_run_core_simd(
		in + i * idist, istride, idist,
		out + i * odist, ostride, odist,
		(sp_simd_t *)(w + j), p, 
		ntt_const, SP_SIMD_VSIZE);

  if (i < num_transforms)
    ntt4_twiddle_run_core_simd(
		in + i * idist, istride, idist,
		out + i * odist, ostride, odist,
		(sp_simd_t *)(w + j), p, 
		ntt_const, num_transforms - i);
}
#endif

static void
ntt4_pfa_run_core(spv_t x, spv_size_t start,
	  spv_size_t inc, spv_size_t n,
	  sp_t p, spv_t ntt_const)
{
  spv_size_t j0, j1, j2, j3;

  sp_t x0, x1, x2, x3;
  sp_t t0, t1, t2, t3;
  sp_t p0, p1, p2, p3;

  j0 = start;
  j1 = sp_array_inc(j0, inc, n);
  j2 = sp_array_inc(j0, 2 * inc, n);
  j3 = sp_array_inc(j0, 3 * inc, n);

  x0 = x[j0];
  x1 = x[j1];
  x2 = x[j2];
  x3 = x[j3];

  t0 = sp_ntt_add(x0, x2, p);
  t2 = sp_ntt_sub(x0, x2, p);
  t1 = sp_ntt_add(x1, x3, p);
  t3 = sp_ntt_sub_partial(x1, x3, p);

  t3 = sp_ntt_mul(t3, ntt_const[3], ntt_const[NC+3], p);

  p0 = sp_ntt_add(t0, t1, p);
  p1 = sp_ntt_sub(t0, t1, p);
  p2 = sp_ntt_add(t2, t3, p);
  p3 = sp_ntt_sub(t2, t3, p);

  x[j0] = p0;
  x[j1] = p2;
  x[j2] = p1;
  x[j3] = p3;
}

#ifdef HAVE_SIMD
static void
ntt4_pfa_run_core_simd(spv_t x, spv_size_t start,
	  spv_size_t inc, spv_size_t inc2, spv_size_t n,
	  sp_t p, spv_t ntt_const, spv_size_t vsize)
{
  spv_size_t j0, j1, j2, j3;
  sp_simd_t x0, x1, x2, x3;
  sp_simd_t t0, t1, t2, t3;
  sp_simd_t p0, p1, p2, p3;

  j0 = start;
  j1 = sp_array_inc(j0, inc, n);
  j2 = sp_array_inc(j0, 2 * inc, n);
  j3 = sp_array_inc(j0, 3 * inc, n);

  x0 = sp_simd_pfa_gather(x, j0, inc2, n, vsize);
  x1 = sp_simd_pfa_gather(x, j1, inc2, n, vsize);
  x2 = sp_simd_pfa_gather(x, j2, inc2, n, vsize);
  x3 = sp_simd_pfa_gather(x, j3, inc2, n, vsize);

  t0 = sp_ntt_add_simd(x0, x2, p);
  t2 = sp_ntt_sub_simd(x0, x2, p);
  t1 = sp_ntt_add_simd(x1, x3, p);
  t3 = sp_ntt_sub_partial_simd(x1, x3, p);

  t3 = sp_ntt_mul_simd(t3, ntt_const[3], ntt_const[NC+3], p);

  p0 = sp_ntt_add_simd(t0, t1, p);
  p1 = sp_ntt_sub_simd(t0, t1, p);
  p2 = sp_ntt_add_simd(t2, t3, p);
  p3 = sp_ntt_sub_simd(t2, t3, p);

  sp_simd_pfa_scatter(p0, x, j0, inc2, n, vsize);
  sp_simd_pfa_scatter(p2, x, j1, inc2, n, vsize);
  sp_simd_pfa_scatter(p1, x, j2, inc2, n, vsize);
  sp_simd_pfa_scatter(p3, x, j3, inc2, n, vsize);
}
#endif

static void
ntt4_pfa_run(spv_t x, spv_size_t cofactor,
	  sp_t p, spv_t ntt_const)
{
  spv_size_t i = 0;
  spv_size_t incstart = 0;
  spv_size_t n = 4 * cofactor;
  spv_size_t inc = cofactor;
  spv_size_t inc2 = 4;

  for (; i < cofactor; i++, incstart += inc2)
    ntt4_pfa_run_core(x, incstart, inc, n, p, ntt_const);
}


#ifdef HAVE_SIMD
static void
ntt4_pfa_run_simd(spv_t x, spv_size_t cofactor,
	  sp_t p, spv_t ntt_const)
{
  spv_size_t i = 0;
  spv_size_t incstart = 0;
  spv_size_t n = 4 * cofactor;
  spv_size_t inc = cofactor;
  spv_size_t inc2 = 4;
  spv_size_t num_simd = SP_SIMD_VSIZE * (cofactor / SP_SIMD_VSIZE);

  for (i = 0; i < num_simd; i += SP_SIMD_VSIZE)
    {
      ntt4_pfa_run_core_simd(x, incstart, inc, inc2, n, p, 
	  			ntt_const, SP_SIMD_VSIZE);
      incstart += SP_SIMD_VSIZE * inc2;
    }

  if (i < cofactor)
    ntt4_pfa_run_core_simd(x, incstart, inc, inc2, n, p, 
	  			ntt_const, cofactor - i);
}
#endif

const nttconfig_t X(ntt4_config) = 
{
  4,
  NC,
  ntt4_get_fixed_ntt_const,
  X(ntt4_init),
#ifdef HAVE_SIMD
  ntt4_run_simd,
  ntt4_pfa_run_simd,
  ntt4_twiddle_run_simd,
#endif
  ntt4_run,
  ntt4_pfa_run,
  ntt4_twiddle_run
};

