
#include "tc_set.h"
#include "link_set.h"
#include "lq_plugin.h"
#include "olsr_spf.h"
#include "lq_packet.h"
#include "packet.h"
#include "olsr.h"
#include "lq_plugin_default_ff.h"
#include "parser.h"
#include "fpm.h"
#include "mid_set.h"
#include "scheduler.h"
#include "log.h"
#include <time.h>


struct timeval startTime1;
extern struct timeval startTime;

#define FERENTRIES 12 
int ss_array[FERENTRIES]={-30,-40,-50,-60,-70,-75,-80,-85,-87,-88,-89,-90};//ss: signal strength: MUST BE DECREASING
float fer_signal[FERENTRIES]={0.0,0.0,0.0,0.0,0.0,0.0,0.05,0.1,0.18,0.4,0.6,1.0};
 
int threshold_signal= -60;//AB: a modifier en fonction des tests


static void default_lq_initialize_ff(void);

static olsr_linkcost default_lq_calc_cost_ff(const void *lq);

static void default_lq_packet_loss_worker_ff(struct link_entry *link, void *lq, bool lost);
static void default_lq_memorize_foreign_hello_ff(void *local, void *foreign);

static int default_lq_serialize_hello_lq_pair_ff(unsigned char *buff, void *lq);
static void default_lq_deserialize_hello_lq_pair_ff(const uint8_t ** curr, void *lq);
static int default_lq_serialize_tc_lq_pair_ff(unsigned char *buff, void *lq);
static void default_lq_deserialize_tc_lq_pair_ff(const uint8_t ** curr, void *lq);

static void default_lq_copy_link2neigh_ff(void *t, void *s);
static void default_lq_copy_link2tc_ff(void *target, void *source);
static void default_lq_clear_ff(void *target);
static void default_lq_clear_ff_hello(void *target);

/****** function added *****/

int signalPrediction(struct link_entry *entry);
float default_lq_etx_ant_get_fer(int signalStrength);

/****** function added *****/

static const char *default_lq_print_ff(void *ptr, char separator, struct lqtextbuffer *buffer);
static const char *default_lq_print_cost_ff(olsr_linkcost cost, struct lqtextbuffer *buffer);

/* etx lq plugin (freifunk fpm version) settings */
struct lq_handler lq_etx_ff_handler = {
  &default_lq_initialize_ff,
  &default_lq_calc_cost_ff,
  &default_lq_calc_cost_ff,

  &default_lq_packet_loss_worker_ff,

  &default_lq_memorize_foreign_hello_ff,
  &default_lq_copy_link2neigh_ff,
  &default_lq_copy_link2tc_ff,
  &default_lq_clear_ff_hello,
  &default_lq_clear_ff,

  &default_lq_serialize_hello_lq_pair_ff,
  &default_lq_serialize_tc_lq_pair_ff,
  &default_lq_deserialize_hello_lq_pair_ff,
  &default_lq_deserialize_tc_lq_pair_ff,

  &default_lq_print_ff,
  &default_lq_print_ff,
  &default_lq_print_cost_ff,

  sizeof(struct default_lq_ff_hello),
  sizeof(struct default_lq_ff),
  4,
  4
};


int 
signalPrediction(struct link_entry *entry)
{
  float timeShift=5.0;	//timeShift: number of seconds for which we anticipate
  float estimatedSignal;
  int nb_valid_signal=0, k, i;
  float meanx, meany, varx, covxy, a, b;		//Coefficient for the linear regression 

  FILE* debug;

  if(DEBUGANT && (debug=fopen("debug2.log","a+"))==NULL)
  {
    perror("fopen() debug fail\n");
    exit(1);
  }
	 
  meanx=0.0; meany=0.0;varx=0.0;covxy=0.0;
	
  //signal_index is the oldest
  i=entry->signal_index;
  for(k=0;k<NBOFSIGNAL;k++)
  {
    if(entry->link_signal[i]==0) //Missed hello
    {
      //AB: TO DO
    }

    if(entry->link_signal[i]==1) //Error on signal (not yet received enough hellos or error when getting the signal strength)
    {
       //We ignore the entry
    }
		
    if(entry->link_signal[i]<0) //All is fine 
    {
      meany+=1.0*entry->link_signal[i];	// dBm (is <0)
      meanx+=1.0*entry->loss_helloint*1.0*k;//loss_helloint is a 32 integer expressed in ms
      if(DEBUGANT) fprintf(debug,"Linear regression  signal[%d]=%d\n",i,entry->link_signal[i]);
      nb_valid_signal++;
    }
      i=(i+1)%NBOFSIGNAL;
  }

  if(nb_valid_signal<2) return(1); //We need at least 2 signals to do a prediction
  meany=meany/(1.0*nb_valid_signal); meanx=meanx/(1.0*nb_valid_signal); 

  i=entry->signal_index;
  for(k=0;k<NBOFSIGNAL;k++)
  {
    if(entry->link_signal[i]==0) //Missed hello
    {
      //AB: TO DO
    }
	

    if(entry->link_signal[i]<0) //All is fine 
    {
      covxy+=(1.0*entry->loss_helloint*1.0*k  - meanx)*(1.0*entry->link_signal[i]-meany); //dBm
      varx+= (1.0*entry->loss_helloint*1.0*k)*(1.0*entry->loss_helloint*1.0*k);
    }
    i=(i+1)%NBOFSIGNAL;
  }
  varx=varx - 1.0*nb_valid_signal*meanx*meanx;
  b=covxy/varx;
  a=meany-b*meanx;			

  //AB: the linear regression
  estimatedSignal=a+b*(1.0*(k-1)*1.0*entry->loss_helloint+timeShift*1000.0); //dBm and ms

  if(DEBUGANT) fprintf(debug,"Linear regression  estimated signal=%d\n",(int) estimatedSignal);
  if(DEBUGANT) fclose(debug);

  return ((int) estimatedSignal);
}



