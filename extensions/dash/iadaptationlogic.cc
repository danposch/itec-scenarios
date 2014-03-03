#include "iadaptationlogic.h"

using namespace ns3::dashimpl;
using namespace ns3::utils;

NS_LOG_COMPONENT_DEFINE ("IAdaptationLogic");

IAdaptationLogic::IAdaptationLogic(dash::mpd::IMPD* mpd, std::string dataset_path, utils::Buffer* buf)
{
  this->mpd = mpd;
  this->dataset_path = dataset_path;
  this->currentPeriod = getFirstPeriod();
  this->currentSegmentNr = 0;
  this->buf = buf;

  if(this->currentPeriod->GetBaseURLs().size () > 0)
    this->base_url = this->currentPeriod->GetBaseURLs().at(0)->GetUrl();
  else
    this->base_url = mpd->GetBaseUrls().at(0)->GetUrl();
}

dash::mpd::IRepresentation* IAdaptationLogic::getBestRepresentation(dash::mpd::IPeriod* period)
{
  std::vector<dash::mpd::IAdaptationSet *> adaptationSets = period->GetAdaptationSets();

  int bitrate = 0;
  dash::mpd::IRepresentation *best = NULL;

  for(size_t i = 0; i < adaptationSets.size(); i++)
  {
      std::vector<dash::mpd::IRepresentation*> reps = adaptationSets.at(i)->GetRepresentation();
      for(size_t j = 0; j < reps.size(); j++)
      {
          int currentBitrate = reps.at(j)->GetBandwidth();

          if(currentBitrate > bitrate)
          {
              bitrate = currentBitrate;
              best    = reps.at(j);
          }
      }
  }
  return best;
}

Segment *IAdaptationLogic::getNextSegment()
{
  dash::mpd::IRepresentation* rep = getOptimalRepresentation(currentPeriod);

  Segment *s = NULL;
  std::string uri("");
  std::string seg_name("");

  if(rep->GetSegmentList ()->GetSegmentURLs().size() > currentSegmentNr)
  {
    uri.append (base_url);
    seg_name.append(rep->GetSegmentList()->GetSegmentURLs().at(currentSegmentNr)->GetMediaURI());
    uri.append (seg_name);
    currentSegmentNr++;

    s = new Segment(uri, getFileSize(dataset_path + seg_name), rep->GetSegmentList()->GetDuration());
  }

  return s;
}

dash::mpd::IRepresentation* IAdaptationLogic::getLowestRepresentation(dash::mpd::IPeriod* period)
{
  std::vector<dash::mpd::IAdaptationSet *> adaptationSets = period->GetAdaptationSets();

  int bitrate = INT_MAX;
  dash::mpd::IRepresentation *lowest = NULL;

  for(size_t i = 0; i < adaptationSets.size(); i++)
  {
      std::vector<dash::mpd::IRepresentation*> reps = adaptationSets.at(i)->GetRepresentation();
      for(size_t j = 0; j < reps.size(); j++)
      {
          int currentBitrate = reps.at(j)->GetBandwidth();

          if(currentBitrate < bitrate)
          {
              bitrate = currentBitrate;
              lowest    = reps.at(j);
          }
      }
  }
  return lowest;
}

dash::mpd::IPeriod* IAdaptationLogic::getFirstPeriod()
{
  std::vector<dash::mpd::IPeriod *> periods = this->mpd->GetPeriods ();

      if(periods.size() == 0)
          return NULL;

      return periods.at(0);
}

unsigned int IAdaptationLogic::getFileSize (std::string filename)
{
  struct stat fstats;
  if(!(stat (filename.c_str(), &fstats) == 0))
  {
    fprintf(stderr, "ContentProvider::OnInterest: File does NOT exist: %s", filename.c_str ());
    return -1;
  }

  return fstats.st_size;
}
