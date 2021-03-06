#include "windowndndownloader.h"

using namespace ns3;
using namespace ns3::utils;

NS_LOG_COMPONENT_DEFINE ("WindowNDNDownloader");


WindowNDNDownloader::WindowNDNDownloader() : IDownloader()
{
  interest  = NULL;

  //this->needDownloadBeforeEvent = EventId();

  this->cwnd = Create<CongestionWindow>();


  //init status
  reset();

  // init statistics
  packets_received = 0;
  packets_timeout = 0;
  packets_inflight = 0;
  packets_nack = 0;
  packets_received_this_second = 0;
  packets_sent_this_second = 0;
  had_ack = had_nack = had_timeout = false;

  // create the mean RTT collector
  m_rtt = CreateObject<ndn::RttMeanDeviation> ();
  m_rtt->SetMinRto(MilliSeconds(DEFAULT_MEAN_RTT_MS));
}

WindowNDNDownloader::WindowNDNDownloader(std::string& cwnd_type) : IDownloader()
{
  interest  = NULL;

  //this->needDownloadBeforeEvent = EventId();
  if (cwnd_type.compare("tcp") == 0)
  {
    this->cwnd = Create<CongestionWindow>();
  } else {
    this->cwnd = Create<StaticCongestionWindow>();
  }

  //init status
  reset();

  // init statistics
  packets_received = 0;
  packets_timeout = 0;
  packets_inflight = 0;
  packets_nack = 0;
  packets_received_this_second = 0;
  packets_sent_this_second = 0;
  had_ack = had_nack = had_timeout = false;

  // create the mean RTT collector
  m_rtt = CreateObject<ndn::RttMeanDeviation> ();
  m_rtt->SetMinRto(MilliSeconds(DEFAULT_MEAN_RTT_MS));
}


void WindowNDNDownloader::collectStats()
{
  // reset had_ack, had_nack, had_timeout
  had_ack = had_nack = had_timeout = false;

  /*
  NS_LOG_INFO("stats(" << this << "," << Simulator::Now().ToDouble(Time::S) <<
              "): cwnd=" <<
              this->cwnd.GetWindowSize() <<
              ", bytesToDownload=" <<
              this->curSegmentStatus.bytesToDownload);
  */
  this->statsOutputTimer = Simulator::Schedule(MilliSeconds(STATS_OUTPUT_TIMER_MS),
                                               &WindowNDNDownloader::collectStats, this);

}


bool WindowNDNDownloader::downloadBefore (Ptr<Segment> s, int miliSeconds)
{
  return download(s);
}

bool WindowNDNDownloader::download (Ptr<Segment> s)
{
  NS_LOG_FUNCTION(this);

  this->segment = s;
  return download(s->getUri());
}