static void
default_lq_ff_handle_lqchange(void) {
  struct default_lq_ff_hello *lq;
  struct ipaddr_str buf;
  struct link_entry *link;

  bool triggered = false;

  OLSR_FOR_ALL_LINK_ENTRIES(link) {
    bool relevant = false;
    lq = (struct default_lq_ff_hello *)link->linkquality;

    if (lq->smoothed_lq.valueLq < lq->lq.valueLq) {
      if (lq->lq.valueLq == 255 || lq->lq.valueLq - lq->smoothed_lq.valueLq > lq->smoothed_lq.valueLq/10) {
        relevant = true;
      }
    }
    else if (lq->smoothed_lq.valueLq > lq->lq.valueLq) {
      if (lq->smoothed_lq.valueLq - lq->lq.valueLq > lq->smoothed_lq.valueLq/10) {
        relevant = true;
      }
    }
    if (lq->smoothed_lq.valueNlq < lq->lq.valueNlq) {
      if (lq->lq.valueNlq == 255 || lq->lq.valueNlq - lq->smoothed_lq.valueNlq > lq->smoothed_lq.valueNlq/10) {
        relevant = true;
      }
    }
    else if (lq->smoothed_lq.valueNlq > lq->lq.valueNlq) {
      if (lq->smoothed_lq.valueNlq - lq->lq.valueNlq > lq->smoothed_lq.valueNlq/10) {
        relevant = true;
      }
    }

    if (relevant) {
      memcpy(&lq->smoothed_lq, &lq->lq, sizeof(struct default_lq_ff));
      link->linkcost = default_lq_calc_cost_ff(&lq->smoothed_lq);
      triggered = true;
    }
  } OLSR_FOR_ALL_LINK_ENTRIES_END(link)

  if (!triggered) {
    return;
  }

  OLSR_FOR_ALL_LINK_ENTRIES(link) {
    lq = (struct default_lq_ff_hello *)link->linkquality;

    if (lq->smoothed_lq.valueLq == 255 && lq->smoothed_lq.valueNlq == 255) {
      continue;
    }

    if (lq->smoothed_lq.valueLq == lq->lq.valueLq && lq->smoothed_lq.valueNlq == lq->lq.valueNlq) {
      continue;
    }

    memcpy(&lq->smoothed_lq, &lq->lq, sizeof(struct default_lq_ff));
    link->linkcost = default_lq_calc_cost_ff(&lq->smoothed_lq);
  } OLSR_FOR_ALL_LINK_ENTRIES_END(link)

  olsr_relevant_linkcost_change();
}

static void
default_lq_parser_ff(struct olsr *olsr, struct interface *in_if, union olsr_ip_addr *from_addr)
{
  const union olsr_ip_addr *main_addr;
  struct link_entry *lnk;
  struct default_lq_ff_hello *lq;
  uint32_t seq_diff;

  /* Find main address */
  main_addr = mid_lookup_main_addr(from_addr);

  /* Loopup link entry */
  lnk = lookup_link_entry(from_addr, main_addr, in_if);
  if (lnk == NULL) {
    return;
  }

  lq = (struct default_lq_ff_hello *)lnk->linkquality;

  /* ignore double package */
  if (lq->last_seq_nr == olsr->olsr_seqno) {
    struct ipaddr_str buf;
    olsr_syslog(OLSR_LOG_INFO, "detected duplicate packet with seqnr 0x%x from %s on %s (%d Bytes)",
		olsr->olsr_seqno,olsr_ip_to_string(&buf, from_addr),in_if->int_name,ntohs(olsr->olsr_packlen));
    return;
  }

  if (lq->last_seq_nr > olsr->olsr_seqno) {
    seq_diff = (uint32_t) olsr->olsr_seqno + 65536 - lq->last_seq_nr;
  } else {
    seq_diff = olsr->olsr_seqno - lq->last_seq_nr;
  }

  /* Jump in sequence numbers ? */
  if (seq_diff > 256) {
    seq_diff = 1;
  }

  lq->received[lq->activePtr]++;
  lq->total[lq->activePtr] += seq_diff;

  lq->last_seq_nr = olsr->olsr_seqno;
  lq->missed_hellos = 0;
}

