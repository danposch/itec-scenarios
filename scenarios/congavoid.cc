#include "ns3-dev/ns3/core-module.h"
#include "ns3-dev/ns3/network-module.h"
#include "ns3-dev/ns3/ndnSIM-module.h"
#include "ns3-dev/ns3/point-to-point-module.h"

#include "ns3-dev/ns3/ndn-app.h"
#include "ns3/nstime.h"
#include <stdlib.h>
#include <string.h>

#include <boost/lexical_cast.hpp>

using namespace ns3;

void parseParameters(int argc, char* argv[], std::string& mode)
{
  bool v0 = false, v1 = false, v2 = false;
  bool vN = false;

  std::string top_path = "topologies/congavoid_6clients.top";

  CommandLine cmd;
  cmd.AddValue ("v0", "Prints all log messages >= LOG_DEBUG. (OPTIONAL)", v0);
  cmd.AddValue ("v1", "Prints all log messages >= LOG_INFO. (OPTIONAL)", v1);
  cmd.AddValue ("v2", "Prints all log messages. (OPTIONAL)", v2);
  cmd.AddValue ("vN", "Disable all internal logging parameters, use NS_LOG instead", vN);
  cmd.AddValue ("top", "Path to the topology file. (OPTIONAL)", top_path);
  cmd.AddValue ("mode", "Sets the simulation mode. Either \"mode=normal\" or \"mode=adaptation\". (OPTIONAL) Default: mode=normal", mode);

  cmd.Parse (argc, argv);

  if (vN == false)
  {
    LogComponentEnableAll (LOG_ALL);

    if(!v2)
    {
      LogComponentDisableAll (LOG_LOGIC);
      LogComponentDisableAll (LOG_FUNCTION);
    }
    if(!v1 && !v2)
    {
      LogComponentDisableAll (LOG_INFO);
    }
    if(!v0 && !v1 && !v2)
    {
      LogComponentDisableAll (LOG_DEBUG);
    }
  } else {
    NS_LOG_UNCOND("Disabled internal logging parameters, using NS_LOG as parameter.");
  }
  AnnotatedTopologyReader topologyReader ("", 20);
  topologyReader.SetFileName (top_path);
  topologyReader.Read();
}

