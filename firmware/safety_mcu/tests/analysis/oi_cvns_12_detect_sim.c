/*
 * OI-CVNS-12 detection simulation — NOT a test, NOT built by CMake.
 * Document: NP-FW-CVNS-001 Rev 10 §14.5.1 (the tables there are this program's
 * output against the unit as built; Rev 9 §14.5's are its output at 69ea5e7).
 *
 * Links the real Class C unit (np_cardiac_interlock.c) against a mocked HAL,
 * ticks the main loop every 1 ms with R-peaks in real time, and reports:
 *   mode 0 — the share of sustained HR steps / ramps that are cut, and latency;
 *   mode 1 — the share of 120 s steady-rhythm sessions that false-trip, with
 *            R-R jitter and missed / split R-peaks.
 * Rev 9 varied the window length and the 5 s refresh by compiling edited
 * COPIES of the unit; Rev 10 runs the unit as built.  It is never modified.
 * Fixed seed, so every run reproduces the published figures.
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "np_safety_config.h"
#include "np_safety_hal.h"
#include "np_safety_protocol.h"
extern np_safe_status_t np_cardiac_interlock_init(void);
extern void np_cardiac_interlock_tick(np_safety_state_t *state);
static uint32_t g_ms; static uint64_t g_us; static bool g_edge; static uint32_t g_cap;
uint32_t np_hal_get_tick_ms(void){return g_ms;}
uint32_t np_hal_tim2_get_capture(void){return g_cap;}
uint32_t np_hal_tim2_now_us(void){return (uint32_t)g_us;}
bool np_hal_rpeak_edge_pending(void){return g_edge;}
void np_hal_rpeak_edge_clear(void){g_edge=false;}
static double urand(void){return rand()/(RAND_MAX+1.0);}
static double gauss(void){double u=urand()+1e-12,v=urand();return sqrt(-2*log(u))*cos(2*M_PI*v);}
/* Run: beats given by rr_fn over time; returns time (ms) of first cutoff after t_on, or -1.
   Main loop ticks every 1 ms. */
typedef double (*rr_fn)(double t_s, void *ctx);
static long run(rr_fn f, void *ctx, double t_end_s, double t_pre_s, long *arm_ms){
  np_safety_state_t st; memset(&st,0,sizeof st); st.fault_slot=NP_FAULT_SLOT_NONE; st.cvns_active=true;
  np_cardiac_interlock_init(); g_ms=(uint32_t)(t_pre_s*1000); g_us=(uint64_t)g_ms*1000; g_edge=false;
  double next_beat = t_pre_s + f(t_pre_s,ctx)*urand();
  *arm_ms=-1;
  for (; g_ms < (uint32_t)(t_end_s*1000); ){
    g_ms++; g_us+=1000;
    double t=g_ms/1000.0;
    if (t>=next_beat){ g_cap=(uint32_t)(next_beat*1e6); g_edge=true; next_beat += f(next_beat,ctx); }
    if ((st.status & NP_SAFETY_STATUS_CARDIAC)==0) st.granted_mask|=NP_SAFETY_EN_CVNS;
    np_cardiac_interlock_tick(&st);
    if (*arm_ms<0 && (st.granted_mask&NP_SAFETY_EN_CVNS)) *arm_ms=g_ms;
    if (st.status & NP_SAFETY_STATUS_CARDIAC) return g_ms;
  }
  return -1;
}
/* scenario: steady hr0 until t_step, then linear ramp to hr1 over ramp_s; hrv noise sd (ms) */
typedef struct { double hr0,hr1,t_step,ramp_s,sd_ms,p_miss,p_extra; } sc_t;
static double sc_rr(double t, void *c){ sc_t*s=c; double hr=s->hr0;
  if (t>=s->t_step){ double f = s->ramp_s>0? (t-s->t_step)/s->ramp_s : 1; if(f>1)f=1; hr = s->hr0+(s->hr1-s->hr0)*f; }
  double rr=60.0/hr + s->sd_ms/1000.0*gauss();
  double u=urand();
  if (u < s->p_miss) rr *= 2;             /* missed detection: interval doubles */
  else if (u < s->p_miss+s->p_extra) rr *= 0.3+0.4*urand(); /* spurious split, >=refractory-ish */
  if (rr<0.2) rr=0.2; return rr; }
int main(int argc,char**argv){
  int mode=atoi(argv[1]); srand(12345);
  if(mode==0){ /* step / ramp detection */
    double hr0s[]={50,60,70,90,110}; double ds[]={-40,-25,-20,-16,16,20,25,40}; double ramps[]={0,2.5,5};
    for(int r=0;r<3;r++) for(int i=0;i<5;i++) for(int j=0;j<8;j++){
      int det=0,trials=200; double lat=0,latmax=0;
      for(int k=0;k<trials;k++){ sc_t s={hr0s[i],hr0s[i]+ds[j],20+5*urand(),ramps[r],0,0,0}; long arm;
        long c=run(sc_rr,&s,60,0,&arm);
        if(c>0 && c/1000.0>=s.t_step){det++; double L=c/1000.0-s.t_step; lat+=L; if(L>latmax)latmax=L;} }
      printf("ramp=%.1f hr0=%3.0f d=%+3.0f detect=%5.1f%% meanlat=%5.2fs maxlat=%5.2fs\n",ramps[r],hr0s[i],ds[j],100.0*det/trials,det?lat/det:0,latmax);
    }
  } else { /* false trips on steady HR: noise sd, miss/extra rates; 120 s sessions */
    double hr0s[]={50,70,100}; double sds[]={20,50,80}; double arts[]={0,0.01,0.03};
    for(int i=0;i<3;i++) for(int a=0;a<3;a++) for(int b=0;b<3;b++){
      int trips=0,trials=300; double armsum=0;
      for(int k=0;k<trials;k++){ sc_t s={hr0s[i],hr0s[i],1e9,0,sds[a],arts[b]/2,arts[b]/2}; long arm; long c=run(sc_rr,&s,120,0,&arm); if(c>0)trips++; if(arm>0)armsum+=arm; }
      printf("hr=%3.0f sd=%2.0fms artefact=%4.1f%% false-trip/120s-session=%5.1f%% mean-arm=%.1fs\n",hr0s[i],sds[a],arts[b]*100,100.0*trips/trials,armsum/trials/1000);
    }
  }
}
