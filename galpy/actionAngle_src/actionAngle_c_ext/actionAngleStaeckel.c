/*
  C code for Binney (2012)'s Staeckel approximation code
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_roots.h>
#include <gsl/gsl_min.h>
#include <gsl/gsl_integration.h>
//Potentials
#include <galpy_potentials.h>
#include <actionAngle.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
/*
  Structure Declarations
*/
struct JRStaeckelArg{
  double E;
  double Lz22delta;
  double I3U;
  double delta;
  double u0;
  double sinh2u0;
  double v0;
  double sin2v0;
  double potu0v0;
  int nargs;
  struct actionAngleArg * actionAngleArgs;
};
struct JzStaeckelArg{
  double E;
  double Lz22delta;
  double I3V;
  double delta;
  double u0;
  double cosh2u0;
  double sinh2u0;
  double potupi2;
  int nargs;
  struct actionAngleArg * actionAngleArgs;
};
struct u0EqArg{
  double E;
  double Lz22delta;
  double delta;
  int nargs;
  struct actionAngleArg * actionAngleArgs;
};
/*
  Function Declarations
*/
void calcu0(int,double *,double *,int,int *,double *,double,double *,int *);
void actionAngleStaeckel_actions(int,double *,double *,double *,double *,
				 double *,int,int *,double *,double,
				 double *,double *,int *);
void calcJR(int,double *,double *,double *,double *,double *,double *,
	    double,double *,double *,double *,double *,double *,int,
	    struct actionAngleArg *,int);
void calcJz(int,double *,double *,double *,double *,double *,double,
	    double *,double *,double *,double *,int,struct actionAngleArg *,
	    int);
void calcUminUmax(int,double *,double *,double *,double *,double *,double *,
		  double *,double,double *,double *,double *,double *,double *,
		  int,struct actionAngleArg *);
void calcVmin(int,double *,double *,double *,double *,double *,double *,double,
	      double *,double *,double *,double *,int,struct actionAngleArg *);
double JRStaeckelIntegrandSquared(double,void *);
double JRStaeckelIntegrand(double,void *);
double JzStaeckelIntegrandSquared(double,void *);
double JzStaeckelIntegrand(double,void *);
double u0Equation(double,void *);
double evaluatePotentials(double,double,int, struct actionAngleArg *);
double evaluatePotentialsUV(double,double,double,int,struct actionAngleArg *);
/*
  Actual functions, inlines first
*/
inline void parse_actionAngleArgs(int npot,
				  struct actionAngleArg * actionAngleArgs,
				  int * pot_type,
				  double * pot_args){
  int ii,jj;
  for (ii=0; ii < npot; ii++){
    switch ( *pot_type++ ) {
    case 0: //LogarithmicHaloPotential, 2 arguments
      actionAngleArgs->potentialEval= &LogarithmicHaloPotentialEval;
      actionAngleArgs->nargs= 3;
      break;
    case 5: //MiyamotoNagaiPotential, 3 arguments
      actionAngleArgs->potentialEval= &MiyamotoNagaiPotentialEval;
      actionAngleArgs->nargs= 3;
      break;
    case 7: //PowerSphericalPotential, 2 arguments
      actionAngleArgs->potentialEval= &PowerSphericalPotentialEval;
      actionAngleArgs->nargs= 2;
      break;
    case 8: //HernquistPotential, 2 arguments
      actionAngleArgs->potentialEval= &HernquistPotentialEval;
      actionAngleArgs->nargs= 2;
      break;
    case 9: //NFWPotential, 2 arguments
      actionAngleArgs->potentialEval= &NFWPotentialEval;
      actionAngleArgs->nargs= 2;
      break;
    case 10: //JaffePotential, 2 arguments
      actionAngleArgs->potentialEval= &JaffePotentialEval;
      actionAngleArgs->nargs= 2;
      break;
    }
    actionAngleArgs->args= (double *) malloc( actionAngleArgs->nargs * sizeof(double));
    for (jj=0; jj < actionAngleArgs->nargs; jj++){
      *(actionAngleArgs->args)= *pot_args++;
      actionAngleArgs->args++;
    }
    actionAngleArgs->args-= actionAngleArgs->nargs;
    actionAngleArgs++;
  }
  actionAngleArgs-= npot;
}
inline void uv_to_Rz(double u, double v, double * R, double *z,double delta){
  *R= delta * sinh(u) * sin(v);
  *z= delta * cosh(u) * cos(v);
}
inline void Rz_to_uv_vec(int ndata,
			 double *R,
			 double *z,
			 double *u,
			 double *v,
			 double delta){
  int ii;
  double d12, d22, coshu, cosv;
  for (ii=0; ii < ndata; ii++) {
    d12= (*(z+ii)+delta)*(*(z+ii)+delta)+(*(R+ii))*(*(R+ii));
    d22= (*(z+ii)-delta)*(*(z+ii)-delta)+(*(R+ii))*(*(R+ii));
    coshu= 0.5/delta*(sqrt(d12)+sqrt(d22));
    cosv=  0.5/delta*(sqrt(d12)-sqrt(d22));
    *u++= acosh(coshu);
    *v++= acos(cosv);
  }
  u-= ndata;
  v-= ndata;
}
inline void calcEL(int ndata,
		   double *R,
		   double *vR,
		   double *vT,
		   double *z,
		   double *vz,
		   double *E,
		   double *Lz,
		   int nargs,
		   struct actionAngleArg * actionAngleArgs){
  int ii;
  for (ii=0; ii < ndata; ii++){
    *(E+ii)= evaluatePotentials(*(R+ii),*(z+ii),
				nargs,actionAngleArgs)
      + 0.5 * *(vR+ii) * *(vR+ii)
      + 0.5 * *(vT+ii) * *(vT+ii)
      + 0.5 * *(vz+ii) * *(vz+ii);
    *(Lz+ii)= *(R+ii) * *(vT+ii);
  }
}
/*
  MAIN FUNCTIONS
 */