int main(int argc, char* argv[])
{
  NS_LOG_COMPONENT_DEFINE ("LargeScenario");

  std::string mode("normal");
  parseParameters(argc, argv, mode);

  NodeContainer streamers;
  int nodeIndex = 0;
  std::string nodeNamePrefix("ContentDst");
  Ptr<Node> contentDst = Names::Find<Node>(nodeNamePrefix +  boost::lexical_cast<std::string>(nodeIndex++));
  while(contentDst != NULL)
  {
    streamers.Add (contentDst);
    contentDst =  Names::Find<Node>(nodeNamePrefix +  boost::lexical_cast<std::string>(nodeIndex++));
  }

  NodeContainer providers;
  nodeIndex = 0;
  nodeNamePrefix = std::string("ContentSrc");
  Ptr<Node> contentSrc = Names::Find<Node>(nodeNamePrefix +  boost::lexical_cast<std::string>(nodeIndex++));
  while(contentSrc != NULL)
  {
    providers.Add (contentSrc);
    contentSrc = Names::Find<Node>(nodeNamePrefix +  boost::lexical_cast<std::string>(nodeIndex++));
  }

  NodeContainer routers;
  nodeIndex = 0;
  nodeNamePrefix = std::string("Router");
  Ptr<Node> router = Names::Find<Node>(nodeNamePrefix +  boost::lexical_cast<std::string>(nodeIndex++));
  while(router != NULL)
  {
    routers.Add (router);
    router = Names::Find<Node>(nodeNamePrefix +  boost::lexical_cast<std::string>(nodeIndex++));
  }

  NodeContainer adaptiveNodes;
  nodeIndex = 0;
  nodeNamePrefix = std::string("AdaptiveNode");
  Ptr<Node> adaptiveNode = Names::Find<Node>(nodeNamePrefix +  boost::lexical_cast<std::string>(nodeIndex++));
  while(adaptiveNode != NULL)
  {
    if(mode.compare ("adaptation") == 0)
      adaptiveNodes.Add (adaptiveNode);
    else
      routers.Add (adaptiveNode); // we dont use adaptive nodes in normal scenario
    adaptiveNode = Names::Find<Node>(nodeNamePrefix +  boost::lexical_cast<std::string>(nodeIndex++));
  }

  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.SetForwardingStrategy ("ns3::ndn::fw::BestRoute", "EnableNACKs", "true"); // try without limits first...
  //ndnHelper.SetForwardingStrategy ("ns3::ndn::fw::BestRoute::PerOutFaceLimits", "Limit", "ns3::ndn::Limits::Rate",  "EnableNACKs", "true");
  //ndnHelper.EnableLimits (true, Seconds(0.2), 100, 4200);
  ndnHelper.SetContentStore ("ns3::ndn::cs::Stats::Lru","MaxSize", "10000"); // all entities can store up to 10k chunks in cache (about 40MB)
  ndnHelper.Install (providers);
  ndnHelper.Install (streamers);
  ndnHelper.Install (routers);

  //change strategy for adaptive NODE
  ndnHelper.SetForwardingStrategy("ns3::ndn::fw::BestRoute::SVCCountingStrategy",
                                  "EnableNACKs", "true", "LevelCount", "6");
  ndnHelper.EnableLimits (false);
  ndnHelper.Install (adaptiveNodes);

  // Installing global routing interface on all nodes
  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.InstallAll ();

   //consumer
  ndn::AppHelper dashRequesterHelper ("ns3::ndn::DashRequester");
  //dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l/bbb-svc.264.mpd"));
  //dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical1/artifical1.mpd"));
  dashRequesterHelper.SetAttribute ("BufferSize",UintegerValue(20));

  ndn::AppHelper svcRequesterHelper ("ns3::ndn::SvcRequester");
  //svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l/bbb-svc.264.mpd"));
  svcRequesterHelper.SetAttribute ("BufferSize",UintegerValue(20));

  ApplicationContainer apps;

  for(int i=0; i < streamers.size (); i++)
  {

    if(i % 3 == 0)
    {
      dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical1-svc/artifical1-svc.mpd"));
      svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical1-svc/artifical1-svc.mpd"));
    }
    else if(i % 3 == 1)
    {
      dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical2-svc/artifical2-svc.mpd"));
      svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical2-svc/artifical2-svc.mpd"));
    }
    else
    {
      dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical3-svc/artifical3-svc.mpd"));
      svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical3-svc/artifical3-svc.mpd"));
    }

    if(mode.compare ("adaptation") == 0)
      apps.Add (svcRequesterHelper.Install(streamers.Get (i)));
    else
      apps.Add (dashRequesterHelper.Install(streamers.Get (i)));
  }

  //provider
  ndn::AppHelper cProviderHelper ("ContentProvider");
  cProviderHelper.SetAttribute("ContentPath", StringValue("/data"));
  cProviderHelper.SetAttribute("Prefix", StringValue("/itec/bbb"));
  ApplicationContainer contentProvider = cProviderHelper.Install (providers);
  ndnGlobalRoutingHelper.AddOrigins("/itec/bbb", providers);

  contentProvider.Start (Seconds(0.0));

  srand (0);

  for (ApplicationContainer::Iterator i = apps.Begin (); i != apps.End (); ++i)
  {
    int startTime = rand() % 20 + 1; //1-20

    fprintf(stderr,"starttime = %d\n", startTime);
    ( *i)->SetStartTime(Time::FromInteger (startTime, Time::S));
  }

  // Calculate and install FIBs
  ndn::GlobalRoutingHelper::CalculateAllPossibleRoutes ();

  NS_LOG_UNCOND("Simulation will be started!");

  Simulator::Stop (Seconds (1200.0)); //runs for 20 min.
  Simulator::Run ();
  Simulator::Destroy ();

  NS_LOG_UNCOND("Simulation completed!");
  return 0;
}


