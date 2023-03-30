#ifndef TCPBBRPLUS_H
#define TCPBBRPLUS_H
#include <iostream>
#include <fstream>
#include <string>
#include "ns3/tcp-congestion-ops.h"
#include "ns3/traced-value.h"
#include "ns3/data-rate.h"
#include "ns3/random-variable-stream.h"
#include "ns3/windowed-filter.h"

namespace ns3{
#define TCP_BBR_DEGUG 1
#define FUNC_INLINE
class TcpBbrDebug;
class TcpBbrPlus: public TcpCongestionOps{
public:
    static void SetAddMode(bool enable);
    static void SetAddOn(uint32_t packets);
    enum Mode {
        // Startup phase of the connection.
        STARTUP,
        // After achieving the highest possible bandwidth during the startup, lower
        // the pacing rate in order to drain the queue.
        DRAIN,
        // Cruising mode.
        PROBE_BW,
        // Temporarily slow down sending in order to empty the buffer and measure
        // the real minimum RTT.
        PROBE_RTT,
    };
    enum HybridSSDetectionMode
    {
        PACKET_TRAIN = 0x1, //!< Detection by trains of packet
        DELAY        = 0x2  //!< Detection by delay value
    };
    /**
    * \brief Get the type ID.
    * \return the object TypeId
    */
    static TypeId GetTypeId (void);
    
    /**
    * \brief Constructor
    */
    TcpBbrPlus ();
    /**
    * Copy constructor.
    * \param sock The socket to copy from.
    */
    TcpBbrPlus (const TcpBbrPlus &sock);
    ~TcpBbrPlus();
    static std::string ModeToString(uint8_t mode);
    virtual std::string GetName () const;
    virtual void Init (Ptr<TcpSocketState> tcb);
    virtual uint32_t GetSsThresh (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight);
    virtual void IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);
    virtual void PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt);
    virtual void CongestionStateSet (Ptr<TcpSocketState> tcb,const TcpSocketState::TcpCongState_t newState);
    virtual void CwndEvent (Ptr<TcpSocketState> tcb,const TcpSocketState::TcpCAEvent_t event);
    virtual bool HasCongControl () const;
    virtual void CongControl (Ptr<TcpSocketState> tcb,const TcpRateOps::TcpRateConnection &rc,
                            const TcpRateOps::TcpRateSample &rs);

    
    
    virtual Ptr<TcpCongestionOps> Fork ();
    /**
    * Assign a fixed random variable stream number to the random variables
    * used by this model.  Return the number of streams (possibly zero) that
    * have been assigned.
    *
    * \param stream first stream index to use
    * \return the number of stream indices assigned by this model
    */
    virtual void AssignStreams (int64_t stream);
    DataRate BbrMaxBandwidth() const;
    // cubic variable
    uint32_t     m_lastMaxCwnd;     //!<  Last maximum cWnd
    uint32_t     m_bicOriginPoint;  //!<  Origin point of bic function
    double       m_bicK;            //!<  Time to origin point from the beginning
                                  //    of the current epoch (in s)
    Time         m_delayMin;        //!<  Min delay
    bool         m_found;           //!<  The exit point is found?
    Time         m_lastAck;         //!<  Last time when the ACK spacing is close
    Time         m_roundStart_cubic; // 新增出來的Time 形別，因為bbr的型別是unit32
    SequenceNumber32   m_endSeq;    //!<  End sequence of the round
    Time         m_currRtt;         //!<  Current Rtt
    uint32_t     m_sampleCnt;       //!<  Count of samples for HyStart
    bool     m_hystart; 
    uint32_t     m_cWndCnt;         //!<  cWnd integer-to-float counter
    Time         m_epochStart;      //!<  Beginning of an epoch
    double   m_c;                //!< Cubic Scaling factor
    uint8_t  m_cntClamp;         //!< Modulo of the (avoided) float division for cWnd
    Time         m_cubicDelta;      //!<  Time to wait after recovery before update
    uint32_t m_hystartLowWindow; //!< Lower bound cWnd for hybrid slow start (segments)
    int      m_hystartDetect;    //!< Detect way for HyStart algorithm \see HybridSSDetectionMode
    Time     m_hystartAckDelta;  //!< Spacing between ack's indicating train
    uint8_t  m_hystartMinSamples; //!< Number of delay samples for detecting the increase of delay
    Time     m_hystartDelayMax;  //!< Maximum time for hystart algorithm
    Time     m_hystartDelayMin;  //!< Minimum time for hystart algorithm
    // Time         m_roundStart_cubic;      //!<  Beginning of each round

    // cubic 
    void Plus_StartCubicMode(Ptr<TcpSocketState> tcb);
    void CongestionStateSet_cubic(Ptr<TcpSocketState> tcb);
    void CubicReset (Ptr<const TcpSocketState> tcb);
    void HystartReset (Ptr<const TcpSocketState> tcb);
    void IncreaseWindow_cubic (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);
    uint32_t Update (Ptr<TcpSocketState> tcb);
    void PktsAcked_cubic (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
                     const Time &rtt);
    void HystartUpdate (Ptr<TcpSocketState> tcb, const Time& delay);
    Time HystartDelayThresh (const Time& t); 
    // cubic

