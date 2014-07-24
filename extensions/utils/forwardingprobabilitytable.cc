#include "forwardingprobabilitytable.h"

using namespace ns3::ndn;
using namespace boost::numeric::ublas;

NS_LOG_COMPONENT_DEFINE ("ForwardingProbabilityTable");

ForwardingProbabilityTable::ForwardingProbabilityTable(std::vector<int> faceIds)
{
  this->faceIds = faceIds;
  initTable ();
}

void ForwardingProbabilityTable::initTable ()
{
  table = matrix<double> (faceIds.size () /*rows*/, MAX_LAYERS /*columns*/);

  // fill matrix column-wise /* table(i,j) = i-th row, j-th column*/
  for (unsigned j = 0; j < table.size2 (); ++j) /* columns */
  {
    for (unsigned i = 0; i < table.size1 (); ++i) /* rows */
    {
      if(faceIds.at (i) == DROP_FACE_ID)
        table(i,j) = 0.0;
      else
        table(i,j) = (1.0 / ((double)faceIds.size () - 1.0)); /*set default value to 1 / (d - 1) */
    }
  }

  for(int i = 0; i < MAX_LAYERS; i++) // set all as non-jammed
    jammed[i] = false;

   //std::cout << table << std::endl; /* prints matrix line by line ( (first line), (second line) )*/
}

int ForwardingProbabilityTable::determineOutgoingFace(Ptr<ndn::Face> inFace, Ptr<const Interest> interest, int ilayer)
{
  // normalize column of layer for all possible outgoing faces
  matrix<double> normalized = removeFaceFromTable(inFace);

  //create a copy of the faceIds that excludes the incoming face
  std::vector<int> face_list(faceIds);
  int offset = determineRowOfFace(inFace);

  face_list.erase (face_list.begin ()+offset);

  // choose one face as outgoing according to the probability
  return chooseFaceAccordingProbability(normalized, ilayer, face_list);
}

int ForwardingProbabilityTable::determineRowOfFace(Ptr<ndn::Face> face)
{
  return determineRowOfFace (face->GetId ());
}

int ForwardingProbabilityTable::determineRowOfFace(int face_id)
{
  //determine row of face
  int faceRow = -1;

  for(int i = 0; i < faceIds.size () ; i++)
  {
    if(faceIds.at (i) == face_id)
    {
      faceRow = i;
      break;
    }
  }

  if(faceRow == -1)
  {
    NS_LOG_UNCOND("ERROR: Invalid faceID.");
  }

  return faceRow;
}

matrix<double> ForwardingProbabilityTable::removeFaceFromTable(Ptr<ndn::Face> face)
{
  return removeFaceFromTable(face->GetId ());
}

boost::numeric::ublas::matrix<double> ForwardingProbabilityTable::removeFaceFromTable (int faceId)
{
  int faceRow = determineRowOfFace (faceId);

  if(faceRow == -1)
    return table;

  matrix<double> m (table.size1 () - 1, table.size2 ());

  for (unsigned j = 0; j < table.size2 (); ++j) /* columns */
  {
    for (unsigned i = 0; i < table.size1 (); ++i) /* rows */
    {
      if(i < faceRow)
      {
        m(i,j) = table(i,j);
      }
      /*else if(faceRow == i)
      {
        // skip i-th row.
      }*/
      else if (i > faceRow)
      {
        m(i-1,j) = table(i,j);
      }
    }
  }

  return normalizeColumns (m);
}

boost::numeric::ublas::matrix<double> ForwardingProbabilityTable::normalizeColumns(boost::numeric::ublas::matrix<double> m)
{

  for (unsigned j = 0; j < m.size2 (); ++j) /* columns */
  {
    double colSum= 0;
    for (unsigned i = 0; i < m.size1 (); ++i) /* rows */
    {
      if(m(i,j) > 0)
        colSum += m(i,j);
    }

    if(colSum == 0) // means we have removed the only face that was able to transmitt the traffic
    {
      //split robabilities amoung all other faces
      for (unsigned i = 0; i < m.size1 (); ++i) /* rows */
        m(i,j) = 1.0 /((double)m.size1 ());
    }
    else
    {
      for (unsigned i = 0; i < m.size1 (); ++i) /* rows */
      {
        if(m(i,j) < 0)
          m(i,j) = 0;
        else
          m(i,j) /= colSum;
      }
    }
  }

  return m;
}

