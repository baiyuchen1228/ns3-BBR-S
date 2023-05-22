#include <unistd.h>
#include <time.h>
#include <limits>
#include <algorithm>
#include <iostream>
#include "tcp-bbrplus.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "tcp-bbr-debug.h"
#include "tcp-cubic.h"



namespace ns3{
NS_LOG_COMPONENT_DEFINE ("TcpBbrPlus");
NS_OBJECT_ENSURE_REGISTERED (TcpBbrPlus);
namespace {
// Constants based on TCP defaults.
// The minimum CWND to ensure delayed acks don't reduce bandwidth measurements.
// Does not inflate the pacing rate.
const uint32_t kMinCWndSegment= 4;

// The gain used for the STARTUP, equal to 2/ln(2).
const double kDefaultHighGain = 2.885f;
// The newly derived gain for STARTUP, equal to 4 * ln(2)
const double kDerivedHighGain = 2.773f;
// The newly derived CWND gain for STARTUP, 2.
const double kDerivedHighCWNDGain = 2.0f;
// The cycle of gains used during the PROBE_BW stage.
const double kPacingGain[] = {1.25, 0.75, 1, 1, 1, 1, 1, 1};
// The length of the gain cycle.
const size_t kGainCycleLength = sizeof(kPacingGain) / sizeof(kPacingGain[0]);
// The size of the bandwidth filter window, in round-trips.
const uint64_t kBandwidthWindowSize = kGainCycleLength + 2;

const double kCWNDGainConstant=2.0;
// The time after which the current min_rtt value expires.
const Time kMinRttExpiry =Seconds(10);
// The minimum time the connection can spend in PROBE_RTT mode.
const Time kProbeRttTime = MilliSeconds(200);

const double kStartupGrowthTarget=1.25;

static const int bbr_pacing_margin_percent = 1;

/* But after 3 rounds w/o significant bw growth, estimate pipe is full: */
static const uint32_t bbr_full_bw_cnt = 3;

/* "long-term" ("LT") bandwidth estimator parameters... */
/* The minimum number of rounds in an LT bw sampling interval: */
static const uint32_t bbr_lt_intvl_min_rtts = 4;
/* If lost/delivered ratio > 20%, interval is "lossy" and we may be policed: */
static const uint32_t bbr_lt_loss_thresh_num=2;
static const uint32_t bbr_lt_loss_thresh_den=10;
/* If 2 intervals have a bw ratio <= 1/8, their bw is "consistent": */
static const double bbr_lt_bw_ratio = 0.125;
/* If 2 intervals have a bw diff <= 4 Kbit/sec their bw is "consistent": */
static const DataRate bbr_lt_bw_diff =DataRate(4000);
/* If we estimate we're policed, use lt_bw for this many round trips: */
const uint32_t bbr_lt_bw_max_rtts =48;

/* Gain factor for adding extra_acked to target cwnd: */
static const double bbr_extra_acked_gain = 1.0;
/* Window length of extra_acked window. */
static const uint32_t bbr_extra_acked_win_rtts = 5;
/* Max allowed val for ack_epoch_acked, after which sampling epoch is reset */
static const uint32_t bbr_ack_epoch_acked_reset_thresh = 1U << 20;
/* Time period for clamping cwnd increment due to ack aggregation */
static const Time bbr_extra_acked_max_time = MilliSeconds(100);
}  // namespace
namespace{
    uint32_t kAddPackets=8;
    bool kAddMode=false;
}
void TcpBbrPlus::SetAddMode(bool enable){
    kAddMode=enable;
}
void TcpBbrPlus::SetAddOn(uint32_t packets){
    if(packets>4){
        kAddPackets=packets;
    }
}
TypeId TcpBbrPlus::GetTypeId (void){

    static TypeId tid = TypeId ("ns3::TcpBbrPlus")
    .SetParent<TcpCongestionOps> ()
    .AddConstructor<TcpBbrPlus> ()
    .SetGroupName ("Internet")
    .AddAttribute ("HighGain",
                   "Value of high gain",
                   DoubleValue (kDefaultHighGain),
                   MakeDoubleAccessor (&TcpBbrPlus::m_highGain),
                   MakeDoubleChecker<double> ())
    //by jh
    .AddAttribute ("FastConvergence", "Enable (true) or disable (false) fast convergence",
                   BooleanValue (true),
                   MakeBooleanAccessor (&TcpBbrPlus::m_fastConvergence),
                   MakeBooleanChecker ())    
    .AddAttribute ("Beta", "Beta for multiplicative decrease",
                   DoubleValue (0.7),
                   MakeDoubleAccessor (&TcpBbrPlus::m_beta),
                   MakeDoubleChecker <double> (0.0))        
    .AddAttribute ("HyStartLowWindow", "Lower bound cWnd for hybrid slow start (segments)",
                   UintegerValue (16),
                   MakeUintegerAccessor (&TcpBbrPlus::m_hystartLowWindow),
                   MakeUintegerChecker <uint32_t> ())    
    .AddAttribute ("HyStartDetect", "Hybrid Slow Start detection mechanisms:" \
                   "1: packet train, 2: delay, 3: both",
                   IntegerValue (3),
                   MakeIntegerAccessor (&TcpBbrPlus::m_hystartDetect),
                   MakeIntegerChecker <int> (1,3))
    .AddAttribute ("HyStartMinSamples", "Number of delay samples for detecting the increase of delay",
                   UintegerValue (8),
                   MakeUintegerAccessor (&TcpBbrPlus::m_hystartMinSamples),
                   MakeUintegerChecker <uint8_t> ())
    .AddAttribute ("HyStartAckDelta", "Spacing between ack's indicating train",
                   TimeValue (MilliSeconds (2)),
                   MakeTimeAccessor (&TcpBbrPlus::m_hystartAckDelta),
                   MakeTimeChecker ())
    .AddAttribute ("HyStartDelayMin", "Minimum time for hystart algorithm",
                   TimeValue (MilliSeconds (4)),
                   MakeTimeAccessor (&TcpBbrPlus::m_hystartDelayMin),
                   MakeTimeChecker ())
    .AddAttribute ("HyStartDelayMax", "Maximum time for hystart algorithm",
                   TimeValue (MilliSeconds (1000)),
                   MakeTimeAccessor (&TcpBbrPlus::m_hystartDelayMax),
                   MakeTimeChecker ())
    .AddAttribute ("CubicDelta", "Delta Time to wait after fast recovery before adjusting param",
                   TimeValue (MilliSeconds (10)),
                   MakeTimeAccessor (&TcpBbrPlus::m_cubicDelta),
                   MakeTimeChecker ())
    .AddAttribute ("CntClamp", "Counter value when no losses are detected (counter is used" \
                   " when incrementing cWnd in congestion avoidance, to avoid" \
                   " floating point arithmetic). It is the modulo of the (avoided)" \
                   " division",
                   UintegerValue (20),
                   MakeUintegerAccessor (&TcpBbrPlus::m_cntClamp),
                   MakeUintegerChecker <uint8_t> ())
    .AddAttribute ("C", "Cubic Scaling factor",
                   DoubleValue (0.4),
                   MakeDoubleAccessor (&TcpBbrPlus::m_c),
                   MakeDoubleChecker <double> (0.0)) 
    //by jh            
  ;
  
  return tid;
}


TcpBbrPlus::TcpBbrPlus():TcpCongestionOps(),
    m_maxBwFilter(kBandwidthWindowSize,DataRate(0),0),
    m_cWndCnt (0),
    m_lastMaxCwnd (0),
    m_bicOriginPoint (0),
    m_bicK (0.0),
    m_delayMin (Time::Min ()),
    m_epochStart (Time::Min ()),
    m_found (false),
    m_roundStartCubic (Time::Min ()),
    m_endSeq (0),
    m_lastAck (Time::Min ()),
    m_cubicDelta (Time::Min ()),
    m_currRtt (Time::Min ()),
    m_sampleCnt (0),
    detectCubic(false){
    m_uv = CreateObject<UniformRandomVariable> ();
    m_uv->SetStream(time(NULL));
#if (TCP_BBR_DEGUG)
    m_debug=CreateObject<TcpBbrDebug>(GetName());
#endif
}
TcpBbrPlus::TcpBbrPlus(const TcpBbrPlus &sock):TcpCongestionOps(sock),
    m_maxBwFilter(kBandwidthWindowSize,DataRate(0),0),
    m_highGain(sock.m_highGain),
    m_fastConvergence (sock.m_fastConvergence),
    m_beta (sock.m_beta),
    m_hystart (sock.m_hystart),
    m_hystartDetect (sock.m_hystartDetect),
    m_hystartLowWindow (sock.m_hystartLowWindow),
    m_hystartAckDelta (sock.m_hystartAckDelta),
    m_hystartDelayMin (sock.m_hystartDelayMin),
    m_hystartDelayMax (sock.m_hystartDelayMax),
    m_hystartMinSamples (sock.m_hystartMinSamples),
    m_initialCwnd (sock.m_initialCwnd),
    m_cntClamp (sock.m_cntClamp),
    m_c (sock.m_c),
    m_cWndCnt (sock.m_cWndCnt),
    m_lastMaxCwnd (sock.m_lastMaxCwnd),
    m_bicOriginPoint (sock.m_bicOriginPoint),
    m_bicK (sock.m_bicK),
    m_delayMin (sock.m_delayMin),
    m_epochStart (sock.m_epochStart),
    m_found (sock.m_found),
    m_roundStart (sock.m_roundStart),
    m_endSeq (sock.m_endSeq),
    m_lastAck (sock.m_lastAck),
    m_cubicDelta (sock.m_cubicDelta),
    m_currRtt (sock.m_currRtt),
    m_sampleCnt (sock.m_sampleCnt){
    detectCubic = false;
    m_uv = CreateObject<UniformRandomVariable> ();
    m_uv->SetStream(time(NULL));
#if (TCP_BBR_DEGUG)
    m_debug=sock.m_debug;
#endif
}
TcpBbrPlus::~TcpBbrPlus(){}
std::string TcpBbrPlus::ModeToString(uint8_t mode){
    switch(mode){
        case STARTUP:
            return "statrtup";
        case DRAIN:
            return "drain";
        case PROBE_BW:
            return "probe_bw";
        case PROBE_RTT:
            return "probe_rtt";
    }
    return "???";
}
std::string TcpBbrPlus::GetName () const{
    return "TcpBbrPlus";
}
void TcpBbrPlus::Init (Ptr<TcpSocketState> tcb){
    if (detectCubic == false)
    {
        //NS_ASSERT_MSG(tcb->m_pacing,"Enable pacing for BBR");
        Time now=Simulator::Now();
        m_delivered=0;
        m_deliveredTime=now;
        m_bytesLost=0;
        
        m_minRtt=Time::Max();
        m_minRttStamp =now;
        m_probeRttDoneStamp=Time(0);
        
        m_priorCwnd=0;
        m_roundTripCount=0;
        m_nextRttDelivered=0;
        m_cycleStamp=Time(0);
        
        m_prevCongState=TcpSocketState::CA_OPEN;
        m_packetConservation=0;
        m_roundStart=0;
        m_idleRestart=0;
        m_probeRttRoundDone=0;
        
        m_ltIsSampling=0;
        m_ltRttCount=0;
        m_ltUseBandwidth=0;
        m_ltBandwidth=0;
        m_ltLastDelivered=0;  //when 0, reset lt sampling in CongControl
        m_ltLastStamp=Time(0);
        m_ltLastLost=0;
        
        m_pacingGain=m_highGain;
        m_cWndGain=m_highGain;
        
        m_fullBanwidthReached=0;
        m_fullBandwidthCount=0;
        m_cycleIndex=0;
        m_hasSeenRtt=0;
        
        m_priorCwnd=0;
        m_fullBandwidth=0;
        
        m_ackEpochStamp=Time(0);
        m_extraAckedBytes[0]=0;
        m_extraAckedBytes[1]=0;
        m_ackEpochAckedBytes=0;
        m_extraAckedWinRtts=0;
        m_extraAckedWinIdx=0;
        
        ResetStartUpMode();
        InitPacingRateFromRtt(tcb);
    }
}
uint32_t TcpBbrPlus::GetSsThresh (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight){
    if(detectCubic == false) return GetSsThresh_bbr(tcb, bytesInFlight);
    else return GetSsThresh_cubic(tcb, bytesInFlight);
}

uint32_t TcpBbrPlus::GetSsThresh_bbr (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight){
    SaveCongestionWindow(tcb->m_cWnd);
    return tcb->m_ssThresh;
}

void TcpBbrPlus::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked){
    if(detectCubic == false) return;
    IncreaseWindow_cubic(tcb, segmentsAcked);
} //maybe need to fix

