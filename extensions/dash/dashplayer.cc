#include "dashplayer.h"

using namespace ns3::dashimpl;
using namespace ns3::utils;

NS_LOG_COMPONENT_DEFINE ("DashPlayer");

DashPlayer::DashPlayer(dash::mpd::IMPD* mpd, IAdaptationLogic *alogic, ns3::utils::Buffer* buf, std::vector<IDownloader*> downloaders)
{
  this->mpd = mpd;
  this->alogic = alogic;
  this->buf = buf;
  this->downloaders = downloaders;
  this->downloaderChooser = 0;

  for(int i = 0; i < downloaders.size (); i++)
    this->downloaders[i]->addObserver (this);

  isPlaying = false;
  isStreaming = false;
}

void DashPlayer::play ()
{
  NS_LOG_FUNCTION(this);
  isPlaying = true;
  allSegmentsDownloaded = false;
  streaming ();
  Simulator::Schedule(Seconds(2.0), &DashPlayer::consume, this);
}

void DashPlayer::streaming ()
{
  if(isPlaying)
  {
    if(current_segments.size () == 0) // else prior segment(s) has not been downloaded completely
      current_segments = alogic->getNextSegments();

    //check if last segment
    if(current_segments.size () == 0)
    {
      allSegmentsDownloaded = true;
      return;
    }

    Segment* cur_seg = current_segments.front ();

   //w8 if buffer is full
    if(buf->bufferedSeconds () >= (buf->maxBufferSeconds () - cur_seg->getDuration ()))
    {
      Simulator::Schedule(MilliSeconds (100), &DashPlayer::streaming, this);
      return;
    }

    //check if connection is open or buffer is full...
    while(isStreaming || (buf->bufferedSeconds () >= (buf->maxBufferSeconds () - cur_seg->getDuration ())))
    {
      Simulator::Schedule(MilliSeconds (1), &DashPlayer::streaming, this);
      return;
    }

    dlStartTime = Simulator::Now ();
    fprintf(stderr, "DashPlayer::requesting Segment: %s\n", cur_seg->getUri ().c_str ());

    downloaders[downloaderChooser++]->download (cur_seg);
    downloaderChooser = downloaderChooser % downloaders.size ();

    isStreaming = true;
  }
}

void DashPlayer::stop ()
{
  NS_LOG_FUNCTION(this);
  isPlaying = false;
}

void DashPlayer::update (ObserverMessage msg)
{
  //fprintf(stderr, "NOTIFYED\n");

  alogic->updateStatistic (dlStartTime, Simulator::Now (), current_segments.front()->getSize ());

  //this means we succesfuly download a dash segment or a bunch of svc-dash segments
  if(current_segments.size () == 1)
  {
    if(!buf->addData (current_segments.front()->getDuration ()))
      fprintf(stderr, "BUFFER FULL!!!\n");
  }

  isStreaming = false;
  current_segments.erase (current_segments.begin ());
  streaming ();
}

void DashPlayer::consume ()
{
  if(allSegmentsDownloaded && buf->isEmpty ())
  {
    stop();
    return;
  }

  if(!buf->consumeData (CONSUME_INTERVALL) && isPlaying)
    fprintf(stderr, "CONSUMED FAILED\n");

  Simulator::Schedule(Seconds (CONSUME_INTERVALL), &DashPlayer::consume, this);
}