int ForwardingProbabilityTable::chooseFaceAccordingProbability(boost::numeric::ublas::matrix<double> m, int layer_of_interest, std::vector<int> faceList)
{
  /*Example:
   *
   *random variable has value R and matrix:
   *F0: 0.23  <--- R < 0.23
   *F1: 0.37  <--- 0.23 < R < 0.23 + 0.37
   *F3: 0.40  <--- 0.23 + 0.37 < R < 0.23 + 0.37 + 0.40
   */

  double rvalue = randomVariable.GetValue ();
  double sum = 0.0;

  if(faceList.size () != m.size1 ())
  {
    NS_LOG_UNCOND("Error ForwardingMatrix has not the same amount of rows as the facelist!");
    return DROP_FACE_ID;
  }

  for(int i = 0; i < m.size1 (); i++)
  {
    sum += m(i, layer_of_interest);

    if(rvalue <= sum)
      return faceList.at (i);
  }

  NS_LOG_UNCOND("Error in Face selection, structs are:");
  NS_LOG_UNCOND("table\n" << m);
  NS_LOG_UNCOND("layer_of_interest " << layer_of_interest);
  NS_LOG_UNCOND("rvalue = " << rvalue);

  for(std::vector<int>::iterator it = faceList.begin (); it != faceList.end (); ++it)
    fprintf(stderr, "facelist[it]=%d",*it);

  return DROP_FACE_ID;
}

void ForwardingProbabilityTable::updateColumns(Ptr<ForwardingStatistics> stats)
{
  NS_LOG_DEBUG("Forwarding Matrix before update:\n" << table);

  std::vector<int> r_faces;
  std::vector<int> ur_faces;

  for(int i = 0; i < MAX_LAYERS; i++) // for each layer
  {
    //determine the set of reliable faces
    r_faces = stats->getReliableFaces (i, RELIABILITY_THRESHOLD);

    //determine the set of unreliable faces
    ur_faces = stats->getUnreliableFaces (i, RELIABILITY_THRESHOLD);

    //utf = unstatisfied_trafic_fraction
    double utf = stats->getUnstatisfiedTrafficFraction (i);
    utf *= ALPHA;

    NS_LOG_DEBUG("Layer " << i << " UTF=" << stats->getUnstatisfiedTrafficFraction (i) << " UTF*alpha=" << utf);

    //check if we need to shift traffic
    if(utf > 0)
    {

      if (ur_faces.size () == 0) //  there are no faces classified as unreliable...
      {
        for(std::vector<int>::iterator it = faceIds.begin(); it != faceIds.end(); ++it)
          table(determineRowOfFace(*it), i) = stats->getActualForwardingProbability (*it,i);
      }
      else // there are unrelaible faces : ur_faces.size () > 0
      {

        //determine the relialbe faces act forwarding Prob > 0
        double r_faces_actual_fowarding_prob = 0.0;
        for(std::vector<int>::iterator it = r_faces.begin(); it != r_faces.end(); ++it) // for each r_face
        {
          r_faces_actual_fowarding_prob += stats->getActualForwardingProbability (*it,i);
        }

        //if we have no relible faces, or no interests can be forwarded to reliable faces
        if(r_faces.size () == 0 || r_faces_actual_fowarding_prob == 0.0) // we drop everything in this case
        {
          table(determineRowOfFace(DROP_FACE_ID), i) = stats->getActualForwardingProbability (DROP_FACE_ID,i)+ utf;
          updateColumn (ur_faces, i, stats, utf, false);

          if(!jammed[i])  // only probe if not jammed
            probeColumn(r_faces, i, stats, false);
        }
        else
        {
          //add traffic to relialbe faces
          updateColumn (r_faces, i, stats, utf, true);
          //remove traffic from unreliable faces
          updateColumn (ur_faces, i, stats, utf, false);
         }
      }
    }
    else if (!jammed[i]) // utf == 0 and layer has not been jammed last time
    {
      if(table(determineRowOfFace(DROP_FACE_ID),i) == 0.0 || r_faces.size () == 0) // dropping prob == 0 or there are no reliable faces
      {
        if(stats->getTotalForwardedInterests (i) != 0)
          for(std::vector<int>::iterator it = faceIds.begin(); it != faceIds.end(); ++it)
            table(determineRowOfFace(*it), i) = stats->getActualForwardingProbability (*it,i);
      }
      else
      {
        NS_LOG_DEBUG("WE SHOULD DECREASE DROPPING TRAFFIC\n");

        //check if we should do probing or shifting and probing
        std::vector<int> shift_faces;
        std::vector<int> probe_faces;
        for(std::vector<int>::iterator it = r_faces.begin(); it != r_faces.end(); ++it)
        {
          if(stats->getActualForwardingProbability (*it,i) > SHIFT_THRESHOLD)
            shift_faces.push_back (*it);
          else
            probe_faces.push_back (*it);
        }

        if(shift_faces.size () == 0)
        {
          probeColumn(probe_faces, i, stats, true); // do only probing
        }
        else
        {
          shiftDroppingTraffic(shift_faces, i, stats); //shift traffic
          probeColumn(probe_faces, i, stats, false); // and probe then
        }
      }
    }
  }

  NS_LOG_DEBUG("Forwarding Matrix after update:\n" << table);

  //finally just normalize to remove the error introduced by threashold RELIABILITY_THRESHOLD
  table = normalizeColumns(table);

  NS_LOG_DEBUG("Forwarding Matrix after normalization:\n" << table);
}