/* download file from URI */
bool WindowNDNDownloader::download (std::string URI)
{
  NS_LOG_FUNCTION(this << "uri:" << URI << "node: " << Names::FindName (this->GetNode ()));
  // this downloader is now bussy
  bussy = true;

  // init some stats
  this->had_ack = false;
  this->had_nack = false;
  this->had_timeout = false;

  /****
  * STEP 0 - START SOME TIMERS
  */
  // check the stats timer, which collects statistics every 10 milliseconds
  if (statsOutputTimer.IsExpired())
  {
    this->statsOutputTimer = Simulator::Schedule(MilliSeconds(10),
                                               &WindowNDNDownloader::collectStats, this);
  }

  // check if resetStatisticsTimer is running
  if (resetStatisticsTimer.IsExpired())
  {
    resetStatisticsTimer = Simulator::Schedule(Seconds(1.0),
                                               &WindowNDNDownloader::resetStatistics, this);
  }

  // parse the URI (replace http:// if needed..)
  this->curSegmentStatus.base_uri = URI;
  if(this->curSegmentStatus.base_uri.size () > 7 &&
     this->curSegmentStatus.base_uri.substr (0,7).compare ("http://") == 0)
  {
    this->curSegmentStatus.base_uri = std::string("/").append(
          this->curSegmentStatus.base_uri.substr (7,this->curSegmentStatus.base_uri.length ()));
  }

  NS_LOG_FUNCTION(this->curSegmentStatus.base_uri << this);
  /****
  * STEP 0 - DONE
  */


  /****
  * STEP 1 - ESTIMATE RECEIVER WINDOW TO LIMIT THE CONGESTION WINDOW
  */
    //Daniel: moved to setNodeForNDN()
  /*
  * STEP 1 - DONE
  */

  /****
  * STEP 2 - INIT VECTORS FOR SEGMENT STATUS
  */
  /* set bytesToDownload to -1 for now;
    chunk_1 will tell us how big the file actually is */
  this->curSegmentStatus.bytesToDownload = -1;

  // init number of chunks with 10 for now - chunk_1 will tell us how many we will need
  this->curSegmentStatus.num_chunks = 10;
  // init the internal status vectors

  this->curSegmentStatus.chunk_status.clear();

  // clear chunk timeout events in case something is still in there
  this->curSegmentStatus.chunk_timeout_events.clear ();

  this->curSegmentStatus.chunk_status.resize(this->curSegmentStatus.num_chunks, NotInitiated);
  //this->curSegmentStatus.chunk_timeout_events.resize(this->curSegmentStatus.num_chunks);


  // init stats
  packets_received = 0;
  packets_timeout = 0;
  packets_inflight = 0;
  packets_nack = 0;
  lastDownloadSuccessful = true;
  soonFinishedAlreadyFired = false;

  // clear the last rtt information
  m_rtt->Reset();
  m_rtt->ClearSent();
  /*
  * STEP 2 - DONE
  */

  /*** STEP 3 - DOWNLOAD chunk_0 WHICH CONTAINS META DATA */
  downloadChunk(0);

  // also make sure that the download timer is running again (if it stopped, which is most likely)
  if (this->scheduleDownloadTimer.IsExpired())
  {
    this->scheduleDownloadTimer =
      Simulator::Schedule(MilliSeconds(1),
                          &WindowNDNDownloader::ScheduleNextChunkDownload, this);
  }

  return true;
}

void WindowNDNDownloader::downloadChunk (int chunk_number)
{
  NS_LOG_FUNCTION(this << chunk_number);

  if(this->curSegmentStatus.bytesToDownload != 0)
  {
    // check that we actually need this chunk, it might have been received by now
    while (this->curSegmentStatus.chunk_status[chunk_number] == Received)
    {
      chunk_number = this->GetNextNeededChunkNumber ();
      if (chunk_number == -1)
      {
        // we seem to be done for now
        break;
      }
    }

    // make sure we have a valid chunk_number to request
    if (chunk_number != -1)
    {
      prepareInterestForDownload(chunk_number);

      // Call trace (for logging purposes)
      m_transmittedInterests (interest, this, m_face);
      m_face->ReceiveInterest (interest);

      packets_sent_this_second++;

      return; // RETURN HERE SO notifyall is not called
    }
  }

  // else: either bytesToDownload = 0 or chunk_number was -1, in both cases:
  chunk_number = this->GetNextNeededChunkNumber ();

  if(chunk_number == -1 && !soonFinishedAlreadyFired)
  {
    soonFinishedAlreadyFired = false;
    notifyAll(Observer::SoonFinished);
  }

}

void WindowNDNDownloader::resetStatistics()
{
  packets_received_this_second = 0;
  packets_sent_this_second = 0;

  // only do that again if still busy
  if (this->isBussy () && resetStatisticsTimer.IsExpired())
  {
    resetStatisticsTimer = Simulator::Schedule(Seconds(1.0),  &WindowNDNDownloader::resetStatistics, this);
  }

}

int WindowNDNDownloader::GetNextNeededChunkNumber()
{
  return GetNextNeededChunkNumber(0);
}


