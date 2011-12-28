#include "utils.h"

int main (int argc, char * argv[]) 
{
	unsigned int i;
#ifdef TEST
	unsigned int j;
#endif
	unsigned int B1;
	unsigned int number_of_curves;
	unsigned int nbfactor=0;

	clock2_t time_gpu;
	int device;

	char usage[]="./gpu_ecm N B1 [-s firstsigma] [-n number_of_curves] [-d device]";

	biguint_t *h_xarray;
	biguint_t *h_zarray;
	biguint_t *h_darray;
	biguint_t h_invmod;
	biguint_t h_N;
 
  mpz_t N;
	mpz_t mpz_max; //2^(32*SIZE_NUMBER)
	mpz_t mpz_invmod; //N^-1 mod (2^(32*SIZE_NUMBER))
	mpz_t mpz_Rinv; // 2^-(32*SIZE_NUMBER) mod N
  mpz_t sigma;
  mpz_t mpz_B1;
  mpz_t mpz_d;
  mpz_t xp;
  mpz_t zp;
  mpz_t xfin;
  mpz_t zfin;
  mpz_t u;
  mpz_t v;

	gmp_randstate_t state;
	gmp_randinit_default(state);
	unsigned long seed=0;

  mpz_init (N);
  mpz_init (mpz_max);
  mpz_init (sigma);
  mpz_init (mpz_B1);
  mpz_init (mpz_d);
  mpz_init (xp);
  mpz_init (zp);
  mpz_init (xfin);
  mpz_init (zfin);
  mpz_init (u);
  mpz_init (v);
  mpz_init (mpz_invmod);
  mpz_init (mpz_Rinv);

  if (argc < 3)
  {  
		printf ("Error in call function : not enough arguments.\n%s\n",usage);
		exit(1);
	}
	
  mpz_set_str (N, argv[1], 10); // in base 10 
  mpz_set_str (mpz_B1, argv[2], 10);

	argc-=2;
	argv+=2;

	//default values
	number_of_curves=0;
	device=-1;
	mpz_set_ui(sigma,0);
	
	while (argc > 2 && argv[1][0]=='-')
	{
		if (argv[1][1] == 's')
		{
			mpz_set_str(sigma, argv[2], 10);
			argc-=2;
			argv+=2;
		}
		else if (argv[1][1] == 'n')
		{
			sscanf(argv[2],"%u",&number_of_curves);
			argc-=2;
			argv+=2;
		}
		else if (argv[1][1] == 'd')
		{
			sscanf(argv[2],"%d",&device);
			argc-=2;
			argv+=2;
		}
	}
	
	if (argc!=1)
	{
			printf ("Error in call function : wrong number of arguments.\n%s\n",usage);
			exit(1);
	}


	//init data according to the arguments
	if (mpz_cmp_ui(sigma,0)==0)
	{
		seed=time(NULL);
		gmp_randseed_ui(state,seed);
  	mpz_urandomb(sigma,state,3);//between 0 and 2^3
		mpz_add_ui(sigma,sigma,6);//add 6
	}
	gmp_randclear (state);
	
  // check N is odd 
  if (mpz_divisible_ui_p (N, 2))
  {
  	fprintf (stderr, "Error, N should be odd\n");
   	exit (1);
  }

	mpz_ui_pow_ui(mpz_max,2,32*SIZE_NUMBER-4);	
	if (mpz_cmp(N,mpz_max) >=0)
  {
  	fprintf (stderr, "Error, N should be stricly lower than 2^%d\n",32*SIZE_NUMBER-4);
   	exit (1);
  }
	mpz_ui_pow_ui(mpz_max,2,32*SIZE_NUMBER);	

	if (mpz_cmp_ui(mpz_B1,TWO32) >=0)
  {
  	fprintf (stderr, "Error, B1 should be stricly lower than 2^%d\n",32);
   	exit (1);
  }
	B1=(unsigned int)mpz_get_ui(mpz_B1);

	//int deviceCount;
	//int bestDevice=-1,bestMajor=-1,bestMinor=-1;
	cudaDeviceProp deviceProp;
	//cudaGetDeviceCount(&deviceCount);
				
	printf("#Compiled for a NVIDIA GPU with compute capability %d.%d.\n",MAJOR,MINOR);
	if (device!=-1)
	{
		printf("#Device %d is required.\n",device);
		cudaError_t err= cudaSetDevice(device);
		if (err != cudaSuccess)
		{
			printf("#Error : Could not use device %d\n",device);
			exit(1);
		}
	}
	
	cudaGetDevice(&device);
	cudaGetDeviceProperties(&deviceProp,device);
	if (deviceProp.major < MAJOR || (deviceProp.major== MAJOR && deviceProp.minor< MINOR))
	{
		printf("#Error : Device %d have a compute capability of %d.%d (required %d.%d).\n",device,deviceProp.major,deviceProp.minor,MAJOR,MINOR);
		exit(1);
	}
	else if (deviceProp.major==MAJOR && deviceProp.minor==MINOR)
		printf("#Will use device %d : %s, compute capability %d.%d, %d MPs.\n",device,deviceProp.name,deviceProp.major,deviceProp.minor,deviceProp.multiProcessorCount);
	else
		printf("#Will use device %d : %s, compute capability %d.%d (you should compile the program for this compute capability to be more efficient), %d MPs.\n",device,deviceProp.name,deviceProp.major,deviceProp.minor,deviceProp.multiProcessorCount);


	if (number_of_curves==0)
		number_of_curves=deviceProp.multiProcessorCount*(deviceProp.sharedMemPerBlock/MAX_USE_SHARED_MEM);

	gmp_printf ("#gpu_ecm launched with :\nN=%Zd\nB1=%u\ncurves=%u\nfirstsigma=%Zd\n",N,B1,number_of_curves,sigma);
	if (seed!=0)
		printf("#used seed %lu to generate sigma\n",seed);

	h_xarray=(biguint_t *) malloc(number_of_curves*sizeof(biguint_t));
	h_zarray=(biguint_t *) malloc(number_of_curves*sizeof(biguint_t));
	h_darray=(biguint_t *) malloc(number_of_curves*sizeof(biguint_t));


	//Some precomputation
	//Compute N^-1 mod 2^(32*SIZE_NUMBER)
	mpz_invert(mpz_invmod,N,mpz_max);
	mpz_sub(mpz_invmod,mpz_max,mpz_invmod);
	//Compute  2^-(32*SIZE_NUMBER) mod N
	mpz_invert(mpz_Rinv,mpz_max,N);
	
	mpz_to_biguint(h_N,N);	
	mpz_to_biguint(h_invmod,mpz_invmod);	
	
	for(i=0;i<number_of_curves;i++)
	{
		calculParam (sigma, N, mpz_d, xp, zp, u, v);

		//Compute the Montgomery representation of x0, z0 and d
		mpz_mul_2exp(xp,xp,32*SIZE_NUMBER);
		mpz_mod(xp,xp,N);
		mpz_to_biguint(h_xarray[i],xp);	

		mpz_mul_2exp(zp,zp,32*SIZE_NUMBER);
		mpz_mod(zp,zp,N);
		mpz_to_biguint(h_zarray[i],zp);	

		mpz_mul_2exp(mpz_d,mpz_d,32*SIZE_NUMBER);
		mpz_mod(mpz_d,mpz_d,N);
		mpz_to_biguint(h_darray[i],mpz_d);	

		mpz_add_ui(sigma,sigma,1);

	}	

	mpz_sub_ui(sigma,sigma,number_of_curves);

	printf("\n#Begin GPU computation...\n");
	time_gpu=cuda_Main(h_N,h_invmod,h_xarray,h_zarray,h_darray,B1,number_of_curves);
	printf("#All kernels finished, analysing results...\n");

	for(i=0;i<number_of_curves;i++)
	{
		biguint_to_mpz(xfin,h_xarray[i]);	
		mpz_mul(xfin,xfin,mpz_Rinv);
		mpz_mod(xfin,xfin,N);

		biguint_to_mpz(zfin,h_zarray[i]);	
		mpz_mul(zfin,zfin,mpz_Rinv);
		mpz_mod(zfin,zfin,N);

		gmp_printf("#Looking for factors for the curves with sigma=%Zd\n",sigma);
		nbfactor+=findfactor(N,xfin,zfin);

		mpz_add_ui(sigma,sigma,1);
	}

	if (nbfactor==0)
		printf("#Results : No factor found\n");
	else if (nbfactor==1)
		printf("#Results : 1 factor found\n");
	else
		printf("#Results : %u curves find a factor (not necessarily different)\n",nbfactor);
	
	printf("\n#Temps gpu : %.3f init&copy=%.3f computation=%.3f\n",(double)(time_gpu.init+time_gpu.computation)/CLOCKS_PER_SEC,(double)(time_gpu.init)/CLOCKS_PER_SEC,(double)(time_gpu.computation)/CLOCKS_PER_SEC);

	mpz_clear (sigma);
  mpz_clear (N);
  mpz_clear (mpz_max);
  mpz_clear (mpz_invmod);
  mpz_clear (mpz_B1);
  mpz_clear (mpz_d);
  mpz_clear (xp);
  mpz_clear (zp);
  mpz_clear (xfin);
  mpz_clear (zfin);
  mpz_clear (u);
  mpz_clear (v);
  mpz_clear (mpz_Rinv);
  
	
	free(h_xarray);
	free(h_zarray);
	free(h_darray);
	
  return 0;
}



