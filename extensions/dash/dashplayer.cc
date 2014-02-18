#include "dashplayer.h"

using namespace ns3::dashimpl;
using namespace ns3::utils;

NS_LOG_COMPONENT_DEFINE ("DashPlayer");

DashPlayer::DashPlayer(dash::mpd::IMPD* mpd, IAdaptationLogic *alogic, ns3::utils::Buffer* buf, IDownloader *downloader)
{
  this->mpd = mpd;
  this->alogic = alogic;
  this->buf = buf;
  this->downloader = downloader;
  this->downloader->addObserver (this);

  isPlaying = false;
  isStreaming = false;
  cur_seg = NULL;
}

void DashPlayer::play ()
{
  NS_LOG_FUNCTION(this);
  isPlaying = true;
  streaming ();
  Simulator::Schedule(Seconds (2.0), &DashPlayer::consume, this);
}

void DashPlayer::streaming ()
{
  if(isPlaying)
  {
    if(cur_seg == NULL) // else prior segment has not been downloaded
      cur_seg = alogic->getNextSegmentUri();

    //check if last segment
    if(cur_seg == NULL)
    {
      isPlaying = false;
      //break;
    }

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

    downloader->download (cur_seg);
    isStreaming = true;
  }
}

void DashPlayer::stop ()
{
  NS_LOG_FUNCTION(this);
  isPlaying = false;
}

void DashPlayer::update ()
{
  //fprintf(stderr, "NOTIFYED\n");

  if(!buf->addData (cur_seg->getDuration ()))
    fprintf(stderr, "BUFFER FULL!!!\n");

  isStreaming = false;
  cur_seg = NULL;
  streaming ();
}

void DashPlayer::consume ()
{
  if(!buf->consumeData (CONSUME_INTERVALL) && isPlaying)
    fprintf(stderr, "CONSUMED FAILED");

  Simulator::Schedule(Seconds (CONSUME_INTERVALL), &DashPlayer::consume, this);
}
