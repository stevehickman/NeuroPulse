/*
 * OI-CVNS-13 false-trip simulation — NOT a test, NOT built by CMake.
 * Document: NP-FW-CVNS-001 Rev 11 §14.6 (the tables there are this program's
 * output).  run_oi_cvns_13_sim.sh builds it against the Class C unit as built
 * and against edited COPIES of it (one per lever); the unit is never modified.
 *
 * What is new against oi_cvns_12_detect_sim.c:
 *   - The hub stage.  The safety MCU does not see raw detections.  It sees the
 *     RPEAK_IN pulses the hub emits, and np_cvns_interlock.c pt_process_sample()
 *     step 7 pulses a peak only if it is >= NP_CVNS_PT_REFRACTORY_MS after the
 *     last detected peak AND its interval is within NP_CVNS_RR_MIN/MAX_VALID_MS.
 *     A suppressed peak still moves the hub's "last peak".  Pipeline 0 feeds raw
 *     detections (as §14.5.1 did), pipeline 1 the hub as built, pipeline 2 the
 *     hub with the 2000 ms upper bound removed from the pulse gate only.
 *   - Artefact kinds are separated: missed detections and spurious (split)
 *     detections.
 *   - A correlated heart-rate model (respiratory sinus arrhythmia) beside the
 *     white-noise one.
 *   - A hazard scenario no step test covers: beats physiologically dropped
 *     (every k-th beat absent, as in second-degree AV block, a vagal effect).
 *     The ventricular rate falls although no single interval looks like a step.
 *
 * Modes:  0 steps/ramps (detection; same grid as §14.5.1)
 *         1 false trips, white-noise R-R jitter, by artefact kind
 *         2 false trips, respiratory sinus arrhythmia
 *         3 dropped-beat detection
 *   sim <mode> <pipeline>
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

/* Hub constants, copied from firmware/cervical_vns/include/np_cvns_config.h. */
#define HUB_REFRACTORY_S 0.200
#define HUB_RR_MIN_S     0.300
#define HUB_RR_MAX_S     2.000

#define MAXEV 4096
static double g_det[MAXEV]; static int g_ndet;     /* detections (raw) */
static double g_pul[MAXEV]; static int g_npul;     /* pulses at RPEAK_IN */

/* Scenario: steady hr0 until t_step, then a linear ramp to hr1 over ramp_s.
 * White R-R jitter sd_ms; RSA of rsa_frac (fraction of R-R) at rsa_hz.
 * Artefacts per beat: p_miss (detection missed), p_extra (a spurious detection
 * at 0.3-0.7 of the interval).  drop_k > 0: from t_step every drop_k-th beat
 * is physiologically absent. */
typedef struct { double hr0,hr1,t_step,ramp_s,sd_ms,rsa_frac,rsa_hz,p_miss,p_extra; int drop_k; } sc_t;