unsigned int findfactor(mpz_t N, mpz_t xfin, mpz_t zfin)
{
	//int probprime;
	int findfactor=0;
	mpz_t gcd;
	mpz_t factor;
	mpz_t temp;

  mpz_init (gcd);
  mpz_init (temp);
  mpz_init (factor);

	mpz_set_ui(factor,0);

	gmp_printf("  xfin=%Zd\n  zfin=%Zd\n",xfin,zfin);
	// tester si pgcd =N et =0
	mpz_gcd(gcd,zfin,N);
	
	if (mpz_cmp_ui(gcd,1)==0)
	{
		mpz_invert(zfin,zfin,N);
		mpz_mul(xfin,xfin,zfin);
		mpz_mod(xfin,xfin,N);
		gmp_printf("  xunif=%Zd\n",xfin);
		mpz_gcd(gcd,xfin,N);
			
		if (mpz_cmp_ui(gcd,1)==0)
		{
			printf("  #No factors found. You shoud try with a bigger B1.\n");
		}
		else if (mpz_cmp(gcd,N))
		{
			printf("  #No factors found. You should try with a smaller B1\n");
		}
		else
		{
			mpz_set(factor,gcd);
			gmp_printf("  #Factor found : %Zd (with x/z)\n",factor);
			findfactor=1;
		}
	}
	else if (mpz_cmp(gcd,N)==0)
	{
		printf("  #No factors found. You should try with a smaller B1\n");
	}
	else //gcd !=1 gcd!=N (and gcd>0 because N>0) so we found a factor
	{
		mpz_set(factor,gcd);
		gmp_printf("  #Factor found : %Zd (with z)\n",factor);
		findfactor=1;
	}
		/*
	if (mpz_cmp_ui(factor,0)!=0)
	{
		mpz_divexact(mpztemp,N,factor);
		//gmp_sprintf (str[i],"%scofactor:=%Zd ",str[i],mpztemp);
		probprime=mpz_probab_prime_p(mpztemp,5);
		if (probprime==2)
			gmp_sprintf (str[i],"%s definitely prime\n",str[i]);
		else if (probprime==1)
			gmp_sprintf (str[i],"%s probably prime\n",str[i]);
		else if (probprime==0)
			gmp_sprintf (str[i],"%s definitely composite\n",str[i]);
		else	
			gmp_sprintf (str[i],"%s \n",str[i]);
	}
 */
 	mpz_clear(temp);
  mpz_clear(gcd);
  mpz_clear(factor);
	
	return findfactor;
}