int WindowNDNDownloader::GetNextNeededChunkNumber(int start_chunk_number)
{
  if (this->curSegmentStatus.num_chunks > 0 // && this->curSegmentStatus.chunk_status != NULL
      )
  {
    int i = start_chunk_number;
    // find first segment that has not been downloaded yet, but is not currently being downloaded
    while (i < this->curSegmentStatus.num_chunks)
    {
      if (this->curSegmentStatus.chunk_status[i] != Received &&
          this->curSegmentStatus.chunk_status[i] != Requested &&
          this->curSegmentStatus.chunk_status[i] != Aborted)
      {
        return i;
      }

      i++;
    }
  }
  return -1;
}


void WindowNDNDownloader::ScheduleNextChunkDownload()
{
  NS_LOG_FUNCTION(this);

  int chunk_number = GetNextNeededChunkNumber();

  //if (chunk_number == -1 && this->curSegmentStatus.bytesToDownload >= 0)
  if (chunk_number == -1)
  {
    // NO chunks available, SAY we will be finished soon and return
    if(!soonFinishedAlreadyFired)
    {
      soonFinishedAlreadyFired = true;
      //fprintf(stderr, "Downloader=%s, notifying that we are soon finished (inflight=%d)...\n", this->curSegmentStatus.base_uri.c_str(), this->packets_inflight);
      notifyAll (Observer::SoonFinished);
    }
    return;
  }

  // cancel the timer in case it is already running
  this->scheduleDownloadTimer.Cancel();

  /* calculate when to download the next chunk
  * make sure to use Double and Seconds here, to get an accurate timer!
  */
  this->scheduleDownloadTimer =
      Simulator::Schedule(Seconds(1.0/(double)this->cwnd->GetWindowSize()),
                          &WindowNDNDownloader::downloadChunk, this, chunk_number);
}



Ptr<ndn::Interest> WindowNDNDownloader::prepareInterestForDownload (int chunk_number)
{
  NS_LOG_FUNCTION(this << chunk_number);
  // set chunk status to requested
  this->curSegmentStatus.chunk_status[chunk_number] = Requested;

  // get current RTO
  double rto = m_rtt->RetransmitTimeout().GetSeconds();
  rto = std::min<double>(rto, WINDOW_MAX_RTO); // RTO must not be higher than 1 second here

  //fprintf(stderr, "RTO=%f\n", rto);

  std::stringstream ss;
  ss << this->curSegmentStatus.base_uri << "/chunk_" << chunk_number;

  Ptr<ndn::Name> prefix = Create<ndn::Name> (ss.str ().c_str());

  // Create and configure ndn::Interest
  this->interest = Create<ndn::Interest> ();
  UniformVariable rand (0,std::numeric_limits<uint32_t>::max ());
  interest->SetNonce (rand.GetValue ());
  interest->SetName (prefix);
  interest->SetInterestLifetime (Seconds (rto)); // set interest life time equal to the event we are fireing

  // set up the timeout event with the calculated RTO
  this->curSegmentStatus.chunk_timeout_events[chunk_number] =
    Simulator::Schedule(Seconds(rto), &WindowNDNDownloader::CheckRetrieveTimeout, this, chunk_number);


  // tell the RTO estimator that we sent out a packet
  m_rtt->SentSeq (SequenceNumber32 (chunk_number), 1);


  // increase packets_inflight
  packets_inflight++;

  //NS_LOG_FUNCTION("Sending Interest packet for " << *prefix << this);

  if (this->scheduleDownloadTimer.IsExpired())
  {
    ScheduleNextChunkDownload();
  }

  //interest->Ref ();

  return interest;
}

void WindowNDNDownloader::CheckRetrieveTimeout(int c_chunk_number)
{
  // timed out (else the event would have been canceled)
  OnTimeout(c_chunk_number);
}