//shift == true means shift trafic towards faces, else remove traffic from faces.
void ForwardingProbabilityTable::updateColumn(std::vector<int> faces, int layer, Ptr<ForwardingStatistics> stats, double utf,
                                              bool shift_traffic/*true -> traffic will be shifted towards faces, false traffic will be shifted away*/)
{
  if(faces.size () == 0)
    return;

  double sum_reliabilities = 0.0;

  if(shift_traffic)
    sum_reliabilities = stats->getSumOfReliabilies (faces, layer);
  else
    sum_reliabilities = stats->getSumOfUnreliabilies (faces, layer);

  double sum_fwProbs = getSumOfForwardingProbabilities (faces, layer);

  double normalization_value = 0.0;
  for(std::vector<int>::iterator it = faces.begin(); it != faces.end(); ++it) // for each r_face
  {
    if(shift_traffic && sum_fwProbs > 0)
    {
      normalization_value +=
        (table(determineRowOfFace (*it), layer) / sum_fwProbs) * (stats->getLinkReliability (*it,layer) / sum_reliabilities);
    }
    else if(shift_traffic && sum_fwProbs == 0)  // special case when forwarding probabilities are all 0 for all non relialbe faces. e.g. (0, 0, 1) where f3() = 1 is the incoming face of the interests
    {
      normalization_value +=
        (1.0 /((double)faces.size ())) * (stats->getLinkReliability (*it,layer) / sum_reliabilities);
    }
    else if(!shift_traffic && sum_fwProbs == 0)
    {
      normalization_value +=
        (1.0 /((double)faces.size ())) * ((1 - stats->getLinkReliability (*it,layer)) / sum_reliabilities);
    }
    else
    {
      normalization_value +=
        (table(determineRowOfFace (*it), layer) / sum_fwProbs) * ((1 - stats->getLinkReliability (*it,layer)) / sum_reliabilities);
    }
  }

  if(normalization_value == 0)
  {
    NS_LOG_UNCOND("Error normalization_value == 0.\n");
  }

  for(std::vector<int>::iterator it = faces.begin(); it != faces.end(); ++it) // for each r_face
  {

    double actualFWProb = stats->getActualForwardingProbability (*it, layer);

    NS_LOG_DEBUG("actual fwProb = " << actualFWProb);

    if(shift_traffic && sum_fwProbs > 0)
    {
      table(determineRowOfFace(*it), layer) = actualFWProb +
        utf * (table(determineRowOfFace (*it), layer) / sum_fwProbs) * (stats->getLinkReliability (*it,layer) / sum_reliabilities) / normalization_value;
    }
    else if (shift_traffic && sum_fwProbs == 0) // special case
    {
      table(determineRowOfFace(*it), layer) = actualFWProb +
          utf * (1.0 /((double)faces.size ())) * ( stats->getLinkReliability (*it,layer) / sum_reliabilities / normalization_value);
    }
    else if (!shift_traffic && sum_fwProbs == 0) // special case
    {
      table(determineRowOfFace(*it), layer) = actualFWProb -
          utf * (1.0 /((double)faces.size ())) * ( (1 - stats->getLinkReliability (*it,layer)) / sum_reliabilities / normalization_value);
    }
    else
    {
      table(determineRowOfFace(*it), layer) = actualFWProb -
          utf * (table(determineRowOfFace (*it), layer) / sum_fwProbs) * ( (1 - stats->getLinkReliability (*it,layer)) / sum_reliabilities / normalization_value);
    }
  }
}