static void gen(const sc_t *s, double t_end){
  g_ndet=0; double t=urand()*60.0/s->hr0; int n_after=0;
  while (t < t_end && g_ndet < MAXEV-2){
    double hr=s->hr0;
    if (t>=s->t_step){ double f = s->ramp_s>0? (t-s->t_step)/s->ramp_s : 1; if(f>1)f=1; hr=s->hr0+(s->hr1-s->hr0)*f; }
    double rr=60.0/hr;
    rr *= 1.0 + s->rsa_frac*sin(2*M_PI*s->rsa_hz*t);
    rr += s->sd_ms/1000.0*gauss();
    if (rr<0.25) rr=0.25;
    bool absent = false;
    if (s->drop_k>0 && t>=s->t_step){ n_after++; absent = (n_after % s->drop_k)==0; }
    double u=urand();
    if (!absent && u >= s->p_miss) g_det[g_ndet++]=t;          /* beat detected */
    if (u >= s->p_miss && u < s->p_miss+s->p_extra) g_det[g_ndet++]=t+rr*(0.3+0.4*urand());
    t+=rr;
  }
}
/* pipeline 0 raw, 1 hub as built, 2 hub without the upper bound on the pulse */
static void hub(int pipe){
  g_npul=0; double last=-1e9;
  for(int i=0;i<g_ndet;i++){ double t=g_det[i];
    if (pipe==0){ g_pul[g_npul++]=t; continue; }
    if (t-last < HUB_REFRACTORY_S) continue;                   /* not a peak */
    double rr=t-last;
    bool ok = rr>=HUB_RR_MIN_S && (pipe==2 || rr<=HUB_RR_MAX_S);
    if (last < -1e8) ok = false;                               /* first peak: no interval */
    if (ok) g_pul[g_npul++]=t;
    last=t;
  }
}
/* Main loop every 1 ms; returns the first cutoff time (ms) or -1. */
static long run(double t_end_s){
  np_safety_state_t st; memset(&st,0,sizeof st); st.fault_slot=NP_FAULT_SLOT_NONE; st.cvns_active=true;
  np_cardiac_interlock_init(); g_ms=0; g_us=0; g_edge=false; int k=0;
  for (; g_ms < (uint32_t)(t_end_s*1000); ){
    g_ms++; g_us+=1000; double t=g_ms/1000.0;
    while (k<g_npul && g_pul[k]<=t){ g_cap=(uint32_t)(g_pul[k]*1e6); g_edge=true; k++;
      if (k<g_npul && g_pul[k]<=t) np_cardiac_interlock_tick(&st); }   /* two in one ms: tick between */
    if ((st.status & NP_SAFETY_STATUS_CARDIAC)==0) st.granted_mask|=NP_SAFETY_EN_CVNS;
    np_cardiac_interlock_tick(&st);
    if (st.status & NP_SAFETY_STATUS_CARDIAC) return g_ms;
  }
  return -1;
}
static double trip_rate(sc_t s, int pipe, int trials){
  int trips=0; for(int k=0;k<trials;k++){ gen(&s,120); hub(pipe); if(run(120)>0) trips++; }
  return 100.0*trips/trials;
}
int main(int argc,char**argv){
  if(argc<3){ fprintf(stderr,"usage: sim <mode> <pipeline>\n"); return 2; }
  int mode=atoi(argv[1]), pipe=atoi(argv[2]); srand(12345);
  if(mode==0){
    double hr0s[]={50,60,70,90,110}; double ds[]={-40,-25,-20,-16,16,20,25,40}; double ramps[]={0,2.5,5};
    int cells=0, full=0; double worst=100, wlat=0;
    for(int r=0;r<3;r++) for(int i=0;i<5;i++) for(int j=0;j<8;j++){
      int det=0,trials=200; double latmax=0;
      for(int k=0;k<trials;k++){ sc_t s={hr0s[i],hr0s[i]+ds[j],20+5*urand(),ramps[r],0,0,0,0,0,0};
        gen(&s,60); hub(pipe); long c=run(60);
        if(c>0 && c/1000.0>=s.t_step){det++; double L=c/1000.0-s.t_step; if(L>latmax)latmax=L;} }
      double pct=100.0*det/trials; cells++; if(pct>=100.0) full++; if(pct<worst) worst=pct;
      if (hr0s[i]+ds[j]>=40 && latmax>wlat) wlat=latmax;
      if (pct<100.0) printf("  not always cut: ramp=%.1f hr0=%3.0f d=%+3.0f detect=%5.1f%%\n",ramps[r],hr0s[i],ds[j],pct);
    }
    printf("steps: %d/%d cells cut in 100%% of trials; worst cell %.1f%%; max latency (end >= 40 BPM) %.1f s\n",full,cells,worst,wlat);
  } else if(mode==1){
    double hr0s[]={50,60,70,100}; double sds[]={20,50}; double arts[]={0,0.01,0.03};
    for(int i=0;i<4;i++) for(int a=0;a<2;a++){
      printf("hr=%3.0f sd=%2.0fms", hr0s[i], sds[a]);
      for(int b=0;b<3;b++){
        sc_t m={hr0s[i],hr0s[i],1e9,0,sds[a],0,0,arts[b],0,0};
        sc_t x={hr0s[i],hr0s[i],1e9,0,sds[a],0,0,0,arts[b],0};
        if(b==0) printf("  none=%5.1f%%", trip_rate(m,pipe,300));
        else printf("  miss%.0f%%=%5.1f%% split%.0f%%=%5.1f%%", arts[b]*100, trip_rate(m,pipe,300), arts[b]*100, trip_rate(x,pipe,300));
      }
      printf("\n");
    }
  } else if(mode==2){
    /* RSA: R-R modulated by +/- frac at 0.25 Hz (spontaneous breathing) or
     * 0.1 Hz (paced, resonance-frequency breathing), plus 10 ms white jitter. */
    double hr0s[]={50,70,100}; double fr[]={0.05,0.10,0.15}; double hz[]={0.25,0.1};
    for(int h=0;h<2;h++) for(int i=0;i<3;i++){
      printf("rsa=%.2fHz hr=%3.0f", hz[h], hr0s[i]);
      for(int f=0;f<3;f++){ sc_t s={hr0s[i],hr0s[i],1e9,0,10,fr[f],hz[h],0,0,0};
        printf("  +/-%2.0f%%=%5.1f%%", fr[f]*100, trip_rate(s,pipe,300)); }
      printf("\n");
    }
  } else {
    double hr0s[]={50,60,70,90}; int ks[]={2,3,4};
    for(int i=0;i<4;i++){ printf("hr0=%3.0f", hr0s[i]);
      for(int j=0;j<3;j++){ int det=0,trials=200; double latmax=0;
        for(int k=0;k<trials;k++){ sc_t s={hr0s[i],hr0s[i],20+5*urand(),0,0,0,0,0,0,ks[j]};
          gen(&s,60); hub(pipe); long c=run(60);
          if(c>0 && c/1000.0>=s.t_step){det++; double L=c/1000.0-s.t_step; if(L>latmax)latmax=L;} }
        printf("  every %d dropped (->%3.0f)=%5.1f%% max %4.1fs", ks[j], hr0s[i]*(ks[j]-1)/ks[j], 100.0*det/trials, latmax); }
      printf("\n");
    }
  }
  return 0;
}