protected:
    FUNC_INLINE bool BbrFullBandwidthReached() const;
    FUNC_INLINE DataRate BbrBandwidth() const;
    FUNC_INLINE uint64_t BbrExtraAcked() const;
    //bbr_rate_bytes_per_sec
    FUNC_INLINE DataRate BbrRate(DataRate bw,double gain) const;
    FUNC_INLINE DataRate BbrBandwidthToPacingRate(Ptr<TcpSocketState> tcb, DataRate bw,double gain) const;
    void InitPacingRateFromRtt(Ptr<TcpSocketState> tcb);
    void SetPacingRate(Ptr<TcpSocketState> tcb,DataRate bw, double gain);
    
    FUNC_INLINE void ResetLongTermBandwidthSamplingInterval();
    FUNC_INLINE void ResetLongTermBandwidthSampling();
    void LongTermBandwidthIntervalDone(Ptr<TcpSocketState> tcb,DataRate bw);
    void LongTermBandwidthSampling(Ptr<TcpSocketState> tcb,const TcpRateOps::TcpRateSample &rs);
    
    void UpdateBandwidth(Ptr<TcpSocketState> tcb,const TcpRateOps::TcpRateConnection &rc,const TcpRateOps::TcpRateSample &rs);
    void UpdateAckAggregation(Ptr<TcpSocketState> tcb,const TcpRateOps::TcpRateConnection &rc,const TcpRateOps::TcpRateSample &rs);
    void CheckFullBandwidthReached(const TcpRateOps::TcpRateSample &rs);
    void CheckDrain(Ptr<TcpSocketState> tcb);
    FUNC_INLINE void CheckProbeRttDone(Ptr<TcpSocketState> tcb);
    void UpdateMinRtt(Ptr<TcpSocketState> tcb,const TcpRateOps::TcpRateConnection &rc,
                            const TcpRateOps::TcpRateSample &rs);
    FUNC_INLINE void UpdateGains();
    void UpdateModel(Ptr<TcpSocketState> tcb,const TcpRateOps::TcpRateConnection &rc,
                            const TcpRateOps::TcpRateSample &rs);
    FUNC_INLINE void SaveCongestionWindow(uint32_t congestion_window);
    FUNC_INLINE uint64_t BbrBdp(Ptr<TcpSocketState> tcb,DataRate bw,double gain);
    FUNC_INLINE uint64_t QuantizationBudget(Ptr<TcpSocketState> tcb,uint64_t cwnd);
    FUNC_INLINE uint64_t BbrInflight(Ptr<TcpSocketState> tcb,DataRate bw,double gain);
    FUNC_INLINE uint64_t BbrBytesInNetAtEdt(uint64_t inflight_now);
    FUNC_INLINE uint64_t AckAggregationCongestionWindow();
    bool SetCongestionWindowRecoveryOrRestore(Ptr<TcpSocketState> tcb,const TcpRateOps::TcpRateConnection &rc,
                                                const TcpRateOps::TcpRateSample &rs,uint32_t *new_cwnd);
    void SetCongestionWindow(Ptr<TcpSocketState> tcb,const TcpRateOps::TcpRateConnection &rc,
                            const TcpRateOps::TcpRateSample &rs,DataRate bw,double gain);
    bool IsNextCyclePhase(Ptr<TcpSocketState> tcb,const TcpRateOps::TcpRateSample &rs);
    FUNC_INLINE void AdvanceCyclePhase();
    void UpdateCyclePhase(Ptr<TcpSocketState> tcb,const TcpRateOps::TcpRateSample &rs);
    FUNC_INLINE void ResetMode();
    FUNC_INLINE void ResetStartUpMode();
    FUNC_INLINE void ResetProbeBandwidthMode();