void ForwardingProbabilityTable::probeColumn(std::vector<int> faces, int layer,Ptr<ForwardingStatistics> stats, bool useActualProbability)
{
  NS_LOG_DEBUG("PROBING");

  if(faces.size () == 0)
    return;

   double probe = 0.0;

  if(useActualProbability)
    probe = stats->getActualForwardingProbability (DROP_FACE_ID,layer) * PROBING_TRAFFIC;
  else
    probe = table(determineRowOfFace (DROP_FACE_ID), layer) * PROBING_TRAFFIC;

  if(useActualProbability)
    table(determineRowOfFace (DROP_FACE_ID), layer) = stats->getActualForwardingProbability (DROP_FACE_ID,layer) - probe;
  else
    table(determineRowOfFace (DROP_FACE_ID), layer) -= probe;

  //split the probe (forwarding percents)....
  for(std::vector<int>::iterator it = faces.begin(); it != faces.end(); ++it) // for each ur_face
  {
    table(determineRowOfFace (*it), layer) = stats->getActualForwardingProbability (*it, layer) + (probe / ((double)faces.size ()));
  }
}

void ForwardingProbabilityTable::shiftDroppingTraffic(std::vector<int> faces, int layer,Ptr<ForwardingStatistics> stats)
{
  //calcualte how much traffic we can take
  double interests_to_shift = 0;
  for(std::vector<int>::iterator it = faces.begin(); it != faces.end(); ++it) // for each r_face
  {
    interests_to_shift += stats->getForwardedInterests (*it, layer);
  }

  interests_to_shift *= SHIFT_TRAFFIC;

  double dropped_interests = stats->getForwardedInterests(DROP_FACE_ID, layer);

  if(dropped_interests <= interests_to_shift)
  {
    interests_to_shift = dropped_interests;
  }

  double utf = (interests_to_shift / (double)stats->getTotalForwardedInterests (layer));
  NS_LOG_DEBUG("shiftDroppingTraffic utf = " << utf);

  table(determineRowOfFace(DROP_FACE_ID), layer) = stats->getActualForwardingProbability (DROP_FACE_ID, layer) - utf;
  updateColumn (faces, layer,stats,utf,true);

}

double ForwardingProbabilityTable::getSumOfForwardingProbabilities(std::vector<int> set_of_faces, int layer)
{
  double sum = 0.0;
  for(std::vector<int>::iterator it = set_of_faces.begin(); it != set_of_faces.end(); ++it)
  {
    sum += table(determineRowOfFace (*it), layer);
  }
  return sum;
}

double ForwardingProbabilityTable::getSumOfActualForwardingProbabilities(std::vector<int> set_of_faces, int layer, Ptr<ForwardingStatistics> stats)
{
  double sum = 0.0;
  for(std::vector<int>::iterator it = set_of_faces.begin(); it != set_of_faces.end(); ++it)
  {
    sum += stats->getActualForwardingProbability (*it, layer);
  }
  return sum;
}