void calcu0(int ndata,
	    double *E,
	    double *Lz,
	    int npot,
	    int * pot_type,
	    double * pot_args,
	    double delta,
	    double *u0,
	    int * err){
  int ii;
  //Set up the potentials
  struct actionAngleArg * actionAngleArgs= (struct actionAngleArg *) malloc ( npot * sizeof (struct actionAngleArg) );
  parse_actionAngleArgs(npot,actionAngleArgs,pot_type,pot_args);
  //setup the function to be minimized
  gsl_function u0Eq;
  struct u0EqArg * params= (struct u0EqArg *) malloc ( sizeof (struct u0EqArg) );
  params->delta= delta;
  params->nargs= npot;
  params->actionAngleArgs= actionAngleArgs;
  //Setup solver
  int status;
  int iter, max_iter = 100;
  const gsl_min_fminimizer_type *T;
  gsl_min_fminimizer *s;
  double u_guess, u_lo, u_hi;
  T = gsl_min_fminimizer_brent;
  s = gsl_min_fminimizer_alloc (T);
  u0Eq.function = &u0Equation;
  for (ii=0; ii < ndata; ii++){
    //Setup function
    params->E= *(E+ii);
    params->Lz22delta= 0.5 * *(Lz+ii) * *(Lz+ii) / delta / delta;
    u0Eq.params = params;
    //Find starting points for minimum
    u_guess= 1.;
    u_lo= 0.001;
    u_hi= 100.;
    status = gsl_min_fminimizer_set (s, &u0Eq, u_guess, u_lo, u_hi);
    iter= 0;
    do
      {
	iter++;
	status = gsl_min_fminimizer_iterate (s);
	u_guess = gsl_min_fminimizer_x_minimum (s);
	u_lo = gsl_min_fminimizer_x_lower (s);
	u_hi = gsl_min_fminimizer_x_upper (s);
	status = gsl_min_test_interval (u_lo, u_hi,
					 9.9999999999999998e-13,
					 4.4408920985006262e-16);
      }
    while (status == GSL_CONTINUE && iter < max_iter);
    *(u0+ii)= gsl_min_fminimizer_x_minimum (s);
  }
  gsl_min_fminimizer_free (s);
  *err= status;
}
void actionAngleStaeckel_actions(int ndata,
				 double *R,
				 double *vR,
				 double *vT,
				 double *z,
				 double *vz,
				 int npot,
				 int * pot_type,
				 double * pot_args,
				 double delta,
				 double *jr,
				 double *jz,
				 int * err){
  int ii;
  //Set up the potentials
  struct actionAngleArg * actionAngleArgs= (struct actionAngleArg *) malloc ( npot * sizeof (struct actionAngleArg) );
  parse_actionAngleArgs(npot,actionAngleArgs,pot_type,pot_args);
  //E,Lz
  double *E= (double *) malloc ( ndata * sizeof(double) );
  double *Lz= (double *) malloc ( ndata * sizeof(double) );
  calcEL(ndata,R,vR,vT,z,vz,E,Lz,npot,actionAngleArgs);
  //Calculate all necessary parameters
  double *ux= (double *) malloc ( ndata * sizeof(double) );
  double *vx= (double *) malloc ( ndata * sizeof(double) );
  Rz_to_uv_vec(ndata,R,z,ux,vx,delta);
  double *coshux= (double *) malloc ( ndata * sizeof(double) );
  double *sinhux= (double *) malloc ( ndata * sizeof(double) );
  double *sinvx= (double *) malloc ( ndata * sizeof(double) );
  double *cosvx= (double *) malloc ( ndata * sizeof(double) );
  double *pux= (double *) malloc ( ndata * sizeof(double) );
  double *pvx= (double *) malloc ( ndata * sizeof(double) );
  double *u0= (double *) malloc ( ndata * sizeof(double) );
  double *sinh2u0= (double *) malloc ( ndata * sizeof(double) );
  double *cosh2u0= (double *) malloc ( ndata * sizeof(double) );
  double *v0= (double *) malloc ( ndata * sizeof(double) );
  double *sin2v0= (double *) malloc ( ndata * sizeof(double) );
  double *potu0v0= (double *) malloc ( ndata * sizeof(double) );
  double *potupi2= (double *) malloc ( ndata * sizeof(double) );
  double *I3U= (double *) malloc ( ndata * sizeof(double) );
  double *I3V= (double *) malloc ( ndata * sizeof(double) );
  for (ii=0; ii < ndata; ii++){
    *(coshux+ii)= cosh(*(ux+ii));
    *(sinhux+ii)= sinh(*(ux+ii));
    *(cosvx+ii)= cos(*(vx+ii));
    *(sinvx+ii)= sin(*(vx+ii));
    *(pux+ii)= delta * (*(vR+ii) * *(coshux+ii) * *(sinvx+ii) 
			+ *(vz+ii) * *(sinhux+ii) * *(cosvx+ii));
    *(pvx+ii)= delta * (*(vR+ii) * *(sinhux+ii) * *(cosvx+ii) 
			- *(vz+ii) * *(coshux+ii) * *(sinvx+ii));
    *(u0+ii)= *(ux+ii);
    *(sinh2u0+ii)= sinh(*(u0+ii)) * sinh(*(u0+ii));
    *(cosh2u0+ii)= cosh(*(u0+ii)) * cosh(*(u0+ii));
    *(v0+ii)= *(vx+ii);
    *(sin2v0+ii)= sin(*(v0+ii)) * sin(*(v0+ii));
    *(potu0v0+ii)= evaluatePotentialsUV(*(u0+ii),*(vx+ii),delta,
					npot,actionAngleArgs);
    *(I3U+ii)= *(E+ii) * *(sinhux+ii) * *(sinhux+ii)
      - 0.5 * *(pux+ii) * *(pux+ii) / delta / delta
      - 0.5 * *(Lz+ii) * *(Lz+ii) / delta / delta / *(sinhux+ii) / *(sinhux+ii);
    *(potupi2+ii)= evaluatePotentialsUV(*(ux+ii),0.5 * M_PI,delta,
					npot,actionAngleArgs);
    *(I3V+ii)= - *(E+ii) * *(sinvx+ii) * *(sinvx+ii)
      + 0.5 * *(pvx+ii) * *(pvx+ii) / delta / delta
      + 0.5 * *(Lz+ii) * *(Lz+ii) / delta / delta / *(sinvx+ii) / *(sinvx+ii)
      - *(coshux+ii) * *(coshux+ii) * *(potupi2+ii)
      + ( *(sinhux+ii) * *(sinhux+ii) + *(sinvx+ii) * *(sinvx+ii))
      * evaluatePotentialsUV(*(ux+ii),*(vx+ii),delta,
			     npot,actionAngleArgs);
  }
  //Calculate 'peri' and 'apo'centers
  double *umin= (double *) malloc ( ndata * sizeof(double) );
  double *umax= (double *) malloc ( ndata * sizeof(double) );
  double *vmin= (double *) malloc ( ndata * sizeof(double) );
  calcUminUmax(ndata,umin,umax,ux,pux,E,Lz,I3U,delta,u0,sinh2u0,v0,sin2v0,
	       potu0v0,npot,actionAngleArgs);
  calcVmin(ndata,vmin,vx,pvx,E,Lz,I3V,delta,u0,cosh2u0,sinh2u0,potupi2,
	   npot,actionAngleArgs);
  //Calculate the actions
  calcJR(ndata,jr,umin,umax,E,Lz,I3U,delta,u0,sinh2u0,v0,sin2v0,potu0v0,
	 npot,actionAngleArgs,10);
  calcJz(ndata,jz,vmin,E,Lz,I3V,delta,u0,cosh2u0,sinh2u0,potupi2,
	 npot,actionAngleArgs,10);
}
void calcJR(int ndata,
	    double * jr,
	    double * umin,
	    double * umax,
	    double * E,
	    double * Lz,
	    double * I3U,
	    double delta,
	    double * u0,
	    double * sinh2u0,
	    double * v0,
	    double * sin2v0,
	    double * potu0v0,
	    int nargs,
	    struct actionAngleArg * actionAngleArgs,
	    int order){
  int ii;
  gsl_function JRInt;
  struct JRStaeckelArg * params= (struct JRStaeckelArg *) malloc ( sizeof (struct JRStaeckelArg) );
  params->delta= delta;
  params->nargs= nargs;
  params->actionAngleArgs= actionAngleArgs;
  //Setup integrator
  gsl_integration_glfixed_table * T= gsl_integration_glfixed_table_alloc (order);
  JRInt.function = &JRStaeckelIntegrand;
  for (ii=0; ii < ndata; ii++){
    if ( (*(umax+ii) - *(umin+ii)) / *(umax+ii) < 0.000001 ){//circular
      *(jr+ii) = 0.;
      continue;
    }
    //Setup function
    params->E= *(E+ii);
    params->Lz22delta= 0.5 * *(Lz+ii) * *(Lz+ii) / delta / delta;
    params->I3U= *(I3U+ii);
    params->u0= *(u0+ii);
    params->sinh2u0= *(sinh2u0+ii);
    params->v0= *(v0+ii);
    params->sin2v0= *(sin2v0+ii);
    params->potu0v0= *(potu0v0+ii);
    JRInt.params = params;
    //Integrate
    *(jr+ii)= gsl_integration_glfixed (&JRInt,*(umin+ii),*(umax+ii),T)
      * sqrt(2.) * delta / M_PI;
  }
  gsl_integration_glfixed_table_free ( T );
}
void calcJz(int ndata,
	    double * jz,
	    double * vmin,
	    double * E,
	    double * Lz,
	    double * I3V,
	    double delta,
	    double * u0,
	    double * cosh2u0,
	    double * sinh2u0,
	    double * potupi2,
	    int nargs,
	    struct actionAngleArg * actionAngleArgs,
	    int order){
  int ii;
  gsl_function JzInt;
  struct JzStaeckelArg * params= (struct JzStaeckelArg *) malloc ( sizeof (struct JzStaeckelArg) );
  params->delta= delta;
  params->nargs= nargs;
  params->actionAngleArgs= actionAngleArgs;
  //Setup integrator
  gsl_integration_glfixed_table * T= gsl_integration_glfixed_table_alloc (order);
  JzInt.function = &JzStaeckelIntegrand;
  for (ii=0; ii < ndata; ii++){
    if ( (0.5 * M_PI - *(vmin+ii)) / M_PI * 2. < 0.000001 ){//circular
      *(jz+ii) = 0.;
      continue;
    }
    //Setup function
    params->E= *(E+ii);
    params->Lz22delta= 0.5 * *(Lz+ii) * *(Lz+ii) / delta / delta;
    params->I3V= *(I3V+ii);
    params->u0= *(u0+ii);
    params->cosh2u0= *(cosh2u0+ii);
    params->sinh2u0= *(sinh2u0+ii);
    params->potupi2= *(potupi2+ii);
    JzInt.params = params;
    //Integrate
    *(jz+ii)= gsl_integration_glfixed (&JzInt,*(vmin+ii),M_PI/2.,T)
      * 2 * sqrt(2.) * delta / M_PI;
  }
  gsl_integration_glfixed_table_free ( T );
}
void calcUminUmax(int ndata,
		  double * umin,
		  double * umax,
		  double * ux,
		  double * pux,
		  double * E,
		  double * Lz,
		  double * I3U,
		  double delta,
		  double * u0,
		  double * sinh2u0,
		  double * v0,
		  double * sin2v0,
		  double * potu0v0,
		  int nargs,
		  struct actionAngleArg * actionAngleArgs){
  int ii;
  double peps, meps;
  gsl_function JRRoot;
  struct JRStaeckelArg * params= (struct JRStaeckelArg *) malloc ( sizeof (struct JRStaeckelArg) );
  params->delta= delta;
  params->nargs= nargs;
  params->actionAngleArgs= actionAngleArgs;
  //Setup solver
  int status;
  int iter, max_iter = 100;
  const gsl_root_fsolver_type *T;
  gsl_root_fsolver *s;
  double u_lo, u_hi;
  T = gsl_root_fsolver_brent;
  s = gsl_root_fsolver_alloc (T);
  JRRoot.function = &JRStaeckelIntegrandSquared;
  for (ii=0; ii < ndata; ii++){
    //Setup function
    params->E= *(E+ii);
    params->Lz22delta= 0.5 * *(Lz+ii) * *(Lz+ii) / delta / delta;
    params->I3U= *(I3U+ii);
    params->u0= *(u0+ii);
    params->sinh2u0= *(sinh2u0+ii);
    params->v0= *(v0+ii);
    params->sin2v0= *(sin2v0+ii);
    params->potu0v0= *(potu0v0+ii);
    JRRoot.params = params;
    //Find starting points for minimum
    if ( fabs(GSL_FN_EVAL(&JRRoot,*(ux+ii))) < 0.0000001){ //we are at umin or umax
      peps= GSL_FN_EVAL(&JRRoot,*(ux+ii)+0.0000001);
      meps= GSL_FN_EVAL(&JRRoot,*(ux+ii)-0.0000001);
      if ( fabs(peps) < 0.00000001 && fabs(meps) < 0.00000001 ) {//circular
	*(umin+ii) = *(ux+ii);
	*(umax+ii) = *(ux+ii);
      }
      else if ( peps < 0. && meps > 0. ) {//umax
	*(umax+ii)= *(ux+ii);
	u_lo= 0.9 * (*(ux+ii) - 0.0000001);
	u_hi= *(ux+ii) - 0.00000001;
	while ( GSL_FN_EVAL(&JRRoot,u_lo) >= 0. && u_lo > 0.000000001){
	  u_hi= u_lo; //this makes sure that brent evaluates using previous
	  u_lo*= 0.9;
	}
	//Find root
	//gsl_set_error_handler_off();
	status = gsl_root_fsolver_set (s, &JRRoot, u_lo, u_hi);
	//if (status == GSL_EINVAL) {
	//  printf("Error in uminmax\n");
	//printf("%f,%f,%f,%f,%f,%f,%f\n",u_lo,GSL_FN_EVAL(&JRRoot,u_lo),
	//	 u_hi,GSL_FN_EVAL(&JRRoot,u_hi),
	//	 *(pux+ii),peps,meps);
	//  fflush(stdout);
	//  continue;
	//}
	//gsl_set_error_handler (NULL);
	iter= 0;
	do
	  {
	    iter++;
	    status = gsl_root_fsolver_iterate (s);
	    u_lo = gsl_root_fsolver_x_lower (s);
	    u_hi = gsl_root_fsolver_x_upper (s);
	    status = gsl_root_test_interval (u_lo, u_hi,
					     9.9999999999999998e-13,
					     4.4408920985006262e-16);
	  }
	while (status == GSL_CONTINUE && iter < max_iter);
	*(umin+ii) = gsl_root_fsolver_root (s);
      }
      else if ( peps > 0. && meps < 0. ){//umin
	*(umin+ii)= *(ux+ii);
	u_lo= *(ux+ii) + 0.0000001;
	u_hi= 1.1 * (*(ux+ii) + 0.0000001);
	while ( GSL_FN_EVAL(&JRRoot,u_hi) >= 0. ) {
	  u_lo= u_hi; //this makes sure that brent evaluates using previous
	  u_hi*= 1.1;
	}
	//Find root
	status = gsl_root_fsolver_set (s, &JRRoot, u_lo, u_hi);
	iter= 0;
	do
	  {
	    iter++;
	    status = gsl_root_fsolver_iterate (s);
	    u_lo = gsl_root_fsolver_x_lower (s);
	    u_hi = gsl_root_fsolver_x_upper (s);
	    status = gsl_root_test_interval (u_lo, u_hi,
					     9.9999999999999998e-13,
					     4.4408920985006262e-16);
	  }
	while (status == GSL_CONTINUE && iter < max_iter);
	*(umax+ii) = gsl_root_fsolver_root (s);
      }
    }
    else {
      u_lo= 0.9 * *(ux+ii);
      u_hi= *(ux+ii);
      while ( GSL_FN_EVAL(&JRRoot,u_lo) >= 0. && u_lo > 0.000000001){
	u_hi= u_lo; //this makes sure that brent evaluates using previous
	u_lo*= 0.9;
      }
      u_hi= (u_lo < 0.9 * *(ux+ii)) ? u_lo / 0.9 / 0.9: *(ux+ii);
      //Find root
      status = gsl_root_fsolver_set (s, &JRRoot, u_lo, u_hi);
      iter= 0;
      do
	{
	  iter++;
	  status = gsl_root_fsolver_iterate (s);
	  u_lo = gsl_root_fsolver_x_lower (s);
	  u_hi = gsl_root_fsolver_x_upper (s);
	  status = gsl_root_test_interval (u_lo, u_hi,
					   9.9999999999999998e-13,
					   4.4408920985006262e-16);
	}
      while (status == GSL_CONTINUE && iter < max_iter);
      *(umin+ii) = gsl_root_fsolver_root (s);
      //Find starting points for maximum
      u_lo= *(ux+ii);
      u_hi= 1.1 * *(ux+ii);
      while ( GSL_FN_EVAL(&JRRoot,u_hi) > 0.) {
	u_lo= u_hi; //this makes sure that brent evaluates using previous
	u_hi*= 1.1;
      }
      u_lo= (u_hi > 1.1 * *(ux+ii)) ? u_hi / 1.1 / 1.1: *(ux+ii);
      //Find root
      status = gsl_root_fsolver_set (s, &JRRoot, u_lo, u_hi);
      iter= 0;
      do
	{
	  iter++;
	  status = gsl_root_fsolver_iterate (s);
	  u_lo = gsl_root_fsolver_x_lower (s);
	  u_hi = gsl_root_fsolver_x_upper (s);
	  status = gsl_root_test_interval (u_lo, u_hi,
					   9.9999999999999998e-13,
					   4.4408920985006262e-16);
	}
      while (status == GSL_CONTINUE && iter < max_iter);
      *(umax+ii) = gsl_root_fsolver_root (s);
    }
  }
 gsl_root_fsolver_free (s);    
}
void calcVmin(int ndata,
	      double * vmin,
	      double * vx,
	      double * pvx,
	      double * E,
	      double * Lz,
	      double * I3V,
	      double delta,
	      double * u0,
	      double * cosh2u0,
	      double * sinh2u0,
	      double * potupi2,
	      int nargs,
	      struct actionAngleArg * actionAngleArgs){
  int ii;
  gsl_function JzRoot;
  struct JzStaeckelArg * params= (struct JzStaeckelArg *) malloc ( sizeof (struct JzStaeckelArg) );
  params->delta= delta;
  params->nargs= nargs;
  params->actionAngleArgs= actionAngleArgs;
  //Setup solver
  int status;
  int iter, max_iter = 100;
  const gsl_root_fsolver_type *T;
  gsl_root_fsolver *s;
  double v_lo, v_hi;
  T = gsl_root_fsolver_brent;
  s = gsl_root_fsolver_alloc (T);
  JzRoot.function = &JzStaeckelIntegrandSquared;
  for (ii=0; ii < ndata; ii++){
    //Setup function
    params->E= *(E+ii);
    params->Lz22delta= 0.5 * *(Lz+ii) * *(Lz+ii) / delta / delta;
    params->I3V= *(I3V+ii);
    params->u0= *(u0+ii);
    params->cosh2u0= *(cosh2u0+ii);
    params->sinh2u0= *(sinh2u0+ii);
    params->potupi2= *(potupi2+ii);
    JzRoot.params = params;
    //Find starting points for minimum
    if ( fabs(GSL_FN_EVAL(&JzRoot,*(vx+ii))) < 0.0000001){ //we are at vmin
      *(vmin+ii)= *(vx+ii);
    }
    else {
      v_lo= 0.9 * *(vx+ii);
      v_hi= *(vx+ii);
      while ( GSL_FN_EVAL(&JzRoot,v_lo) >= 0. && v_lo > 0.000000001){
	v_hi= v_lo; //this makes sure that brent evaluates using previous
	v_lo*= 0.9;
      }
      //Find root
      status = gsl_root_fsolver_set (s, &JzRoot, v_lo, v_hi);
      iter= 0;
      do
	{
	  iter++;
	  status = gsl_root_fsolver_iterate (s);
	  v_lo = gsl_root_fsolver_x_lower (s);
	  v_hi = gsl_root_fsolver_x_upper (s);
	  status = gsl_root_test_interval (v_lo, v_hi,
					   9.9999999999999998e-13,
					   4.4408920985006262e-16);
	}
      while (status == GSL_CONTINUE && iter < max_iter);
      *(vmin+ii) = gsl_root_fsolver_root (s);
    }
  }
  gsl_root_fsolver_free (s);    
}

