#ifndef IADAPTATIONLOGIC_H
#define IADAPTATIONLOGIC_H

#include <string>
#include <vector>
#include "libdash/libdash.h"

#include "ns3-dev/ns3/timer.h"

#include "../utils/buffer.h"
#include "../utils/segment.h"

#include <limits.h>

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>

namespace ns3
{
  namespace dashimpl
  {

    enum AdaptationLogicType
    {
        AlwaysBest, //todo impl
        AlwaysLowest,
        RateBased,
        BufferBased
    };

    class IAdaptationLogic
    {
    public:

      //TODO
      IAdaptationLogic(dash::mpd::IMPD* mpd, std::string dataset_path, utils::Buffer *buf);

      virtual ~IAdaptationLogic(){}
      virtual utils::Segment* getNextSegment();
      virtual void updateStatistic(Time start, Time stop, unsigned int segment_size) = 0;

    protected:
      dash::mpd::IMPD* mpd;
      std::string dataset_path;
      dash::mpd::IPeriod* currentPeriod;
      unsigned int currentSegmentNr;
      std::string base_url;
      utils::Buffer* buf;

      virtual dash::mpd::IRepresentation* getOptimalRepresentation(dash::mpd::IPeriod *period) = 0;

      virtual dash::mpd::IPeriod* getFirstPeriod();

      virtual dash::mpd::IRepresentation* getBestRepresentation(dash::mpd::IPeriod* period);
      virtual dash::mpd::IRepresentation* getLowestRepresentation(dash::mpd::IPeriod* period);
      virtual unsigned int getFileSize(std::string filename);

    };
  }
}
#endif // IADAPTATIONLOGIC_H
