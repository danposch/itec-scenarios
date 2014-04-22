#include "downloadmanager.h"

using namespace ns3::utils;

DownloadManager::DownloadManager(DownloaderType dwType, Ptr<Node> node)
{
  //make downloaders ready
  this->node = node;

  IDownloader* d = NULL;
  for(int i = 0; i < DOWNLOADER_NUMBER; i++)
  {
    d = resolveDownloader (dwType, node);

    d->addObserver (this);
    this->downloaders.push_back (d);
  }

  lastDownloader = downloaders.front ();
}

void DownloadManager::update(ObserverMessage msg)
{
  switch(msg)
  {
    case Observer::NackReceived:
    {
      specialNACKreceived();
      break;
    }
    case Observer::SegmentReceived://dwnl is still bussy BUT finished and waits for reset
    {
      segmentReceived();
      break;
    }
    case Observer::SoonFinished:
    {
      if(enquedSegments.size () > 0)
        downloadSegments ();
      break;
    }
    default:
    {
      fprintf(stderr, "Received a invalid msg\n");
    }
  }
}

void DownloadManager::addToFinished (Segment *seg)
{
  finishedSegments.push_back (seg);

  /*fprintf(stderr, "DownloadManager::addToFinished %s\n", seg->getUri ().c_str ());
  fprintf(stderr, "enquedSegments.size () = %d\n", enquedSegments.size ());
  fprintf(stderr, "downloaders.size () = %d\n", downloaders.size ());
  fprintf(stderr, "getAllNonBussyDownloaders.size () = %d\n", getAllNonBussyDownloaders().size ());
*/

  if(enquedSegments.size () == 0 && downloaders.size () == getAllNonBussyDownloaders ().size ())
  {
    //bunch of segments finsihed notify observers
    fprintf(stderr, "DownloadManager::Observer::SegmentReceived %s\n", seg->getUri ().c_str ());
    notifyAll (Observer::SegmentReceived);
  }

  return;
}

void DownloadManager::specialNACKreceived ()
{
  //delete all requests of further layers
  enquedSegments.clear ();

  //find the downloader who triggered the special NACK

  IDownloader* nackDwn = NULL;

  std::vector<IDownloader*> dwn = getAllBussyDownloaders ();
  for(int i = 0; i < dwn.size (); i++)
  {
    if(!dwn.at(i)->downloadFinished() && dwn.at(i)->wasSuccessfull() == false)
    {
      nackDwn = dwn.at(i);
      break;
    }
  }

  if(nackDwn == NULL)
  {
    fprintf(stderr, "ERROR COULD NOT FIND NACK DOWNLOADER\n");
    return;
  }

  int level = nackDwn->getSegment ()->getLevel ();

  //stop all downloaders that download quality > level
  for(int i = 0; i < dwn.size (); i++)
  {
    if(!dwn.at(i)->downloadFinished() && dwn.at(i)->getSegment ()->getLevel () >= level)
    {
      dwn.at(i)->abortDownload();
      dwn.at(i)->reset();
    }
  }
}

void DownloadManager::segmentReceived ()
{
  //search for the finished downloader wich where succesfull
   IDownloader *d = NULL;
   for(int i = 0; i < downloaders.size (); i++)
   {
     d = downloaders.at (i);

     if(d->isBussy () && d->downloadFinished () && d->wasSuccessfull ())
     {
       break;
     }
   }

   if(d == NULL)
     return;

   Segment* s = d->getSegment ();

   // reset the downloader state (but not its cwnd)
   d->reset ();

   addToFinished(s);

   //if no downloaders a running...
   if(getAllBussyDownloaders ().size () == 0 && enquedSegments.size () > 0)
      downloadSegments ();
}

void DownloadManager::enque (std::vector<Segment *> segments)
{
  // ok enque them
  if(segments.size () < 1)
    return;

  this->enquedSegments = segments;
  this->finishedSegments.clear ();

  //if all are non bussy, well trigger the download
  std::vector<IDownloader*> dls = getAllNonBussyDownloaders();

  if(dls.size () == downloaders.size ())
    downloadSegments();
}

void DownloadManager::downloadSegments()
{
  IDownloader* dl = getFreeDownloader ();

  if (dl == NULL)
    return;

  dl->setCongWindow(lastDownloader->getCongWindow ());
  lastDownloader = dl;

  Segment* seg_to_dl = *(enquedSegments.begin ());
  enquedSegments.erase (enquedSegments.begin ());

  dl->download (seg_to_dl);
}

std::vector<IDownloader*> DownloadManager::getAllNonBussyDownloaders()
{
  std::vector<IDownloader*> dwn;

  for(int i = 0; i < downloaders.size (); i++)
    if(!downloaders.at (i)->isBussy())
      dwn.push_back (downloaders.at (i));

  return dwn;
}

std::vector<IDownloader*> DownloadManager::getAllBussyDownloaders()
{
  std::vector<IDownloader*> dwn;

  for(int i = 0; i < downloaders.size (); i++)
    if(downloaders.at (i)->isBussy())
      dwn.push_back (downloaders.at (i));

  return dwn;
}

IDownloader* DownloadManager::getFreeDownloader ()
{
  std::vector<IDownloader*> dls = getAllNonBussyDownloaders();

  if(dls.size () > 0)
    return dls.front ();
  else
    return NULL;
}

std::vector<Segment*> DownloadManager::retriveFinishedSegments()
{
  return finishedSegments;
}

std::vector<Segment *> DownloadManager::retriveUnfinishedSegments()
{
   //we cant return anything just continue download player will stall
  if(finishedSegments.size () < 1)
    return std::vector<Segment*> ();

  //check if base layer is here and order segments
  std::vector<utils::Segment *> checked_segs;
  //check if segment levels are continous.
  bool foundLayer = false;
  for(int i = 0; i < finishedSegments.size (); i++)
  {
    for(int j = 0; j < finishedSegments.size (); j++)
    {
      if(finishedSegments.at (j)->getLevel() == i)
      {
        checked_segs.push_back (finishedSegments.at (j));
        foundLayer = true;
        break;
      }
    }
    if(!foundLayer)
      break;
    foundLayer = false;
  }

  if(checked_segs.size () == 0)
  {
    //fprintf(stderr, "Base layer is missing.\n");
    return std::vector<Segment*> ();
  }

  // turn off all downloaders segments are too late now
  std::vector<IDownloader*> dwn = getAllBussyDownloaders ();
  for(int i = 0; i < dwn.size (); i++)
  {
    if(!dwn.at(i)->downloadFinished())
    {
      dwn.at(i)->abortDownload();
      dwn.at(i)->reset();
    }
  }

  std::vector<Segment*> return_segs = checked_segs;
  enquedSegments.clear ();
  finishedSegments.clear ();

  return return_segs;
}

IDownloader* DownloadManager::resolveDownloader(DownloaderType downloader, Ptr<Node> node)
{
  IDownloader* d = NULL;

  switch(downloader)
  {
    case SimpleNDN:
    {
      d = new SimpleNDNDownloader();
      break;
    }
    case WindowNDN:
    {
      d = new WindowNDNDownloader();
      break;
    }
    default:
      d = new WindowNDNDownloader();
  }

  d->setNodeForNDN (node);
  return d;
}

uint64_t DownloadManager::getPhysicalBitrate()
{
  // Get Device Bitrate
  Ptr<PointToPointNetDevice> nd1 =
  node->GetDevice(0)->GetObject<PointToPointNetDevice>();
  DataRateValue dv;
  nd1->GetAttribute("DataRate", dv);
  DataRate d = dv.Get();
  return d.GetBitRate();
}