void WindowNDNDownloader::OnTimeout (int c_chunk_number)
{
  // do not timeout on requests that have status != Requested
  // e.g. packet has already been received or NACK'ed
  if (this->curSegmentStatus.chunk_status[c_chunk_number] != Requested)
  {
    // this case should not happen, but it does happen (rarely)
    return;
  }


  NS_LOG_FUNCTION("TIMEOUT: chunk=" << c_chunk_number << ";" << this);

  //fprintf(stderr, "WindowNDNDownloader::OnTimeout chunk %d, cwnd = %d, in_flight=%d\n", c_chunk_number, cwnd.GetWindowSize(), this->packets_inflight);

  // adjust stats
  this->packets_inflight--;
  this->packets_timeout++;



  int64_t diff = Simulator::Now().ToInteger(Time::MS) - lastTimeout.ToInteger(Time::MS);

  if (diff > m_rtt->RetransmitTimeout().ToInteger(Time::MS))
  {
    had_timeout = true;
    lastTimeout = Simulator::Now();
    cwnd->DecreaseWindow();
    cwnd->SetThreshold(this->packets_inflight);
  }

  m_rtt->IncreaseMultiplier ();

  // re-request the packet
  this->curSegmentStatus.chunk_status[c_chunk_number] = Timeout;

  // make sure that the next packet is scheduled again
  ScheduleNextChunkDownload();

}


void WindowNDNDownloader::OnNack (Ptr<const ndn::Interest> interest)
{
  // find out what chunk this is
  std::string s =  interest->GetName().toUri().c_str();
  std::string segment_uri;
  unsigned int pos1 = s.find_last_of("chunk_");
  segment_uri = s.substr(0, pos1-6);
  s = s.substr(pos1+1);
  int c_chunk_number = atoi(s.c_str());


  // check if we requested that chunk (else it probably already timed out or was received
  if(this->curSegmentStatus.chunk_status[c_chunk_number] == Requested)
  {
    int64_t diff = Simulator::Now().ToInteger(Time::MS) - lastTimeout.ToInteger(Time::MS);

    if (diff > m_rtt->RetransmitTimeout().ToInteger(Time::MS))
    {
      had_nack = true;
      lastTimeout = Simulator::Now();
      cwnd->DecreaseWindow();
      cwnd->SetThreshold(this->packets_inflight);
    }

    //fprintf(stderr, "WindowNDNDownloader::OnNack: received NACK for URI: %s\n", interest->GetName ().toUri().c_str());

    NS_LOG_FUNCTION("NACK: chunk=" << c_chunk_number << "; " << this);

    // make sure to cancel the OnTimeout event for this chunk
    this->curSegmentStatus.chunk_timeout_events[c_chunk_number].Cancel();
    this->curSegmentStatus.chunk_timeout_events.erase(c_chunk_number);

    // adjust stats
    this->packets_inflight--;
    this->packets_nack++;

    // adjust rto
    m_rtt->IncreaseMultiplier();

    // re-request the packet
    this->curSegmentStatus.chunk_status[c_chunk_number] = Timeout;

    // make sure that the next packet is scheduled again
    ScheduleNextChunkDownload();
  } else {
    // packet was already received or timed out - do nothing
  }
}

bool WindowNDNDownloader::isPartOfCurrentSegment(std::string packet_name)
{
  if(packet_name.find(curSegmentStatus.base_uri) == std::string::npos)
    return false;
  else
    return true;
}