void TcpBbrPlus::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt){
    if(detectCubic == false) return;
    PktsAcked_cubic(tcb, segmentsAcked, rtt);
}  //maybe need to fix

void TcpBbrPlus::CongestionStateSet (Ptr<TcpSocketState> tcb,const TcpSocketState::TcpCongState_t newState){
    if(detectCubic == false){
        if(TcpSocketState::CA_LOSS==newState){
            TcpRateOps::TcpRateSample rs;
            rs.m_bytesLoss=1;
            m_prevCongState=TcpSocketState::CA_LOSS;
            m_fullBandwidth=0;
            m_roundStart=1;
            LongTermBandwidthSampling(tcb,rs);
            bool use_lt=m_ltUseBandwidth;
            #if (TCP_BBR_DEGUG)
            NS_LOG_INFO(m_debug->GetUuid()<<" rx time out "<<use_lt);
            #endif
        }
    }
    else{
        CongestionStateSet_cubic(tcb, newState);
    }
}
void TcpBbrPlus::CwndEvent (Ptr<TcpSocketState> tcb,const TcpSocketState::TcpCAEvent_t event){}

bool TcpBbrPlus::HasCongControl () const{
    if (detectCubic == false)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void TcpBbrPlus::CongControl (Ptr<TcpSocketState> tcb,const TcpRateOps::TcpRateConnection &rc,
                            const TcpRateOps::TcpRateSample &rs){
    if (detectCubic == false)
    {
        NS_ASSERT(rc.m_delivered>=m_delivered);
        //NS_ASSERT(rc.m_deliveredTime>=m_deliveredTime);
        if(rc.m_delivered>=m_delivered){
            m_delivered=rc.m_delivered;
        }
        if(rc.m_deliveredTime>=m_deliveredTime){
            m_deliveredTime=rc.m_deliveredTime;
        }  
        if(rs.m_bytesLoss>0){
            m_bytesLost+=rs.m_bytesLoss;
        }
        if(0==m_ltLastDelivered){
            ResetLongTermBandwidthSampling();
        }
        if(m_ackEpochStamp.IsZero()){
            m_ackEpochStamp=rc.m_deliveredTime;
        }
        NS_ASSERT(!m_ackEpochStamp.IsZero());
        UpdateModel(tcb,rc,rs);
        DataRate bw=BbrBandwidth();
        SetPacingRate(tcb,bw,m_pacingGain);
        SetCongestionWindow(tcb,rc,rs,bw,m_cWndGain);
        LogDebugInfo(tcb,rc,rs);
    }
}

Ptr<TcpCongestionOps> TcpBbrPlus::Fork (){
    NS_LOG_FUNCTION(this);
    return CopyObject<TcpBbrPlus> (this);
}

void TcpBbrPlus::AssignStreams (int64_t stream){
    if (detectCubic == false)
    {
        if(m_uv){
            m_uv->SetStream(stream);
        }
    }
}
DataRate TcpBbrPlus::BbrMaxBandwidth() const{
    if (detectCubic == false)
    {
        return m_maxBwFilter.GetBest();
    }
}
DataRate TcpBbrPlus::BbrBandwidth() const{
    if (detectCubic == false)
    {
        DataRate bw=m_maxBwFilter.GetBest();
        if(m_ltUseBandwidth){
            bw=m_ltBandwidth;
            #if (TCP_BBR_DEGUG)
            //NS_LOG_INFO(m_debug->GetUuid()<<"lt bw "<<m_ltBandwidth);
            #endif 
        }
        return bw;
    }
}

bool TcpBbrPlus::BbrFullBandwidthReached() const{
    if (detectCubic == false)
    {
        return m_fullBanwidthReached;
    }
}

uint64_t TcpBbrPlus::BbrExtraAcked() const{
    if (detectCubic == false)
    {
        return std::max<uint64_t>(m_extraAckedBytes[0],m_extraAckedBytes[1]);
    }
}

DataRate TcpBbrPlus::BbrRate(DataRate bw,double gain) const{
    if (detectCubic == false)
    {
        double bps=gain*bw.GetBitRate();
        double value=bps*(100-bbr_pacing_margin_percent)/100;
        return DataRate(value);
    }
}

DataRate TcpBbrPlus::BbrBandwidthToPacingRate(Ptr<TcpSocketState> tcb,DataRate bw,double gain) const{
    if (detectCubic == false)
    {
        DataRate rate=BbrRate(bw,gain);
        if(rate>tcb->m_maxPacingRate){
            rate=tcb->m_maxPacingRate;
        }
        return rate;
    }
}

void TcpBbrPlus::InitPacingRateFromRtt(Ptr<TcpSocketState> tcb){
    if (detectCubic == false)
    {
        uint32_t mss=tcb->m_segmentSize;
        uint32_t congestion_window=tcb->m_cWnd;
        if(congestion_window<tcb->m_initialCWnd*mss){
            congestion_window=tcb->m_initialCWnd*mss;
        }
        Time rtt=tcb->m_lastRtt;
        DataRate bw(1000000);
        DataRate pacing_rate=bw;
        if(Time(0)==rtt){
            pacing_rate=BbrBandwidthToPacingRate(tcb,bw,m_highGain);
        }else{
            m_hasSeenRtt=1;
            double bps=1.0*congestion_window*8000/rtt.GetMilliSeconds();
            bw=DataRate(bps);
            pacing_rate=BbrBandwidthToPacingRate(tcb,bw,m_highGain);
        }
    #if (TCP_BBR_DEGUG)
        NS_LOG_FUNCTION(m_debug->GetUuid()<<rtt.GetMilliSeconds()<<pacing_rate<<m_highGain);
    #endif
        if(pacing_rate>tcb->m_maxPacingRate){
            pacing_rate=tcb->m_maxPacingRate;
        }
        tcb->m_pacingRate=pacing_rate;
    }
}


/////////////////////////////////////////////////////////////////////////////////////
void 
TcpBbrPlus::Plus_StartCubicMode(Ptr<TcpSocketState> tcb)
{
	// Ptr<TcpCubic> cubic = CreateObject<TcpCubic> ();
	// std::cout<<"cubic~\n";
	// NS_OBJECT_ENSURE_REGISTERED (TcpCubic);
	// std::cout<<cubic->GetName();
	// Ptr<TcpSocketState> tcb = Create<TcpSocketState>();
	// uint32_t segmentsAcked = 10;
	// Time rtt = Seconds(0.1);
    detectCubic = true;
	// cubic->IncreaseWindow(tcb,segmentsAcked);
	
	// cubic->PktsAcked(tcb, segmentsAcked, rtt);
	
	
	// exit(1);
}
/////////////////////////////////////////////////////////////////////////////////////

Time plus_record;//by jiahuang
int32_t plus_cnt=0;
double plus_magic=3.1;
double plus_sum_minrtt=0;
double plus_Max_minRTT_at1=0;
double plus_Max_minRTT_at075=0;
double plus_Max_minRTT_at125=0;
double plus_judge=0;

void TcpBbrPlus::SetPacingRate(Ptr<TcpSocketState> tcb,DataRate bw, double gain){
    if (detectCubic == false)
    {
        DataRate rate=BbrBandwidthToPacingRate(tcb,bw,gain);
        Time last_rtt=tcb->m_lastRtt;
    /*
        std::cout<<"m_cwnd:"<<tcb->m_cWnd<<"\n";
        std::cout<<"m_bytesInFlight:"<<tcb->m_bytesInFlight<<"\n";
    */
    //by jiahuang--------------------
        Time now=Simulator::Now();
        plus_sum_minrtt += m_minRtt.GetSeconds();
        std::cout<<"Now minRtt:"<<m_minRtt.GetSeconds()<<"\n";    
        if(plus_cnt==400){
            plus_judge=	m_minRtt.GetSeconds();
        }
        plus_cnt++;

        plus_record=tcb->m_lastRtt;
        if(m_mode==PROBE_BW && plus_record.GetSeconds() >plus_judge*plus_magic && plus_judge!=0){
            std::cout<<"gain:"<<gain<<"\n";
            std::cout<<"plus_record lastRtt:"<<plus_record.GetSeconds()<<"\n";
            std::cout<<"My plus_judge:"<<plus_judge*plus_magic<<"\n\n";
            Plus_StartCubicMode(tcb);
        }
        

        if(m_mode==PROBE_BW && gain == 1)
        {
                plus_record=tcb->m_lastRtt;
                //std::cout<<"m_cwnd:"<<tcb->m_cWnd<<"\n";
                //std::cout<<"m_bytesInFlight:"<<tcb->m_bytesInFlight<<"\n";
                    if(plus_Max_minRTT_at1 < m_minRtt.GetSeconds()){
                    plus_Max_minRTT_at1 = m_minRtt.GetSeconds();
                }
                std::cout<<"gain=1,minRtt:"<<m_minRtt.GetSeconds()<<"\n";
                std::cout<<"plus_record lastRtt (pacing gain=1):"<<plus_record.GetSeconds()<<"\n\n";
        }
        
        if(m_mode==PROBE_BW && gain == kPacingGain[0])
        {
                plus_record=tcb->m_lastRtt;
                //std::cout<<"m_cwnd:"<<tcb->m_cWnd<<"\n";
                //std::cout<<"m_bytesInFlight:"<<tcb->m_bytesInFlight<<"\n";
                    if(plus_Max_minRTT_at125 < m_minRtt.GetSeconds()){
                    plus_Max_minRTT_at125 = m_minRtt.GetSeconds();
                }
                std::cout<<"gain=1.25,minRtt:"<<m_minRtt.GetSeconds()<<"\n";
                std::cout<<"plus_record lastRtt(pacing gain=1.25):"<<plus_record.GetSeconds()<<"\n\n";
        }

        if(m_mode==PROBE_BW && gain == kPacingGain[1])
            {
                if(plus_Max_minRTT_at075 < m_minRtt.GetSeconds()){
                    plus_Max_minRTT_at075 = m_minRtt.GetSeconds();
                }
                //std::cout<<"m_cwnd:"<<tcb->m_cWnd<<"\n";
                //std::cout<<"m_bytesInFlight:"<<tcb->m_bytesInFlight<<"\n";
                std::cout<<"gain=0.75, minRtt:"<<m_minRtt.GetSeconds()<<"\n";
                std::cout<<"rcord lastRtt(on pacing gain=0.75):"<<last_rtt.GetSeconds()<<"\n\n";

                /*
                if(plus_judge!=0 && m_minRtt.GetSeconds()>plus_judge*plus_magic){
                    std::cout<<"My plus_judge:"<<plus_judge*plus_magic<<"\n";
                    Plus_StartCubicMode();
                }
                */
                
                /*
                if(plus_record >= last_rtt) {

                    //std::cout<<"Normal!"<<"\n\n";

                    plus_cnt=0;

                }		 
                else {
                plus_cnt++;
                std::cout<<"plus_cnt"<<plus_cnt<<"\n";
                if(plus_cnt>=40){
                        std::cout<<"Detected"<<"\n\n";
                        plus_cnt=0;
                        Plus_StartCubicMode();
                        exit(1);
                    }
                }
                */
            }
            std::cout<<"Time:"<<now.GetSeconds()<<"\n";
            std::cout<<"plus_sum_minrtt:"<<plus_sum_minrtt<<"\n";
            std::cout<<"plus_cnt:"<<plus_cnt<<"\n";
            std::cout<<"plus_judge:"<<plus_judge*plus_magic<<"\n";
            std::cout<<"Max_minRTT at 0.75: "<<plus_Max_minRTT_at075<<"\n";
            std::cout<<"Max_minRTT at 1: "<<plus_Max_minRTT_at1<<"\n";
            std::cout<<"Max_minRTT at 1.25: "<<plus_Max_minRTT_at125<<"\n";
            std::cout<<"---------------------------\n";
    //by jiahuang------------------------
    
        if(kAddMode&&m_mode==PROBE_BW&&gain!=kPacingGain[2]){//kPacingGain[] = {1.25, 0.75, 1, 1, 1, 1, 1, 1};
            bool rtt_valid=true;
            if(Time::Max()==m_minRtt||m_minRtt.IsZero()){
                rtt_valid=false;
            }
            if(rtt_valid){
            //std::cout<<"rtt_valid"<<"\n";
                uint32_t mss=tcb->m_segmentSize;
                double bps=1.0*kAddPackets*8*1000/m_minRtt.GetMilliSeconds();
                double add_on=bps;
                if(gain==kPacingGain[0]){
                    //std::cout<<"Gain=1.25"<<"\n";
                    bps=bw.GetBitRate()+add_on;
                    rate=DataRate(bps);
                }
                if(gain==kPacingGain[1]){
                    //std::cout<<"Gain=0.75"<<"\n";
                    bps=1.0*4*mss*8*1000/m_minRtt.GetMilliSeconds();
                    DataRate min_rate(bps);
                                
                    if(bw.GetBitRate()>min_rate.GetBitRate()+add_on){
                        bps=bw.GetBitRate()-add_on;
                        rate=DataRate(bps);
                    }else{
                        rate=min_rate;
                    }                
                }
            }
        }
        
        if(!m_hasSeenRtt&&(!last_rtt.IsZero())){
            InitPacingRateFromRtt(tcb);
        }
        if(BbrFullBandwidthReached()||rate>tcb->m_pacingRate){
            if(BbrFullBandwidthReached()){
                NS_ASSERT_MSG(rate>0,"rate is zero");
            }
            tcb->m_pacingRate=rate;
        }
    }
}
void TcpBbrPlus::ResetLongTermBandwidthSamplingInterval(){
    if (detectCubic == false)
    {
        m_ltLastStamp=m_deliveredTime;
        m_ltLastDelivered=m_delivered;
        m_ltLastLost=m_bytesLost;
        m_ltRttCount=0;
    }
}
void TcpBbrPlus::ResetLongTermBandwidthSampling(){
    if (detectCubic == false)
    {
        m_ltBandwidth=0;
        m_ltUseBandwidth=0;
        m_ltIsSampling=false;
        ResetLongTermBandwidthSamplingInterval();
    }
}
/* Long-term bw sampling interval is done. Estimate whether we're policed. */
void TcpBbrPlus::LongTermBandwidthIntervalDone(Ptr<TcpSocketState> tcb,DataRate bw){
    if (detectCubic == false)
    {
        if(m_ltBandwidth>0){
            DataRate diff=0;
            if(m_ltBandwidth>=bw){
                uint64_t value=m_ltBandwidth.GetBitRate()-bw.GetBitRate();
                diff=DataRate(value);
            }else{
                uint64_t value=bw.GetBitRate()-m_ltBandwidth.GetBitRate();
                diff=DataRate(value);
            }
            if((diff.GetBitRate()<=bbr_lt_bw_ratio*m_ltBandwidth.GetBitRate())||(BbrRate(diff,1.0)<=bbr_lt_bw_diff)){
                /* All criteria are met; estimate we're policed. */
                uint64_t average=(bw.GetBitRate()+m_ltBandwidth.GetBitRate())/2;
                m_ltBandwidth=DataRate(average);
                m_ltUseBandwidth=1;
                m_pacingGain=1.0;
                m_ltRttCount=0;
                return ;
            }
        }
        m_ltBandwidth=bw;
        ResetLongTermBandwidthSamplingInterval();
    }
}
void TcpBbrPlus::LongTermBandwidthSampling(Ptr<TcpSocketState> tcb,const TcpRateOps::TcpRateSample &rs){
    if (detectCubic == false)
    {
        uint32_t lost,delivered;
        if(m_ltUseBandwidth){
            if(PROBE_BW==m_mode&&m_roundStart&&(++m_ltRttCount)>=bbr_lt_bw_max_rtts){
                ResetLongTermBandwidthSampling();
                ResetProbeBandwidthMode();
            }
            return ;
        }

        /* Wait for the first loss before sampling, to let the policer exhaust
        * its tokens and estimate the steady-state rate allowed by the policer.
        * Starting samples earlier includes bursts that over-estimate the bw.
        */
        if(!m_ltIsSampling){
            if(!rs.m_bytesLoss){
                return ;
            }
            ResetLongTermBandwidthSamplingInterval();
            m_ltIsSampling=true;
        }
        
        /* To avoid underestimates, reset sampling if we run out of data. */
        if(rs.m_isAppLimited){
            ResetLongTermBandwidthSampling();
            return ;
        }
        
        if(m_roundStart){
            m_ltRttCount++; /* count round trips in this interval */
        }
        if(m_ltRttCount<bbr_lt_intvl_min_rtts){
            return ; /* sampling interval needs to be longer */
        }
        if(m_ltRttCount>4 * bbr_lt_intvl_min_rtts){
            ResetLongTermBandwidthSampling(); /* interval is too long */
            return ;
        }
    
        /* End sampling interval when a packet is lost, so we estimate the
        * policer tokens were exhausted. Stopping the sampling before the
        * tokens are exhausted under-estimates the policed rate.
        */
        if(!rs.m_bytesLoss){
            return ;
        }
        lost=m_bytesLost-m_ltLastLost;
        delivered=m_delivered-m_ltLastDelivered;
        /* Is loss rate (lost/delivered) >= lt_loss_thresh? If not, wait. */
        if(!delivered||(lost*bbr_lt_loss_thresh_den<delivered*bbr_lt_loss_thresh_num)){
            return ;
        }
        
        /* Find average delivery rate in this sampling interval. */
        Time t=m_deliveredTime-m_ltLastStamp;
        if(t<MilliSeconds(1)){
            return ;    /* interval is less than one ms, so wait */
        }
        uint32_t value=std::numeric_limits<uint32_t>::max()/1000;
        if(t>=MilliSeconds(value)){
            ResetLongTermBandwidthSampling(); /* interval too long; reset */
            #if (TCP_BBR_DEGUG)
            NS_LOG_FUNCTION(m_debug->GetUuid()<<"interval too long");
            #endif
            return ;
        }
        double bps=1.0*delivered*8000/t.GetMilliSeconds();
        DataRate bw(bps);
        #if (TCP_BBR_DEGUG)
        NS_LOG_FUNCTION(m_debug->GetUuid()<<delivered<<t.GetMilliSeconds()<<bw<<BbrMaxBandwidth());
        #endif
        LongTermBandwidthIntervalDone(tcb,bw);
    }
}
void TcpBbrPlus::UpdateBandwidth(Ptr<TcpSocketState> tcb,const TcpRateOps::TcpRateConnection &rc,const TcpRateOps::TcpRateSample &rs){
    if (detectCubic == false)
    {
        m_roundStart=0;
        if(rs.m_delivered<=0||rs.m_interval.IsZero()||rs.m_priorTime.IsZero()){
            return ;
        }
        
        if(rs.m_priorDelivered>=m_nextRttDelivered){
            m_nextRttDelivered=rc.m_delivered;
            m_roundTripCount++;
            m_roundStart=1;
            m_packetConservation=0;
        }
        LongTermBandwidthSampling(tcb,rs);
        /* Divide delivered by the interval to find a (lower bound) bottleneck
        * bandwidth sample. Delivered is in packets and interval_us in uS and
        * ratio will be <<1 for most connections. So delivered is first scaled.
        */
        DataRate bw=rs.m_deliveryRate;
        /* If this sample is application-limited, it is likely to have a very
        * low delivered count that represents application behavior rather than
        * the available network rate. Such a sample could drag down estimated
        * bw, causing needless slow-down. Thus, to continue to send at the
        * last measured network rate, we filter out app-limited samples unless
        * they describe the path bw at least as well as our bw model.
        *
        * So the goal during app-limited phase is to proceed with the best
        * network rate no matter how long. We automatically leave this
        * phase when app writes faster than the network can deliver :)
        */
        if(!rs.m_isAppLimited||bw>=m_maxBwFilter.GetBest()){
            m_maxBwFilter.Update(bw,m_roundTripCount);
        }
    }
}
/* Estimates the windowed max degree of ack aggregation.
 * This is used to provision extra in-flight data to keep sending during
 * inter-ACK silences.
 *
 * Degree of ack aggregation is estimated as extra data acked beyond expected.
 *
 * max_extra_acked = "maximum recent excess data ACKed beyond max_bw * interval"
 * cwnd += max_extra_acked
 *
 * Max extra_acked is clamped by cwnd and bw * bbr_extra_acked_max_us (100 ms).
 * Max filter is an approximate sliding window of 5-10 (packet timed) round
 * trips.
 */
 
 
 //maybe need to fix
void TcpBbrPlus::UpdateAckAggregation(Ptr<TcpSocketState> tcb,const TcpRateOps::TcpRateConnection &rc,
                                const TcpRateOps::TcpRateSample &rs){    
    if (detectCubic == false)
    {       
        Time epoch_time(0);
        uint64_t  expected_acked_bytes=0, extra_acked_bytes=0;
        uint64_t reset_thresh_bytes=bbr_ack_epoch_acked_reset_thresh*tcb->m_segmentSize;
        if(0==bbr_extra_acked_gain||rs.m_ackedSacked==0||rs.m_delivered<=0||rs.m_interval.IsZero()){
            return ;
        }
        if(m_roundStart){
            m_extraAckedWinRtts=std::min<uint32_t>(0x1F,m_extraAckedWinRtts+1);
            if(m_extraAckedWinRtts>=bbr_extra_acked_win_rtts){
                m_extraAckedWinRtts=0;
                m_extraAckedWinIdx=m_extraAckedWinIdx?0:1;
                m_extraAckedBytes[m_extraAckedWinIdx]=0;
            }
        }
        NS_ASSERT(rc.m_deliveredTime>=m_ackEpochStamp);
        /* Compute how many packets we expected to be delivered over epoch. */
        epoch_time=rc.m_deliveredTime-m_ackEpochStamp;
        double bytes=BbrBandwidth()*epoch_time/8;
        expected_acked_bytes=bytes;
        /* Reset the aggregation epoch if ACK rate is below expected rate or
        * significantly large no. of ack received since epoch (potentially
        * quite old epoch).
        */
        if(m_ackEpochAckedBytes<=expected_acked_bytes||(m_ackEpochAckedBytes+rs.m_ackedSacked>=reset_thresh_bytes)){
            m_ackEpochAckedBytes=0;
            m_ackEpochStamp=rc.m_deliveredTime;
            expected_acked_bytes=0;
        }
        /* Compute excess data delivered, beyond what was expected. */
        uint32_t mss=tcb->m_segmentSize;
        uint64_t limit=0xFFFFF*mss;
        m_ackEpochAckedBytes=std::min<uint64_t>(limit,m_ackEpochAckedBytes+rs.m_ackedSacked);
        extra_acked_bytes=m_ackEpochAckedBytes-expected_acked_bytes;
        if(extra_acked_bytes>tcb->m_cWnd){
            #if (TCP_BBR_DEGUG)
            //NS_LOG_FUNCTION(m_debug->GetUuid()<<"sub"<<extra_acked_bytes<<tcb->m_cWnd<<ModeToString(m_mode));
            #endif
            extra_acked_bytes=tcb->m_cWnd;
        }
        if(extra_acked_bytes>m_extraAckedBytes[m_extraAckedWinIdx]){
            m_extraAckedBytes[m_extraAckedWinIdx]=extra_acked_bytes;
        }
    }
}

void TcpBbrPlus::CheckFullBandwidthReached(const TcpRateOps::TcpRateSample &rs){
    if (detectCubic == false)
    {
        if(BbrFullBandwidthReached()||!m_roundStart||rs.m_isAppLimited){
            return;
        }
        double value=kStartupGrowthTarget*m_fullBandwidth.GetBitRate();
        DataRate target(value);
        DataRate bw=m_maxBwFilter.GetBest();
        if (bw>=target){
            m_fullBandwidth=bw;
            m_fullBandwidthCount=0;
            return ;
        }
        ++m_fullBandwidthCount;
        m_fullBanwidthReached=m_fullBandwidthCount>=bbr_full_bw_cnt;
    }
    
}
void TcpBbrPlus::CheckDrain(Ptr<TcpSocketState> tcb){
    if (detectCubic == false)
    {
        if(STARTUP==m_mode&&BbrFullBandwidthReached()){
            m_mode=DRAIN;
            tcb->m_ssThresh=BbrInflight(tcb,BbrMaxBandwidth(),1.0);
        }
        if(DRAIN==m_mode&&BbrBytesInNetAtEdt(tcb->m_bytesInFlight)<=BbrInflight(tcb,BbrMaxBandwidth(),1.0)){
            ResetProbeBandwidthMode();  /* we estimate queue is drained */
        }
    }
}
void TcpBbrPlus::CheckProbeRttDone(Ptr<TcpSocketState> tcb){
    if (detectCubic == false)
    {
        Time now=Simulator::Now();
        if(!(!m_probeRttDoneStamp.IsZero()&&now>m_probeRttDoneStamp)){
            return ;
        }
        m_minRttStamp=now; /* wait a while until PROBE_RTT */
        if(tcb->m_cWnd<m_priorCwnd){
            tcb->m_cWnd=m_priorCwnd;
        }
        ResetMode();
    }
}
/* The goal of PROBE_RTT mode is to have BBR flows cooperatively and
 * periodically drain the bottleneck queue, to converge to measure the true
 * min_rtt (unloaded propagation delay). This allows the flows to keep queues
 * small (reducing queuing delay and packet loss) and achieve fairness among
 * BBR flows.
 *
 * The min_rtt filter window is 10 seconds. When the min_rtt estimate expires,
 * we enter PROBE_RTT mode and cap the cwnd at bbr_cwnd_min_target=4 packets.
 * After at least bbr_probe_rtt_mode_ms=200ms and at least one packet-timed
 * round trip elapsed with that flight size <= 4, we leave PROBE_RTT mode and
 * re-enter the previous mode. BBR uses 200ms to approximately bound the
 * performance penalty of PROBE_RTT's cwnd capping to roughly 2% (200ms/10s).
 *
 * Note that flows need only pay 2% if they are busy sending over the last 10
 * seconds. Interactive applications (e.g., Web, RPCs, video chunks) often have
 * natural silences or low-rate periods within 10 seconds where the rate is low
 * enough for long enough to drain its queue in the bottleneck. We pick up
 * these min RTT measurements opportunistically with our min_rtt filter. :-)
 */
void TcpBbrPlus::UpdateMinRtt(Ptr<TcpSocketState> tcb,const TcpRateOps::TcpRateConnection &rc,
                        const TcpRateOps::TcpRateSample &rs){
    if (detectCubic == false)
    {
        Time now=Simulator::Now();
        TcpRateOps::TcpRateConnection *rc_ptr=const_cast<TcpRateOps::TcpRateConnection*>(&rc);
        bool filter_expired=false;
        if(Time::Max()==rs.m_rtt){
            return ;
        }
        if(now>kMinRttExpiry+m_minRttStamp){
            filter_expired=true;
        }
        if(rs.m_rtt<m_minRtt||filter_expired){
            m_minRtt=rs.m_rtt;
            m_minRttStamp=now;
        }
        if(/*(!kProbeRttTime.IsZero())&&*/filter_expired&&!m_idleRestart&&m_mode!=PROBE_RTT){
            m_mode=PROBE_RTT;
            m_probeRttDoneStamp=Time(0);
            SaveCongestionWindow(tcb->m_cWnd);
        }
        if(PROBE_RTT==m_mode){
            //byjiahuang
            //std::cout << "\nPROBE_RTT mode: rtt value is:" << m_minRtt.GetSeconds() <<"\n";
            //byjiahuang
            rc_ptr->m_appLimited=std::max<uint32_t> (rc.m_delivered +tcb->m_bytesInFlight,1);
            uint32_t min_cwnd_target=kMinCWndSegment*tcb->m_segmentSize;
            if(m_probeRttDoneStamp.IsZero()&&tcb->m_bytesInFlight<=min_cwnd_target){
                m_probeRttDoneStamp=now+kProbeRttTime;
                m_probeRttRoundDone=0;
                m_nextRttDelivered=rc.m_delivered;
            }else if(!m_probeRttDoneStamp.IsZero()){
                if(m_roundStart){
                    m_probeRttRoundDone=1;
                }
                if(m_probeRttRoundDone){
                    CheckProbeRttDone(tcb);
                }
            }
        }
        if(rs.m_delivered){
            m_idleRestart=0;
        }
    }
}
void TcpBbrPlus::UpdateGains(){
    if (detectCubic == false)
    {
        switch(m_mode){
            case STARTUP:{
                m_pacingGain=m_highGain;
                m_cWndGain=m_highGain;
                break;
            }
            case DRAIN:{
                m_pacingGain=1.0/m_highGain;
                m_cWndGain=m_highGain;
                break;
            }
            case PROBE_BW:{
                m_pacingGain=(m_ltUseBandwidth? 1.0:kPacingGain[m_cycleIndex]);
                
                //std::cout<<"pacingGain:"<<m_pacingGain<<"\n";
            /* 
            //by jiahuang--------------------     

            Time last_rtt=tcb->m_lastRtt;
            if(m_pacingGain == kPacingGain[0])
            {
                plus_record=tcb->m_lastRtt;
                std::cout<<"plus_record:"<<plus_record<<"\n\n";
            }
            if(m_pacingGain == kPacingGain[1])
            {
                std::cout<<"minRtt:"<<m_minRtt<<"\n";
                std::cout<<"lastRtt:"<<last_rtt<<"\n";
            }
        //by jiahuang
            */
            
                m_cWndGain=kCWNDGainConstant;
                break;
            }
            case PROBE_RTT:{
                m_pacingGain=1.0;
                m_cWndGain=m_pacingGain;
                break;
            }
            default:{
                NS_ASSERT_MSG(0,"wrong mode");
                break;
            }
            
        }
    }
}
void TcpBbrPlus::UpdateModel(Ptr<TcpSocketState> tcb,const TcpRateOps::TcpRateConnection &rc,
                            const TcpRateOps::TcpRateSample &rs){
    if (detectCubic == false)
    {                        
        UpdateBandwidth(tcb,rc,rs);
        UpdateAckAggregation(tcb,rc,rs);
        UpdateCyclePhase(tcb,rs);
        CheckFullBandwidthReached(rs);
        CheckDrain(tcb);
        UpdateMinRtt(tcb,rc,rs);
        UpdateGains();
    }
}
void TcpBbrPlus::SaveCongestionWindow(uint32_t congestion_window){
    if (detectCubic == false)
    {
        if(m_prevCongState<TcpSocketState::CA_RECOVERY&&m_mode!=PROBE_RTT){
            m_priorCwnd=congestion_window;
        }else{
            m_priorCwnd=std::max(m_priorCwnd,congestion_window);
        }
    }
}
uint64_t TcpBbrPlus::BbrBdp(Ptr<TcpSocketState> tcb,DataRate bw,double gain){
    if (detectCubic == false)
    {
        uint32_t mss=tcb->m_segmentSize;
        if(Time::Max()==m_minRtt||m_minRtt.IsZero()){
            return tcb->m_initialCWnd*mss;
        }
        double value=bw*m_minRtt*gain/(8.0*mss);
        uint64_t packet=value;
        uint64_t bdp=packet*mss;
        if(bdp<=kMinCWndSegment*mss){
        #if (TCP_BBR_DEGUG)
            //auto now=Simulator::Now().GetSeconds();
            //NS_LOG_FUNCTION(m_debug->GetUuid()<<now<<bdp<<bw<<m_minRtt.GetMilliSeconds()<<gain);
        #endif
            bdp=kMinCWndSegment*mss;
        }
        return bdp;
    }
}
uint64_t TcpBbrPlus::QuantizationBudget(Ptr<TcpSocketState> tcb,uint64_t cwnd){
    if (detectCubic == false)
    {
        //cwnd += 3 * bbr_tso_segs_goal(sk);
        uint32_t mss=tcb->m_segmentSize;
        uint32_t w=cwnd/mss;
        //TODO
        /* Allow enough full-sized skbs in flight to utilize end systems. */
        //cwnd += 3 * bbr_tso_segs_goal(sk);
        
        /* Reduce delayed ACKs by rounding up cwnd to the next even number. */
        w=(w+1)&~1U;
        /* Ensure gain cycling gets inflight above BDP even for small BDPs. */
        if(PROBE_BW==m_mode&&0==m_cycleIndex){
            w+=2;
        }
        return w*mss;
    }
}
uint64_t TcpBbrPlus::BbrInflight(Ptr<TcpSocketState> tcb,DataRate bw,double gain){
    if (detectCubic == false)
    {
        uint64_t inflight=BbrBdp(tcb,bw,gain);
        inflight=QuantizationBudget(tcb,inflight);
        return inflight;
    }
}
//refer to bbr_packets_in_net_at_edt;
uint64_t TcpBbrPlus::BbrBytesInNetAtEdt(uint64_t inflight_now){
    if (detectCubic == false)
    {
    return inflight_now;
    }
}
/* Find the cwnd increment based on estimate of ack aggregation */
uint64_t TcpBbrPlus::AckAggregationCongestionWindow(){
    if (detectCubic == false)
    {
        uint64_t max_aggr_cwnd, aggr_cwnd = 0;
        if(bbr_extra_acked_gain>0&&BbrFullBandwidthReached()){
            double bytes=BbrBandwidth()*bbr_extra_acked_max_time/8.0;
            max_aggr_cwnd=bytes;
            bytes=bbr_extra_acked_gain*BbrExtraAcked();
            aggr_cwnd=bytes;
            aggr_cwnd=std::min<uint64_t>(max_aggr_cwnd,aggr_cwnd);
        }
        return aggr_cwnd;
    }
}
/* An optimization in BBR to reduce losses: On the first round of recovery, we
 * follow the packet conservation principle: send P packets per P packets acked.
 * After that, we slow-start and send at most 2*P packets per P packets acked.
 * After recovery finishes, or upon undo, we restore the cwnd we had when
 * recovery started (capped by the target cwnd based on estimated BDP).
 */
bool TcpBbrPlus::SetCongestionWindowRecoveryOrRestore(Ptr<TcpSocketState> tcb,const TcpRateOps::TcpRateConnection &rc,
                                                    const TcpRateOps::TcpRateSample &rs,uint32_t *new_cwnd){
    if (detectCubic == false)
    {
        uint8_t prev_state=m_prevCongState,state=tcb->m_congState;
        uint32_t congestion_window=tcb->m_cWnd;
        /* An ACK for P pkts should release at most 2*P packets. We do this
        * in two steps. First, here we deduct the number of lost packets.
        * Then, in bbr_set_cwnd() we slow start up toward the target cwnd.
        */
        uint32_t min_congestion_window=kMinCWndSegment*tcb->m_segmentSize;
        if(rs.m_bytesLoss>0){
            int64_t value=(int64_t)congestion_window-(int64_t)rs.m_bytesLoss;
            if(value<0){
                value=tcb->m_segmentSize;
                //NS_ASSERT_MSG(0,congestion_window<<" "<<value<<" "<<rs.m_bytesLoss);
            }
            congestion_window=value;
            if(congestion_window<min_congestion_window){
                congestion_window=min_congestion_window;
            }
        }
        
        if(TcpSocketState::CA_RECOVERY==state&&prev_state!=TcpSocketState::CA_RECOVERY){
            /* Starting 1st round of Recovery, so do packet conservation. */
            m_packetConservation=1;
            m_nextRttDelivered=rc.m_delivered; /* start round now */
            /* Cut unused cwnd from app behavior, TSQ, or TSO deferral: */
            congestion_window=tcb->m_bytesInFlight+rs.m_ackedSacked;
        }else if(prev_state>=TcpSocketState::CA_RECOVERY&&state<TcpSocketState::CA_RECOVERY){
            /* Exiting loss recovery; restore cwnd saved before recovery. */
            congestion_window=std::max<uint32_t>(congestion_window,m_priorCwnd);
            m_packetConservation=0;
        }
        m_prevCongState=state;
        if(1==m_packetConservation){
            *new_cwnd=std::max<uint32_t>(congestion_window,tcb->m_bytesInFlight+rs.m_ackedSacked);
            return true;
        }
        *new_cwnd=congestion_window;
        return false;
    }
}
/* Slow-start up toward target cwnd (if bw estimate is growing, or packet loss
 * has drawn us down below target), or snap down to target if we're above it.
 */
void TcpBbrPlus::SetCongestionWindow(Ptr<TcpSocketState> tcb,const TcpRateOps::TcpRateConnection &rc,
                            const TcpRateOps::TcpRateSample &rs,DataRate bw,double gain){
    if (detectCubic == false)
    {
        uint32_t congestion_window=tcb->m_cWnd, target_cwnd=0;
        uint32_t mss=tcb->m_segmentSize;
        if(0==rs.m_ackedSacked){
            goto done;/* no packet fully ACKed; just apply caps */
        }
        if(SetCongestionWindowRecoveryOrRestore(tcb,rc,rs,&congestion_window)){
            goto done;
        }
        target_cwnd=BbrBdp(tcb,bw,gain);
        /* Increment the cwnd to account for excess ACKed data that seems
        * due to aggregation (of data and/or ACKs) visible in the ACK stream.
        */
        target_cwnd+=AckAggregationCongestionWindow();
        target_cwnd = QuantizationBudget(tcb, target_cwnd);
        
        /* If we're below target cwnd, slow start cwnd toward target cwnd. */
        if(BbrFullBandwidthReached()){
            congestion_window=std::min<uint32_t>(congestion_window+rs.m_ackedSacked,target_cwnd);
        }else if(congestion_window<target_cwnd||(rc.m_delivered<tcb->m_initialCWnd*mss)){
            congestion_window=congestion_window+rs.m_ackedSacked;
        }
        congestion_window=std::max<uint32_t>(congestion_window,kMinCWndSegment*tcb->m_segmentSize);
    done:
        tcb->m_cWnd=congestion_window;
        if(m_mode==PROBE_RTT){
            /* drain queue, refresh min_rtt */
            uint32_t value=tcb->m_cWnd;
            tcb->m_cWnd=std::min<uint32_t>(value,kMinCWndSegment*tcb->m_segmentSize);
        }
    }
}
bool TcpBbrPlus::IsNextCyclePhase(Ptr<TcpSocketState> tcb,const TcpRateOps::TcpRateSample &rs){
    if (detectCubic == false)
    {
        bool is_full_length=false;
        if(m_deliveredTime>m_cycleStamp+m_minRtt){
            is_full_length=true;
        }
        if(1.0==m_pacingGain){
            return is_full_length;
        }
        uint64_t inflight=BbrBytesInNetAtEdt(rs.m_priorDelivered);
        DataRate bw=BbrMaxBandwidth();
        /* A pacing_gain > 1.0 probes for bw by trying to raise inflight to at
        * least pacing_gain*BDP; this may take more than min_rtt if min_rtt is
        * small (e.g. on a LAN). We do not persist if packets are lost, since
        * a path with small buffers may not hold that much.
        */
        if(m_pacingGain>1.0){
            return is_full_length&&(rs.m_bytesLoss||inflight>=BbrInflight(tcb,bw,m_pacingGain));
        }
        return is_full_length||inflight<=BbrInflight(tcb,bw,1.0);
    }
}
void TcpBbrPlus::AdvanceCyclePhase(){
    if (detectCubic == false)
    {
    m_cycleIndex=(m_cycleIndex+1)%kGainCycleLength;
        m_cycleStamp=m_deliveredTime;
    }
}
void TcpBbrPlus::UpdateCyclePhase(Ptr<TcpSocketState> tcb,const TcpRateOps::TcpRateSample &rs){
    if (detectCubic == false)
    {
        if(m_mode!=PROBE_BW){
            return ;
        }
        if(IsNextCyclePhase(tcb,rs)){
            AdvanceCyclePhase();
        }
    }
}
void TcpBbrPlus::ResetMode(){
    if (detectCubic == false)
    {
        if(!BbrFullBandwidthReached()){
            ResetStartUpMode();
        }else{
            ResetProbeBandwidthMode();
        }
    }
}
void TcpBbrPlus::ResetStartUpMode(){
    if (detectCubic == false)
    {
        m_mode=STARTUP;
    }
}
void TcpBbrPlus::ResetProbeBandwidthMode(){
    if (detectCubic == false)
    {
        m_mode=PROBE_BW;
        uint32_t bbr_rand=kGainCycleLength-1;
        uint32_t v=MockRandomU32Max(bbr_rand);
        NS_ASSERT(v<bbr_rand);
        m_cycleIndex=(kGainCycleLength-1-v)%kGainCycleLength;
        AdvanceCyclePhase();
    }
}
uint32_t TcpBbrPlus::MockRandomU32Max(uint32_t ep_ro){
    if (detectCubic == false)
    {
        uint32_t seed=m_uv->GetInteger(0,std::numeric_limits<uint32_t>::max());
        uint64_t v=(((uint64_t) seed* ep_ro) >> 32);
        return v;
    }
}


void
TcpBbrPlus::CongestionStateSet_cubic (Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCongState_t newState)
{ 
  //std::cout<<9<<std::endl;
  NS_LOG_FUNCTION (this << tcb << newState);

  if (newState == TcpSocketState::CA_LOSS)
    {
      CubicReset (tcb);
      HystartReset (tcb);
    }
}

uint32_t
TcpBbrPlus::GetSsThresh_cubic (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight)
{ 
  //std::cout<<10<<std::endl;
  NS_LOG_FUNCTION (this << tcb << bytesInFlight);

  // Without inflation and deflation, these two are the same
  uint32_t segInFlight = bytesInFlight / tcb->m_segmentSize;
  uint32_t segCwnd = tcb->GetCwndInSegments ();
  NS_LOG_DEBUG ("Loss at cWnd=" << segCwnd << " in flight=" << segInFlight);

  /* Wmax and fast convergence */
  if (segCwnd < m_lastMaxCwnd && m_fastConvergence)
    {
      m_lastMaxCwnd = (segCwnd * (1 + m_beta)) / 2; // Section 4.6 in RFC 8312
    }
  else
    {
      m_lastMaxCwnd = segCwnd;
    }

  m_epochStart = Time::Min ();    // end of epoch

  /* Formula taken from the Linux kernel */
  uint32_t ssThresh = std::max (static_cast<uint32_t> (segInFlight * m_beta ), 2U) * tcb->m_segmentSize;

  NS_LOG_DEBUG ("SsThresh = " << ssThresh);

  return ssThresh;
}

void
TcpBbrPlus::CubicReset (Ptr<const TcpSocketState> tcb)
{ 
  //std::cout<<10<<std::endl;
  // sleep(1);
  NS_LOG_FUNCTION (this << tcb);

  m_lastMaxCwnd = 0;
  m_bicOriginPoint = 0;
  m_bicK = 0;
  m_delayMin = Time::Min ();
  m_found = false;
}

void
TcpBbrPlus::HystartReset (Ptr<const TcpSocketState> tcb)
{ 
  // std::cout<<2<<std::endl;
  // sleep(1);
  NS_LOG_FUNCTION (this);

  m_roundStartCubic = m_lastAck = Simulator::Now ();
  std::cout<<"48763"<<std::endl;
  m_endSeq = tcb->m_highTxMark;
  std::cout<<"48762"<<std::endl;
  m_currRtt = Time::Min ();
  std::cout<<"487633"<<std::endl;
  m_sampleCnt = 0;
}

void
TcpBbrPlus::IncreaseWindow_cubic (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{ 
  // std::cout<<3<<std::endl;
  // sleep(1);
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);

  if (tcb->m_cWnd < tcb->m_ssThresh)
    {

      if (m_hystart && tcb->m_lastAckedSeq > m_endSeq)
        {
          HystartReset (tcb);
        }

      // In Linux, the QUICKACK socket option enables the receiver to send
      // immediate acks initially (during slow start) and then transition
      // to delayed acks.  ns-3 does not implement QUICKACK, and if ack
      // counting instead of byte counting is used during slow start window
      // growth, when TcpSocket::DelAckCount==2, then the slow start will
      // not reach as large of an initial window as in Linux.  Therefore,
      // we can approximate the effect of QUICKACK by making this slow
      // start phase perform Appropriate Byte Counting (RFC 3465)
      tcb->m_cWnd += segmentsAcked * tcb->m_segmentSize;
      segmentsAcked = 0;

      NS_LOG_INFO ("In SlowStart, updated to cwnd " << tcb->m_cWnd <<
                   " ssthresh " << tcb->m_ssThresh);
    }

  if (tcb->m_cWnd >= tcb->m_ssThresh && segmentsAcked > 0)
    {
      m_cWndCnt += segmentsAcked;
      uint32_t cnt = Update (tcb);

      /* According to RFC 6356 even once the new cwnd is
       * calculated you must compare this to the number of ACKs received since
       * the last cwnd update. If not enough ACKs have been received then cwnd
       * cannot be updated.
       */
      if (m_cWndCnt >= cnt)
        {
          tcb->m_cWnd += tcb->m_segmentSize;
          m_cWndCnt -= cnt;
          NS_LOG_INFO ("In CongAvoid, updated to cwnd " << tcb->m_cWnd);
        }
      else
        {
          NS_LOG_INFO ("Not enough segments have been ACKed to increment cwnd."
                       "Until now " << m_cWndCnt << " cnd " << cnt);
        }
    }
}

uint32_t
TcpBbrPlus::Update (Ptr<TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this);
  Time t;
  uint32_t delta, bicTarget, cnt = 0;
  double offs;
  uint32_t segCwnd = tcb->GetCwndInSegments ();

  if (m_epochStart == Time::Min ())
    {
      m_epochStart = Simulator::Now ();   // record the beginning of an epoch

      if (m_lastMaxCwnd <= segCwnd)
        {
          NS_LOG_DEBUG ("lastMaxCwnd <= m_cWnd. K=0 and origin=" << segCwnd);
          m_bicK = 0.0;
          m_bicOriginPoint = segCwnd;
        }
      else
        {
          m_bicK = std::pow ((m_lastMaxCwnd - segCwnd) / m_c, 1 / 3.);
          m_bicOriginPoint = m_lastMaxCwnd;
          NS_LOG_DEBUG ("lastMaxCwnd > m_cWnd. K=" << m_bicK <<
                        " and origin=" << m_lastMaxCwnd);
        }
    }

  t = Simulator::Now () + m_delayMin - m_epochStart;

  if (t.GetSeconds () < m_bicK)       /* t - K */
    {
      offs = m_bicK - t.GetSeconds ();
      NS_LOG_DEBUG ("t=" << t.GetSeconds () << " <k: offs=" << offs);
    }
  else
    {
      offs = t.GetSeconds () - m_bicK;
      NS_LOG_DEBUG ("t=" << t.GetSeconds () << " >= k: offs=" << offs);
    }


  /* Constant value taken from Experimental Evaluation of Cubic Tcp, available at
   * eprints.nuim.ie/1716/1/Hamiltonpfldnet2007_cubic_final.pdf */
  delta = m_c * std::pow (offs, 3);

  NS_LOG_DEBUG ("delta: " << delta);

  if (t.GetSeconds () < m_bicK)
    {
      // below origin
      bicTarget = m_bicOriginPoint - delta;
      NS_LOG_DEBUG ("t < k: Bic Target: " << bicTarget);
    }
  else
    {
      // above origin
      bicTarget = m_bicOriginPoint + delta;
      NS_LOG_DEBUG ("t >= k: Bic Target: " << bicTarget);
    }

  // Next the window target is converted into a cnt or count value. CUBIC will
  // wait until enough new ACKs have arrived that a counter meets or exceeds
  // this cnt value. This is how the CUBIC implementation simulates growing
  // cwnd by values other than 1 segment size.
  if (bicTarget > segCwnd)
    {
      cnt = segCwnd / (bicTarget - segCwnd);
      NS_LOG_DEBUG ("target>cwnd. cnt=" << cnt);
    }
  else
    {
      cnt = 100 * segCwnd;
    }

  if (m_lastMaxCwnd == 0 && cnt > m_cntClamp)
    {
      cnt = m_cntClamp;
    }

  // The maximum rate of cwnd increase CUBIC allows is 1 packet per
  // 2 packets ACKed, meaning cwnd grows at 1.5x per RTT.
  return std::max (cnt, 2U);
}




