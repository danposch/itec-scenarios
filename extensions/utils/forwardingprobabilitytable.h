#ifndef FORWARDINGPROBABILITYTABLE_H
#define FORWARDINGPROBABILITYTABLE_H

#define MAX_LAYERS 4
#define DROP_FACE_ID -1

#include "ns3/random-variable.h"
#include "ns3/ndn-face.h"
#include "ns3/ndn-interest.h"
#include "ns3/log.h"

#include <vector>
#include <stdio.h>

#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/io.hpp>

namespace ns3
{
namespace ndn
{
class ForwardingProbabilityTable
{
public:
  ForwardingProbabilityTable(std::vector<int> faceIds);
  int determineOutgoingFace(Ptr<ndn::Face> inFace, Ptr<const Interest> interest);

protected:

  boost::numeric::ublas::matrix<double> table;
  std::vector<int> faceIds;

  void initTable();
  boost::numeric::ublas::matrix<double> removeFaceFromTable(Ptr<ndn::Face> face);

  ns3::UniformVariable randomVariable;
};

}
}
#endif // FORWARDINGPROBABILITYTABLE_H

