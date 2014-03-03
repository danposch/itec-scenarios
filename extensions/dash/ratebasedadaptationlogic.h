#ifndef RATEBASEDADAPTATIONLOGIC_H
#define RATEBASEDADAPTATIONLOGIC_H

#include "iadaptationlogic.h"

#define SEGMENT_HISTORY 10

namespace ns3
{
  namespace dashimpl
  {
    class RateBasedAdaptationLogic : public IAdaptationLogic
    {
    public:
      RateBasedAdaptationLogic(dash::mpd::IMPD *mpd, std::string dataset_path, utils::Buffer *buf);
      virtual void updateStatistic(Time start, Time stop, unsigned int segment_size);

    protected:
      dash::mpd::IRepresentation* getOptimalRepresentation(dash::mpd::IPeriod *period);

      unsigned int getAvgDLS();

      int64_t* avg_dl_rates;
      int64_t index;

    };
    }
}

#endif // RATEBASEDADAPTATIONLOGIC_H