void WindowNDNDownloader::OnData (Ptr<const ndn::Data> contentObject)
{

  //check of chunk corresponds to current segment.
  if(!isPartOfCurrentSegment(contentObject->GetName ().toUri ()))
   {
     return;
   }



  //fprintf(stderr, "ONDATA %s \n", contentObject->GetName ().toUri ().c_str ());
  NS_LOG_FUNCTION(this);
  bool duplicate = false;

  packets_received_this_second++;
  packets_received++;


  // find out what chunk this is
  std::string s =  contentObject->GetName().toUri().c_str();
  std::string segment_uri;
  unsigned int pos1 = s.find_last_of("chunk_");
  segment_uri = s.substr(0, pos1-6);
  s = s.substr(pos1+1);
  int c_chunk_number = atoi(s.c_str());

  // check if that chunk was already received (--> duplicate ack)
  if (this->curSegmentStatus.chunk_status[c_chunk_number] == Received)
  {
    // should not have happened, but it happened - okay. just make sure to not increase stats
    duplicate = true;
    fprintf(stderr, "Duplicate packet received\n");
  }
  else
  {
    // only reduce inflight if status = Requested (else it already was reduced)
    if (this->curSegmentStatus.chunk_status[c_chunk_number] ==  Requested)
    {
      //fprintf(stderr, "Interest %s, Reducing in flight to %d\n", contentObject->GetName().toUri().c_str(), packets_inflight-1);
      packets_inflight--;
    }
    else
    {
      //fprintf(stderr, "Interest %s timed out, but received anyway...\n", contentObject->GetName().toUri().c_str());
    }

    // Set the chunk status to received
    this->curSegmentStatus.chunk_status[c_chunk_number] = Received;

    // tell the RTO estimator that we received the packet with chunk_number
    m_rtt->AckSeq (SequenceNumber32 (c_chunk_number));

    int64_t diff = Simulator::Now().ToInteger(Time::MS) - lastValidPacket.ToInteger(Time::MS);

    if (diff > m_rtt->RetransmitTimeout().ToInteger(Time::MS))
    {
      had_ack = true;
      // only increase if there was no nack/timeout recently
      if (!had_nack && !had_timeout)
      {
        lastValidPacket = Simulator::Now();
        cwnd->IncreaseWindow();
      }
    }
  }


  // make sure to cancel the OnTimeout event for this chunk
  this->curSegmentStatus.chunk_timeout_events[c_chunk_number].Cancel();
  this->curSegmentStatus.chunk_timeout_events.erase (c_chunk_number);

  if (!duplicate)
  {
    if (c_chunk_number == 0) // this is the first chunk - it contains the file size
    {
      //fprintf(stderr, "chunk number 0 received\n");
      Ptr<Packet> pack = contentObject->GetPayload()->Copy();
      unsigned int buffer = 0;
      pack->CopyData((uint8_t*)&buffer, 4);

      // set bytesToDownload
      this->curSegmentStatus.bytesToDownload += buffer + 1;

      // init number of chunks
      this->curSegmentStatus.num_chunks =
          (int) ceil ( (double)  (buffer) / (double)MAX_PACKET_PAYLOAD );
      // increase by 1, because chunk number 0 is a meta packet
      this->curSegmentStatus.num_chunks++;

      NS_LOG_FUNCTION(std::string("Received first chunk with payload (=bytesToDownload): ")
                      << buffer << std::string("Num_chunks=") << curSegmentStatus.num_chunks << this);


      // reserving memory for num_chunks
      this->curSegmentStatus.chunk_status.resize(this->curSegmentStatus.num_chunks, NotInitiated);
      //this->curSegmentStatus.chunk_timeout_events.resize(this->curSegmentStatus.num_chunks);

    } else
    {
      this->curSegmentStatus.bytesToDownload -= contentObject->GetPayload()->GetSize ();
      NS_LOG_FUNCTION(this << "received chunk " << c_chunk_number << "with size" << contentObject->GetPayload()->GetSize () <<
                      "bytesToDownload=" << this->curSegmentStatus.bytesToDownload);

    }
  }

  //this can occur if the first chunk is delayed. its often not a problem but may indicate one.
  if (this->curSegmentStatus.bytesToDownload < 0)
  {
    //fprintf(stderr, "WARNING: %s bytesToDownload=%d < 0...\n",  contentObject->GetName().toUri().c_str(), this->curSegmentStatus.bytesToDownload);
  }

  if(this->curSegmentStatus.bytesToDownload == 0)
  {
    //fprintf(stderr, "BytesToDownload=0\n");
    // cancel the scheduled download timer
    this->scheduleDownloadTimer.Cancel();

    NS_LOG_FUNCTION(this << "Finally received last segment for uri:" << curSegmentStatus.base_uri << "node: " << Names::FindName (this->GetNode ()));
    //NS_LOG_INFO(std::string("Finally received segment: ").append(curSegmentStatus.base_uri.substr (0,curSegmentStatus.base_uri.find_last_of ("/chunk_"))) << this);
    //this download is now finished and was succesfull
    finished = true;
    lastDownloadSuccessful = true;
    //let the downloader state bussy UNTIL the result has been feteched by the MANAGER
    notifyAll (Observer::SegmentReceived); //notify observers
  }
  else
  {
    if (this->scheduleDownloadTimer.IsExpired())
    {
    // cancel the next download timer, we have a bigger congestion window now, which means less waiting time
    // make sure that the next packet is scheduled again
      ScheduleNextChunkDownload();
    }
  }
}