float 
default_lq_etx_ant_get_fer(int signalStrength)
{
  int i=1;
  float ratio, diff;

  if(signalStrength >= ss_array[0]) return(fer_signal[0]);//should be fer=0.0 

  while(i<FERENTRIES && signalStrength < ss_array[i]) i++;
  
  if( i >= FERENTRIES ) return(fer_signal[FERENTRIES-1]);
  if( fer_signal[i]<=0.0 ) return(0.0);

  ratio = (1.0*(signalStrength - ss_array[i]))/(1.0*(ss_array[i-1]-ss_array[i]));
  diff = fer_signal[i]-fer_signal[i-1];

 return(fer_signal[i-1]+diff*ratio);
}

static void
default_lq_ff_timer(void __attribute__ ((unused)) * context)
{
  struct link_entry *link;
  
  FILE* debug;

  if(DEBUGANT && (debug=fopen("debug.log","a+"))==NULL)
  {
	perror("fopen() debug fail\n");
  	exit(1);
  }

  OLSR_FOR_ALL_LINK_ENTRIES(link) {
    struct default_lq_ff_hello *tlq = (struct default_lq_ff_hello *)link->linkquality;
    fpm ratio;
    int i, signalStrength, received, total;
    float fer;


    //Update of lq (link quality) is done in this function
    //The etx_ant algorithm that anticipates lq is done here. 
   
    if(link->signal_index==0) signalStrength=link->link_signal[NBOFSIGNAL-1];
    else signalStrength=link->link_signal[link->signal_index-1];

    received = 0;
    total = 0;

    /* We keep the lines below to have the ETX statistcis update (even if we not use it) */
    /* enlarge window if still in quickstart phase */
    if (tlq->windowSize < LQ_FF_WINDOW) {
      tlq->windowSize++;
    }
    for (i = 0; i < tlq->windowSize; i++) {
      received += tlq->received[i];
      total += tlq->total[i];
    }

    /* calculate link quality */

    // start with link-loss-factor
    ratio = fpmidiv(itofpm(link->loss_link_multiplier), LINK_LOSS_MULTIPLIER);
    
    //The signal is above threshold_limit so we use the classical ETX metric	
    //If there is an error getting the signal, signalStrength=1 and we use the classical ETX (since threshold_signal<0)
    if(signalStrength>threshold_signal)
    {
      if(DEBUGANT) fprintf(debug,"The classical ETX is used\n");
      if (total == 0) {
        tlq->lq.valueLq = 0;
      } else {

        /* keep missed hello periods in mind (round up hello interval to seconds) */
        if (tlq->missed_hellos > 1) {
          received = received - received * tlq->missed_hellos * link->inter->hello_etime/1000 / LQ_FF_WINDOW;
        }
      }
    } else {
      if(DEBUGANT) fprintf(debug,"ETX_ANT is used\n");
      signalStrength=signalPrediction(link);
      fer = default_lq_etx_ant_get_fer(signalStrength);
      if(DEBUGANT) fprintf(debug,"The anticipated fer is =%f\n",fer);
      total = 100;
      received = (int) 100.0 * (1.0 - fer);
    }

    // calculate received/total factor
    ratio = fpmmuli(ratio, received);
    ratio = fpmidiv(ratio, total);
    ratio = fpmmuli(ratio, 255);
      
    tlq->lq.valueLq = (uint8_t) (fpmtoi(ratio));
      
    
    // shift buffer 
    tlq->activePtr = (tlq->activePtr + 1) % LQ_FF_WINDOW;
    tlq->total[tlq->activePtr] = 0;
    tlq->received[tlq->activePtr] = 0;
    
  } OLSR_FOR_ALL_LINK_ENTRIES_END(link);
  
  if(DEBUGANT) fclose(debug);
  default_lq_ff_handle_lqchange();
}

static void
default_lq_initialize_ff(void)
{
  olsr_packetparser_add_function(&default_lq_parser_ff);
  olsr_start_timer(1000, 0, OLSR_TIMER_PERIODIC, &default_lq_ff_timer, NULL, 0);
}