double JRStaeckelIntegrand(double u,
			   void * p){
  return sqrt(JRStaeckelIntegrandSquared(u,p));
}
double JRStaeckelIntegrandSquared(double u,
				  void * p){
  struct JRStaeckelArg * params= (struct JRStaeckelArg *) p;
  double sinh2u= sinh(u) * sinh(u);
  double dU= (sinh2u+params->sin2v0)
    *evaluatePotentialsUV(u,params->v0,params->delta,
			  params->nargs,params->actionAngleArgs)
    - (params->sinh2u0+params->sin2v0)*params->potu0v0;
  return params->E * sinh2u - params->I3U - dU  - params->Lz22delta / sinh2u;
}
  
double JzStaeckelIntegrand(double v,
			   void * p){
  return sqrt(JzStaeckelIntegrandSquared(v,p));
}
double JzStaeckelIntegrandSquared(double v,
				  void * p){
  struct JzStaeckelArg * params= (struct JzStaeckelArg *) p;
  double sin2v= sin(v) * sin(v);
  double dV= params->cosh2u0 * params->potupi2
    - (params->sinh2u0+sin2v)
    *evaluatePotentialsUV(params->u0,v,params->delta,
			  params->nargs,params->actionAngleArgs);
  return params->E * sin2v + params->I3V + dV  - params->Lz22delta / sin2v;
}
double u0Equation(double u, void * p){
  struct u0EqArg * params= (struct u0EqArg *) p;
  double sinh2u= sinh(u) * sinh(u);
  double cosh2u= cosh(u) * cosh(u);
  double dU= cosh2u * evaluatePotentialsUV(u,0.5*M_PI,params->delta,
				    params->nargs,params->actionAngleArgs);
  return -(params->E*sinh2u-dU-params->Lz22delta/sinh2u);
}  
double evaluatePotentials(double R, double Z, 
			  int nargs, struct actionAngleArg * actionAngleArgs){
  int ii;
  double pot= 0.;
  for (ii=0; ii < nargs; ii++){
    pot+= actionAngleArgs->potentialEval(R,Z,0.,0.,
					 actionAngleArgs->nargs,
					 actionAngleArgs->args);
    actionAngleArgs++;
  }
  actionAngleArgs-= nargs;
  return pot;
}
double evaluatePotentialsUV(double u, double v, double delta,
			    int nargs, 
			    struct actionAngleArg * actionAngleArgs){
  double R,z;
  uv_to_Rz(u,v,&R,&z,delta);
  return evaluatePotentials(R,z,nargs,actionAngleArgs);
}