private:
    FUNC_INLINE uint32_t MockRandomU32Max(uint32_t ep_ro); //pseudo-random number in interval [0, ep_ro)
    typedef WindowedFilter<DataRate,MaxFilter<DataRate>,uint64_t,uint64_t> MaxBandwidthFilter_t;
    
    //used by ResetLongTermBandwidthSamplingInterval
    uint64_t m_delivered{0};
    Time        m_deliveredTime{Seconds (0)};
    uint64_t    m_bytesLost{0};             //!< Count All lost bytes during session
    
    Time                    m_minRtt;    //!< Estimated two-way round-trip propagation delay of the path, estimated from the windowed minimum recent round-trip delay sample.
    Time                    m_minRttStamp;      //!< The wall clock time at which the current BBR.RTProp sample was obtained
    Time                    m_probeRttDoneStamp;     //!< The wall clock time at which the current BBR.RTProp sample was obtained.
    
    MaxBandwidthFilter_t    m_maxBwFilter;                          //!< Maximum bandwidth filter
    uint64_t    m_roundTripCount;
    uint64_t    m_nextRttDelivered; //!< delivered at end of round 
    Time        m_cycleStamp;           //!<  time of this cycle phase start 
    
    uint32_t    m_mode:3,                       //!< current bbr_mode in state machine 
                m_prevCongState:3,              //!< CA state on previous ACK 
                m_packetConservation:1,         //!<  use packet conservation? 
                m_roundStart:1,                 //!<  start of packet-timed tx->ack round? 
                m_idleRestart:1,                //!< restarting after idle? 
                m_probeRttRoundDone:1,     //!<  a BBR_PROBE_RTT round at 4 pkts? 
                unused:13,
                m_ltIsSampling:1,           //!<  taking long-term ("LT") samples now? 
                m_ltRttCount:7,            //!<  round trips in long-term interval 
                m_ltUseBandwidth:1;            //!<  use lt_bw as our bw estimate? 
    DataRate    m_ltBandwidth;
    uint64_t    m_ltLastDelivered;
    Time        m_ltLastStamp;
    uint64_t    m_ltLastLost;
    double      m_highGain;
    double      m_pacingGain;
    double      m_cWndGain;
    uint8_t     m_fullBanwidthReached:1,
                m_fullBandwidthCount:2,     //!< Count of full bandwidth recorded consistently
                m_cycleIndex:3,             //!< current index in pacing_gain cycle array 
                m_hasSeenRtt:1,                //!<have we seen an RTT sample yet?
                unused_b:1;
    uint32_t    m_priorCwnd;
    DataRate    m_fullBandwidth{0};         //!< Value of full bandwidth recorded
    Time        m_ackEpochStamp{Seconds (0)};
    uint64_t    m_extraAckedBytes[2];
    uint64_t    m_ackEpochAckedBytes{0};
    uint16_t    m_extraAckedWinRtts:5,
                m_extraAckedWinIdx:1,
                unused_c:10;
    Ptr<UniformRandomVariable> m_uv{nullptr};
#if (TCP_BBR_DEGUG)
    void LogDebugInfo(Ptr<TcpSocketState> tcb,const TcpRateOps::TcpRateConnection &rc,
                            const TcpRateOps::TcpRateSample &rs);
    Ptr <TcpBbrDebug> m_debug;
#endif
};
}
#endif // TCPBBR_H