void ForwardingProbabilityTable::syncDroppingPolicy(Ptr<ForwardingStatistics> stats)
{
  NS_LOG_DEBUG("syncDroppingPolicy");

  NS_LOG_DEBUG("Forwarding Matrix before update:\n" <<table);

  //set all layers as not jammed
  for(int i=0; i < MAX_LAYERS; i++)
    jammed[i] = false;

  int first = getFirstDroppingLayer ();
  int last = getLastDroppingLayer ();

  //jamm all layers until > last all layers

  if(first < last)
  {
    for(int i= MAX_LAYERS -1; i > last; i--)
      jammed[i] = true;
  }

  //int iteration = 0;

  while(first < last)
  {

    /*iteration++;
    if(iteration > 6)
      exit(-1);

    fprintf(stderr, "first = %d\n", first);
    fprintf(stderr, "last = %d\n", last);*/

    //set last as jammed
    jammed[last] = true;

    // interest forwared to the dropping face of first
    double theta = table(determineRowOfFace (DROP_FACE_ID), first) * stats->getTotalForwardedInterests (first);

    //max traffic that can be shifted towards face(s) of last
    double chi = (1.0 - table(determineRowOfFace (DROP_FACE_ID), last)) * stats->getTotalForwardedInterests (last);

    //fprintf(stderr, "theta = %f\n", theta);
    //fprintf(stderr, "chi = %f\n", chi);

    if (theta == 0)
    {
      for(std::vector<int>::iterator it = faceIds.begin(); it != faceIds.end(); ++it)
      {
        if(*it == DROP_FACE_ID)
          table(determineRowOfFace (*it), first) = 0;
        else
          table(determineRowOfFace (*it), first) = 1.0 / (faceIds.size ()-1);
      }
    }
    else if(chi == 0) // nothing has been forwarded via the last face, set drop prob to 100%
    {
      for(std::vector<int>::iterator it = faceIds.begin(); it != faceIds.end(); ++it)
      {
        if(*it == DROP_FACE_ID)
          table(determineRowOfFace (*it), last) = 1;
        else
          table(determineRowOfFace (*it), last) = 0;
      }
    }
    else
    {
      if(chi > theta) //if we can shift more than we need to, we just shift how much we need
        chi = theta;

      //reduce dropping prob for lower layer
      table(determineRowOfFace (DROP_FACE_ID), first) -= (chi / stats->getTotalForwardedInterests (first));

      //increase dropping prob for higher layer
      table(determineRowOfFace (DROP_FACE_ID), last)  += (chi / stats->getTotalForwardedInterests (last));

      //calc n_frist , n_last normalization value without dropping face;
      double n_first = 0;
      double n_last = 0;

      for(std::vector<int>::iterator it = faceIds.begin(); it != faceIds.end(); ++it)
      {
        if(*it == DROP_FACE_ID)
          continue;

        n_first += table(determineRowOfFace (*it), first);
        n_last +=  table(determineRowOfFace (*it), last);
      }

      //increase & decrease other faces accordingly
      for(std::vector<int>::iterator it = faceIds.begin(); it != faceIds.end(); ++it)
      {
        if(*it == DROP_FACE_ID)
          continue;

        table(determineRowOfFace (*it), first) += (chi/stats->getTotalForwardedInterests (first)) * (table(determineRowOfFace (*it), first)/n_first);
        table(determineRowOfFace (*it), last) -= (chi/stats->getTotalForwardedInterests (last)) * (table(determineRowOfFace (*it), last)/n_last);
      }

      NS_LOG_DEBUG("Forwarding Matrix after sync:\n" << table);

      table = normalizeColumns(table);

      NS_LOG_DEBUG("Forwarding Matrix after normalization:\n" << table);
    }

    first = getFirstDroppingLayer ();
    last = getLastDroppingLayer ();
  }

  for(int i = 0; i < jammed.size (); i++)
    NS_LOG_DEBUG("jammed[" << i << "%d]="<< jammed[i]);
}

int ForwardingProbabilityTable::getFirstDroppingLayer()
{
  for(int i = 0; i < MAX_LAYERS; i++) // for each layer
  {
    if(table(determineRowOfFace (DROP_FACE_ID), i) > 0.0)
      return i;
  }
  return MAX_LAYERS-1;
}

int ForwardingProbabilityTable::getLastDroppingLayer()
{
  for(int i = MAX_LAYERS - 1; i >= 0; i--) // for each layer
  {
    if(table(determineRowOfFace (DROP_FACE_ID), i) < 1.0)
      return i;
  }

  return 0;
}