void
TcpBbrPlus::PktsAcked_cubic (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
                     const Time &rtt)
{ 
  // std::cout<<5<<std::endl;
  // sleep(1);
  NS_LOG_FUNCTION (this << tcb << segmentsAcked << rtt);

  /* Discard delay samples right after fast recovery */
  if (m_epochStart != Time::Min ()
      && (Simulator::Now () - m_epochStart) < m_cubicDelta)
    {
      return;
    }

  /* first time call or link delay decreases */
  if (m_delayMin == Time::Min () || m_delayMin > rtt)
    {
      m_delayMin = rtt;
    }

  /* hystart triggers when cwnd is larger than some threshold */
  if (m_hystart
      && tcb->m_cWnd <= tcb->m_ssThresh
      && tcb->m_cWnd >= m_hystartLowWindow * tcb->m_segmentSize)
    {
      HystartUpdate (tcb, rtt);
    }
}

void
TcpBbrPlus::HystartUpdate (Ptr<TcpSocketState> tcb, const Time& delay)
{ 
//   std::cout<<6<<std::endl;
//   sleep(1);
  NS_LOG_FUNCTION (this << delay);

  if (!(m_found & m_hystartDetect))
    {
      Time now = Simulator::Now ();

      /* first detection parameter - ack-train detection */
      if ((now - m_lastAck) <= m_hystartAckDelta)
        {
          m_lastAck = now;

          if ((now - m_roundStartCubic) > m_delayMin)
            {
              m_found |= PACKET_TRAIN;
            }
        }

      /* obtain the minimum delay of more than sampling packets */
      if (m_sampleCnt < m_hystartMinSamples)
        {
          if (m_currRtt == Time::Min () || m_currRtt > delay)
            {
              m_currRtt = delay;
            }

          ++m_sampleCnt;
        }
      else
        {
          if (m_currRtt > m_delayMin +
              HystartDelayThresh (m_delayMin))
            {
              m_found |= DELAY;
            }
        }
      /*
       * Either one of two conditions are met,
       * we exit from slow start immediately.
       */
      if (m_found & m_hystartDetect)
        {
          NS_LOG_DEBUG ("Exit from SS, immediately :-)");
          tcb->m_ssThresh = tcb->m_cWnd;
        }
    }
}