void WindowNDNDownloader::abortDownload ()
{
  this->scheduleDownloadTimer.Cancel ();
  this->resetStatisticsTimer.Cancel ();
  this->statsOutputTimer.Cancel ();

  CancelAllTimeoutEvents();

  //abort all segments
  for (int i = 0; i < curSegmentStatus.chunk_status.size (); i++)
  {
    if (this->curSegmentStatus.chunk_status[i] != Received)
    {
      this->curSegmentStatus.chunk_status[i] = Aborted;
    }
  }

  // make sure to set finished to false
  this->finished = false;

}

void WindowNDNDownloader::CancelAllTimeoutEvents()
{
    for (std::map<int, EventId>::iterator it = this->curSegmentStatus.chunk_timeout_events.begin();
         it != this->curSegmentStatus.chunk_timeout_events.end(); ++it)
    {
        it->second.Cancel();
    }

    this->curSegmentStatus.chunk_timeout_events.clear ();

    /*

  for (int i = 0; i < this->curSegmentStatus.num_chunks; i++)
  {
    if (this->curSegmentStatus.chunk_status[i] == Requested)
    {
      this->curSegmentStatus.chunk_timeout_events[i].Cancel();
    }
  } */
}

// Processing upon start of the application
void WindowNDNDownloader::StartApplication ()
{
  NS_LOG_FUNCTION(this);
  ndn::App::StartApplication ();
}

// Processing when application is stopped
void WindowNDNDownloader::StopApplication ()
{
  NS_LOG_FUNCTION(this);
  ndn::App::StopApplication ();
}

void WindowNDNDownloader::setNodeForNDN (Ptr<Node> node)
{
  NS_LOG_FUNCTION(this);
  SetNode(node);

  StartApplication ();

  uint64_t bitrate = this->getPhysicalBitrate();

  int max_packets = bitrate / ( MAX_PACKET_PAYLOAD + PACKET_OVERHEAD ) / 8;

  // set threshold to max_packets / 2
  cwnd->SetReceiverWindowSize(max_packets);
  cwnd->SetWindowSize (max_packets); //edited by daniel
  cwnd->SetThreshold(max_packets/2);

}

DownloaderType WindowNDNDownloader::getDownloaderType ()
{
  return WindowNDN;
}

uint64_t WindowNDNDownloader::getPhysicalBitrate()
{
  // Get Device Bitrate
  Ptr<PointToPointNetDevice> nd1 =
  m_face->GetNode ()->GetDevice(0)->GetObject<PointToPointNetDevice>();
  DataRateValue dv;
  nd1->GetAttribute("DataRate", dv);
  DataRate d = dv.Get();
  return d.GetBitRate();
}

const Ptr<CongestionWindow> WindowNDNDownloader::getCongWindow()
{
 return this->cwnd;
}

void WindowNDNDownloader::setCongWindow (Ptr<CongestionWindow> window)
{
  cwnd = window; //check
}

void WindowNDNDownloader::reset ()
{
  this->scheduleDownloadTimer.Cancel ();
  IDownloader::reset ();
}