void biguint_print (biguint_t a)
{
  unsigned int i;

  printf ("%u", a[0]);
  for (i = 1; i < SIZE_NUMBER; i++)
    if (a[i]!=0)
			printf ("+%u*2^%u", a[i], 32*i);
  //printf ("\n");
}

void bigint_print (dbigint_t a)
{
  unsigned int i;

  printf ("%d", a[0]);
  for (i = 1; i < SIZE_NUMBER; i++)
    printf ("+%d*2^%u", a[i], 32*i);
  printf ("\n");
}

void mpz_to_biguint (biguint_t a, mpz_t b)
{
	int i;

	for (i=0;i<SIZE_NUMBER;i++)
	{
		if (i%2 == 0)
			a[i]=(mpz_getlimbn(b,i/2) & 0x00000000ffffffff);
		else
			a[i]=(mpz_getlimbn(b, i/2) >> 32);	
	}
}

void biguint_to_mpz (mpz_t a, biguint_t b)
{
	int i;
	unsigned long temp;
	
	mpz_set_ui(a,0);

	for (i=SIZE_NUMBER-1;i>=0;i--)
	{
		if (i%2 == 0)
			mpz_add_ui(a,a,b[i]);
		else
		{
			temp=(unsigned long)b[i];
			mpz_add_ui(a,a,(temp<<32));
		}
		if (i!=0 && i%2==0)
			mpz_mul_2exp(a,a,64);
	}
}