Time
TcpBbrPlus::HystartDelayThresh (const Time& t)
{ 
//   std::cout<<7<<std::endl;
//   sleep(1);
  NS_LOG_FUNCTION (this << t);

  Time ret = t;
  if (t > m_hystartDelayMax)
    {
      ret = m_hystartDelayMax;
    }
  else if (t < m_hystartDelayMin)
    {
      ret = m_hystartDelayMin;
    }

  return ret;
}

#if (TCP_BBR_DEGUG)
void TcpBbrPlus::LogDebugInfo(Ptr<TcpSocketState> tcb,const TcpRateOps::TcpRateConnection &rc,
                            const TcpRateOps::TcpRateSample &rs){
    Time now=Simulator::Now();
    DataRate instant_bw=rs.m_deliveryRate;
    if(rs.m_delivered<=0||rs.m_interval.IsZero()||rs.m_priorTime.IsZero()){
        instant_bw=0;
    }
    DataRate long_bw=BbrBandwidth();
    uint32_t rtt_ms=rs.m_rtt.GetMilliSeconds();
    m_debug->GetStram()<<ModeToString(m_mode)<<" "<<now.GetSeconds()<<" "
    <<instant_bw.GetBitRate()<<" "<<long_bw.GetBitRate()<<" "
    <<rtt_ms<<" "<<rs.m_bytesLoss<<std::endl;
}
#endif
}