static olsr_linkcost
default_lq_calc_cost_ff(const void *ptr)
{
  const struct default_lq_ff *lq = ptr;
  olsr_linkcost cost;

  if (lq->valueLq < (unsigned int)(255 * MINIMAL_USEFUL_LQ) || lq->valueNlq < (unsigned int)(255 * MINIMAL_USEFUL_LQ)) {
    return LINK_COST_BROKEN;
  }

  cost = fpmidiv(itofpm(255 * 255), (int)lq->valueLq * (int)lq->valueNlq);

  if (cost > LINK_COST_BROKEN)
    return LINK_COST_BROKEN;
  if (cost == 0)
    return 1;
  return cost;
}

static int
default_lq_serialize_hello_lq_pair_ff(unsigned char *buff, void *ptr)
{
  struct default_lq_ff *lq = ptr;

  buff[0] = (unsigned char)lq->valueLq;
  buff[1] = (unsigned char)lq->valueNlq;
  buff[2] = (unsigned char)(0);
  buff[3] = (unsigned char)(0);

  return 4;
}

static void
default_lq_deserialize_hello_lq_pair_ff(const uint8_t ** curr, void *ptr)
{
  struct default_lq_ff *lq = ptr;

  pkt_get_u8(curr, &lq->valueLq);
  pkt_get_u8(curr, &lq->valueNlq);
  pkt_ignore_u16(curr);
}

static int
default_lq_serialize_tc_lq_pair_ff(unsigned char *buff, void *ptr)
{
  struct default_lq_ff *lq = ptr;

  buff[0] = (unsigned char)lq->valueLq;
  buff[1] = (unsigned char)lq->valueNlq;
  buff[2] = (unsigned char)(0);
  buff[3] = (unsigned char)(0);

  return 4;
}

static void
default_lq_deserialize_tc_lq_pair_ff(const uint8_t ** curr, void *ptr)
{
  struct default_lq_ff *lq = ptr;

  pkt_get_u8(curr, &lq->valueLq);
  pkt_get_u8(curr, &lq->valueNlq);
  pkt_ignore_u16(curr);
}

static void
default_lq_packet_loss_worker_ff(struct link_entry *link,
    void __attribute__ ((unused)) *ptr, bool lost)
{
  struct default_lq_ff_hello *tlq = (struct default_lq_ff_hello *)link->linkquality;
  FILE* debug;

  if (lost) {
    tlq->missed_hellos++;
  }

  if(DEBUGANT && (debug=fopen("debug.log","a+"))==NULL)
  {
	perror("fopen() debug fail\n");
  	exit(1);
  }

  if(DEBUGANT) fprintf(debug,"Error (2) on Signal - missed hello = 0 inserted at index %d\n", link->signal_index);

  if (lost) {
    //We indicate a loss in the array use in the ETX_ANT computation
    link->link_signal[link->signal_index]=0;	// 0 is a missing hello
    link->signal_index=(link->signal_index+1)%NBOFSIGNAL; 	
  }

  if(DEBUGANT) fclose(debug);

  return;
}

static void
default_lq_memorize_foreign_hello_ff(void *ptrLocal, void *ptrForeign)
{
  struct default_lq_ff_hello *local = ptrLocal;
  struct default_lq_ff *foreign = ptrForeign;

  if (foreign) {
    local->lq.valueNlq = foreign->valueLq;
  } else {
    local->lq.valueNlq = 0;
  }
}

static void
default_lq_copy_link2neigh_ff(void *t, void *s)
{
  struct default_lq_ff *target = t;
  struct default_lq_ff_hello *source = s;
  *target = source->smoothed_lq;
}

static void
default_lq_copy_link2tc_ff(void *t, void *s)
{
  struct default_lq_ff *target = t;
  struct default_lq_ff_hello *source = s;
  *target = source->smoothed_lq;
}

static void
default_lq_clear_ff(void *target)
{
  memset(target, 0, sizeof(struct default_lq_ff));
}

static void
default_lq_clear_ff_hello(void *target)
{
  struct default_lq_ff_hello *local = target;
  int i;

  default_lq_clear_ff(&local->lq);
  default_lq_clear_ff(&local->smoothed_lq);
  local->windowSize = LQ_FF_QUICKSTART_INIT;
  for (i = 0; i < LQ_FF_WINDOW; i++) {
    local->total[i] = 3;
  }
}

static const char *
default_lq_print_ff(void *ptr, char separator, struct lqtextbuffer *buffer)
{
  struct default_lq_ff *lq = ptr;

  snprintf(buffer->buf, sizeof(buffer->buf), "%s%c%s", fpmtoa(fpmidiv(itofpm((int)lq->valueLq), 255)), separator,
           fpmtoa(fpmidiv(itofpm((int)lq->valueNlq), 255)));
  return buffer->buf;
}

static const char *
default_lq_print_cost_ff(olsr_linkcost cost, struct lqtextbuffer *buffer)
{
  snprintf(buffer->buf, sizeof(buffer->buf), "%s", fpmtoa(cost));
  return buffer->buf;
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