//calculParam computes the values of parameters of Suyama's parametrisation
//input : a random integer sigma and N
//output : parameters of Suyama's parametrisation -> a, x0, z0, u and v 
void
calculParam(mpz_t sigma, mpz_t N, mpz_t d, mpz_t x0, mpz_t z0, mpz_t u, mpz_t v)
{
	mpz_t a; 
	mpz_t tmp; 
	mpz_t bezout1;
	mpz_t bezout2; 
	mpz_t gcd; 

  mpz_init(a);
  mpz_init(tmp);
  mpz_init(bezout1);
  mpz_init(bezout2);
  mpz_init(gcd);

	// u 
	mpz_pow_ui(u,sigma,2);
	mpz_sub_ui(u,u,5);
	// v 
	mpz_mul_ui(v,sigma,4);
	// x0 
	mpz_powm_ui(x0,u,3,N);
	// z0 
	mpz_powm_ui(z0,v,3,N);
	// a 
	mpz_sub(a,v,u);
	mpz_pow_ui(a,a,3);
	mpz_mul_ui(tmp,u,3);
	mpz_add(tmp,tmp,v);
	mpz_mul(a,a,tmp);
	mpz_mod(a,a,N);
	mpz_pow_ui(tmp,u,3);
	mpz_mul(tmp,tmp,v);
	mpz_mul_ui(tmp,tmp,4);
	mpz_mod(tmp,tmp,N);
 // set gcd to the greatest common divisor of tmp and N, and in addition
 // set bezout1 and bezout2 to coefficients satisfying 
 //tmp*bezout1 + N*bezout2 = gcd -> bezout1 = (1/tmp)%N  
	mpz_gcdext(gcd,bezout1,bezout2,tmp,N);
  
	mpz_mod(tmp,bezout1,N);
	mpz_mul(a,a,tmp);
	mpz_mod(a,a,N);
	mpz_sub_ui(a,a,2);
	mpz_mod(a,a,N);

	//gmp_printf("a=%Zd\n",a);
	
	// d = (a+2)/4 mod N
	mpz_add_ui(d,a,2);
  while (mpz_divisible_ui_p (d, 4) == 0)
   	mpz_add (d, d, N);
	mpz_divexact_ui(d,d,4);
	mpz_mod(d,d,N);
	
	// calculation of the starting point x = (x0/z0)%N 
	mpz_gcdext(gcd,bezout1,bezout2,z0,N);
  // set gcd to the greatest common divisor of z0 and N, and in addition
  // set bezout1 and bezout2 to coefficients satisfying
  // z0*bezout1 + N*bezout2 = gcd -> bezout1=(1/z0)%N 
	mpz_mod(tmp,bezout1,N);
	mpz_mul(tmp,x0,tmp); // x0/z0 
	mpz_mod(tmp,tmp,N);
        
  // x0 <- x0/z0, z0 <- 1 
  mpz_set (x0, tmp);
	mpz_set_ui (z0, 1);

	mpz_clear(a);
	mpz_clear(tmp);
	mpz_clear(bezout1);
	mpz_clear(bezout2);
	mpz_clear(gcd);
}

