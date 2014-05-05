#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/point-to-point-module.h"

#include "ns3/ndn-app.h"
#include "ns3/nstime.h"
#include "ns3/random-variable.h"
#include <stdlib.h>
#include <string.h>

#include <boost/lexical_cast.hpp>

using namespace ns3;

void parseParameters(int argc, char* argv[], std::string& mode)
{
  bool v0 = false, v1 = false, v2 = false;
  bool vN = false;

  std::string top_path = "topologies/congavoid_100clients.top";

  CommandLine cmd;
  cmd.AddValue ("v0", "Prints all log messages >= LOG_DEBUG. (OPTIONAL)", v0);
  cmd.AddValue ("v1", "Prints all log messages >= LOG_INFO. (OPTIONAL)", v1);
  cmd.AddValue ("v2", "Prints all log messages. (OPTIONAL)", v2);
  cmd.AddValue ("vN", "Disable all internal logging parameters, use NS_LOG instead", vN);
  cmd.AddValue ("top", "Path to the topology file. (OPTIONAL)", top_path);
  cmd.AddValue ("mode", "Sets the simulation mode. Either \"mode=dash-svc\" or \"mode=dash-avc\ or \"mode=adaptation\". (OPTIONAL) Default: mode=dash-svc", mode);

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
  int client_map[100] = { 66,97,7,96,52,74,28,11,91,27,60,19,84,15,23,82,72,56,57,70,33,87,25,76,2,50,90,64,65,71,49,59,85,37,58,0,42,17,1,51,94,63,69,3,61,16,75,79,68,98,67,12,38,41,10,93,95,55,45,73,35,20,32,30,29,86,89,34,77,8,43,26,78,46,81,5,48,13,92,44,9,24,36,21,80,39,14,47,54,40,99,62,88,53,22,31,6,83,4,18 };


  NS_LOG_COMPONENT_DEFINE ("LargeScenario");

  std::string mode("dash-svc");
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
    {
      adaptiveNodes.Add (adaptiveNode);
      fprintf(stderr, "adding node to adaptiveNode\n");
    }
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
  /*ndnHelper.SetForwardingStrategy("ns3::ndn::fw::BestRoute::SVCStaticStrategy",
                                  "EnableNACKs", "true", "MaxLevelAllowed", "6"); */
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

  // Spreading 10 Videos to 100 clients:
  // V1: 1 client ( 1 )
  // V2: 1 client ( 2 )
  // ...
  // V6: 1 client ( 6 )
  // V7: 15 clients ( 21 )
  // V8: 25 clients ( 46 )
  // V9: 27 clients ( 73 )
  // V10: 27 clients ( 100 )

  int distribution[] = {1,2,3,5,7,11,19,35,67,100};

  for(int i=0; i < streamers.size (); i++)
  {
    if(i < distribution[0])
    {
      if(mode.compare ("dash-avc") == 0)
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical0-avc/artifical0-avc.mpd"));
      else
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical0-svc/artifical0-svc.mpd"));
      svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical0-svc/artifical0-svc.mpd"));
    }
    else if(i < distribution[1])
    {
      if(mode.compare ("dash-avc") == 0)
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical1-avc/artifical1-avc.mpd"));
      else
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical1-svc/artifical1-svc.mpd"));
      svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical1-svc/artifical1-svc.mpd"));
    }
    else if(i < distribution[2])
    {
      if(mode.compare ("dash-avc") == 0)
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical2-avc/artifical2-avc.mpd"));
      else
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical2-svc/artifical2-svc.mpd"));
      svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical2-svc/artifical2-svc.mpd"));
    }
    else if(i < distribution[3])
    {
      if(mode.compare ("dash-avc") == 0)
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical3-avc/artifical3-avc.mpd"));
      else
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical3-svc/artifical3-svc.mpd"));
      svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical3-svc/artifical3-svc.mpd"));
    }
    else if(i < distribution[4])
    {
      if(mode.compare ("dash-avc") == 0)
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical4-avc/artifical4-avc.mpd"));
      else
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical4-svc/artifical4-svc.mpd"));
      svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical4-svc/artifical4-svc.mpd"));
    }
    else if(i < distribution[5])
    {
      if(mode.compare ("dash-avc") == 0)
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical5-avc/artifical5-avc.mpd"));
      else
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical5-svc/artifical5-svc.mpd"));
      svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical5-svc/artifical5-svc.mpd"));
    }
    else if(i < distribution[6])
    {
      if(mode.compare ("dash-avc") == 0)
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical6-avc/artifical6-avc.mpd"));
      else
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical6-svc/artifical6-svc.mpd"));
      svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical6-svc/artifical6-svc.mpd"));
    }
    else if(i < distribution[7])
    {
      if(mode.compare ("dash-avc") == 0)
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical7-avc/artifical7-avc.mpd"));
      else
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical7-svc/artifical7-svc.mpd"));
      svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical7-svc/artifical7-svc.mpd"));
    }
    else if(i < distribution[8])
    {
      if(mode.compare ("dash-avc") == 0)
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical8-avc/artifical8-avc.mpd"));
      else
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical8-svc/artifical8-svc.mpd"));
      svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical8-svc/artifical8-svc.mpd"));
    }
    else if(i < distribution[9])
    {
      if(mode.compare ("dash-avc") == 0)
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical9-avc/artifical9-avc.mpd"));
      else
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical9-svc/artifical9-svc.mpd"));
      svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical9-svc/artifical9-svc.mpd"));
    }

    if(mode.compare ("adaptation") == 0)
      apps.Add (svcRequesterHelper.Install(streamers.Get (client_map[i])));
    else
      apps.Add (dashRequesterHelper.Install(streamers.Get (client_map[i])));
  }

  /*for(int i=0; i < streamers.size (); i++)
  {

    switch(i % 10)
    {
      case 1:
      {
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical1-svc/artifical1-svc.mpd"));
        svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical1-svc/artifical1-svc.mpd"));
        break;
      }
      case 2:
      {
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical2-svc/artifical2-svc.mpd"));
        svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical2-svc/artifical2-svc.mpd"));
        break;
      }
      case 3:
      {
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical3-svc/artifical3-svc.mpd"));
        svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical3-svc/artifical3-svc.mpd"));
        break;
      }
      case 4:
      {
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical4-svc/artifical4-svc.mpd"));
        svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical4-svc/artifical4-svc.mpd"));
        break;
      }
      case 5:
      {
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical5-svc/artifical5-svc.mpd"));
        svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical5-svc/artifical5-svc.mpd"));
        break;
      }
      case 6:
      {
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical6-svc/artifical6-svc.mpd"));
        svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical6-svc/artifical6-svc.mpd"));
        break;
      }
      case 7:
      {
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical7-svc/artifical7-svc.mpd"));
        svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical7-svc/artifical7-svc.mpd"));
        break;
      }
      case 8:
      {
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical8-svc/artifical8-svc.mpd"));
        svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical8-svc/artifical8-svc.mpd"));
        break;
      }
      case 9:
      {
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical9-svc/artifical9-svc.mpd"));
        svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical9-svc/artifical9-svc.mpd"));
        break;
      }
      default:
      {
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical0-svc/artifical0-svc.mpd"));
        svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical0-svc/artifical0-svc.mpd"));
      }
    }

    if(mode.compare ("adaptation") == 0)
      apps.Add (svcRequesterHelper.Install(streamers.Get (i)));
    else
      apps.Add (dashRequesterHelper.Install(streamers.Get (i)));
  }*/

  //provider
  ndn::AppHelper cProviderHelper ("ContentProvider");
  cProviderHelper.SetAttribute("ContentPath", StringValue("/data"));
  cProviderHelper.SetAttribute("Prefix", StringValue("/itec/bbb"));
  ApplicationContainer contentProvider = cProviderHelper.Install (providers);
  ndnGlobalRoutingHelper.AddOrigins("/itec/bbb", providers);

  contentProvider.Start (Seconds(0.0));

  //srand (1);
  //mean, upper-limit
  ns3::ExponentialVariable exp(12,30);

  for (ApplicationContainer::Iterator i = apps.Begin (); i != apps.End (); ++i)
  {
    //int startTime = rand() % 30 + 1; //1-30
    //int startTime = exp.GetInteger () + 1;
    double startTime = exp.GetValue() + 1;

    //fprintf(stderr,"starttime = %d\n", startTime);
    //fprintf(stderr,"starttime = %f\n", startTime);
    //( *i)->SetStartTime(Time::FromInteger (startTime, Time::S));
    ( *i)->SetStartTime(Time::FromDouble(startTime, Time::S));
  }

  // Calculate and install FIBs
  ndn::GlobalRoutingHelper::CalculateAllPossibleRoutes ();

  NS_LOG_UNCOND("Simulation will be started!");

  Simulator::Stop (Seconds (1800.0)); //runs for 30 min.
  Simulator::Run ();
  Simulator::Destroy ();

  NS_LOG_UNCOND("Simulation completed!");
  return 0;
}