unsigned long getprime (unsigned long pp)
{
 static unsigned long offset = 0;     // offset for current primes 
 static long current = -1;            // index of previous prime 
 static unsigned *primes = NULL;      // small primes up to sqrt(p) 
 static unsigned long nprimes = 0;    // length of primes[] 
 static unsigned char *sieve = NULL;  // sieving table 
 static long len = 0;                 // length of sieving table 
 static unsigned long *moduli = NULL; // offset for small primes 

 if (pp == 0) // free the tables, and reinitialize 
   {
     offset = 0.0;
     current = -1;
     free (primes);
     primes = NULL;
     nprimes = 0;
     free (sieve);
     sieve = NULL;
     len = 0;
     free (moduli);
     moduli = NULL;
     return pp;
   }

 // the following complex block is equivalent to:
 // while ((++current < len) && (sieve[current] == 0));
 // but is faster.
 
 {
   unsigned char *ptr = sieve + current;
   unsigned char *end = sieve + len;
   while ((++ptr < end) && (*ptr == 0));
   current = ptr - sieve;
 }

 if (current < len) // most calls will end here 
   return offset + 2 * current;

 // otherwise we have to sieve 
 offset += 2 * len;

 // first enlarge sieving table if too small 
 if ((unsigned long) len * len < offset)
   {
     free (sieve);
     len *= 2;
     sieve = (unsigned char *) malloc (len * sizeof (unsigned char));
     // assume this "small" malloc will not fail in normal usage 
     assert(sieve != NULL);
   }

 // now enlarge small prime table if too small 
 if ((nprimes == 0) ||
     (primes[nprimes - 1] * primes[nprimes - 1] < offset + len))
     {
       if (nprimes == 0) // initialization 
         {
           nprimes = 1;
           primes = (unsigned *) malloc (nprimes * sizeof(unsigned long));
           // assume this "small" malloc will not fail in normal usage 
           assert(primes != NULL);
           moduli = (long unsigned int *) malloc (nprimes *
                                                  sizeof(unsigned long));
           // assume this "small" malloc will not fail in normal usage 
           assert(moduli != NULL);
           len = 1;
           sieve = (unsigned char *) malloc(len * sizeof(unsigned char));//len=1 here
           // assume this "small" malloc will not fail in normal usage 
           assert(sieve != NULL);
           offset = 5.0;
           sieve[0] = 1; // corresponding to 5 
           primes[0] = 3;
           moduli[0] = 1; // next odd multiple of 3 is 7, i.e. next to 5 
           current = -1;
           return 3.0;
         }
       else
         {
           unsigned int i, p, j, ok;

           i = nprimes;
           nprimes *= 2;
           primes = (unsigned *) realloc (primes, nprimes *
                                          sizeof(unsigned long));
           moduli = (unsigned long*) realloc (moduli, nprimes *
                                                   sizeof(unsigned long));
           // assume those "small" realloc's will not fail in normal usage 
           assert(primes != NULL && moduli != NULL);
           for (p = primes[i-1]; i < nprimes; i++)
             {
               // find next (odd) prime > p 
               do
                 {
                   for (p += 2, ok = 1, j = 0; (ok != 0) && (j < i); j++)
                     ok = p % primes[j];
                 }
               while (ok == 0);
               primes[i] = p;
               // moduli[i] is the smallest m such that offset + 2*m = k*p
               j = offset % p;
               j = (j == 0) ? j : p - j; // -offset mod p 
               if ((j % 2) != 0)
                 j += p; // ensure j is even 
               moduli[i] = j / 2;
             }
         }
     }

 // now sieve for new primes 
 {
   long i;
   unsigned long j, p;

   for (i = 0; i < len; i++)
     sieve[i] = 1;
   for (j = 0; j < nprimes; j++)
     {
       p = primes[j];
       for (i = moduli[j]; i < len; i += p)
         sieve[i] = 0;
       moduli[j] = i - len; // for next sieving array 
     }
 }

 current = -1;
 while ((++current < len) && (sieve[current] == 0));
 assert(current < len);//otherwise we found a prime gap >= sqrt(x) around x
 return offset + 2 * current;
}

