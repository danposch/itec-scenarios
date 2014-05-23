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

#include "../extensions/utils/idownloader.h"

using namespace ns3;

void parseParameters(int argc, char* argv[], std::string& mode, std::string& cwnd)
{
  bool v0 = false, v1 = false, v2 = false;
  bool vN = false;

  std::string top_path = "congavoid_10clients.top";

  cwnd = "tcp";

  CommandLine cmd;
  cmd.AddValue ("v0", "Prints all log messages >= LOG_DEBUG. (OPTIONAL)", v0);
  cmd.AddValue ("v1", "Prints all log messages >= LOG_INFO. (OPTIONAL)", v1);
  cmd.AddValue ("v2", "Prints all log messages. (OPTIONAL)", v2);
  cmd.AddValue ("vN", "Disable all internal logging parameters, use NS_LOG instead", vN);
  cmd.AddValue ("top", "Path to the topology file. (OPTIONAL)", top_path);
  cmd.AddValue ("cwnd", "Type of congestion window to be used (either tcp (default) or static) (OPTIONAL)", cwnd);
  cmd.AddValue ("mode", "Sets the simulation mode. Either \"mode=dash-svc\" or \"mode=dash-avc or \"mode=adaptation\". (OPTIONAL) Default: mode=dash-svc", mode);

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
  NS_LOG_UNCOND("Using topology file " << top_path);
  topologyReader.SetFileName ("topologies/" + top_path);
  topologyReader.Read();
}

int main(int argc, char* argv[])
{
  NS_LOG_COMPONENT_DEFINE ("CongAvoidScenario");

  std::string mode("dash-svc");
  std::string cwnd("tcp");

  parseParameters(argc, argv, mode, cwnd);

  fprintf(stderr, "Selected Mode = %s\n", mode.c_str ());

  NodeContainer streamers;
  int nodeIndex = 0;
  std::string nodeNamePrefix("ContentDst");
  Ptr<Node> contentDst = Names::Find<Node>(nodeNamePrefix +  boost::lexical_cast<std::string>(nodeIndex++));
  while(contentDst != NULL)
  {
    streamers.Add (contentDst);
    contentDst =  Names::Find<Node>(nodeNamePrefix +  boost::lexical_cast<std::string>(nodeIndex++));
  }


  //random permutation
  NS_LOG_UNCOND("RngRun=" << ns3::SeedManager::GetRun ());
  srand(ns3::SeedManager::GetRun ());

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

  // the content provider should get a huge cache to avoid a lot of hard disk access
  ndnHelper.SetContentStore ("ns3::ndn::cs::Stats::Lru","MaxSize", "1000000");
  ndnHelper.Install (providers);

  ndnHelper.SetContentStore ("ns3::ndn::cs::Stats::Lru","MaxSize", "1000"); // all entities can store up to 1k chunks in cache (about 4MB)
  ndnHelper.Install (streamers);

  ndnHelper.SetContentStore ("ns3::ndn::cs::Stats::Lru","MaxSize", "25000"); // all entities can store up to 25k chunks in cache (about 100MB)
  ndnHelper.Install (routers);

  //change strategy for adaptive NODE
  ndnHelper.SetForwardingStrategy("ns3::ndn::fw::BestRoute::SVCLiveCountingStrategy",
                                  "EnableNACKs", "true", "LevelCount", "6");
  /*ndnHelper.SetForwardingStrategy("ns3::ndn::fw::BestRoute::SVCStaticStrategy",
                                  "EnableNACKs", "true", "MaxLevelAllowed", "6"); */
  ndnHelper.EnableLimits (false);
  ndnHelper.Install (adaptiveNodes);

  // Installing global routing interface on all nodes
  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.InstallAll ();

   //consumer
  //ndn::AppHelper dashRequesterHelper ("ns3::ndn::DashRequester");
  ndn::AppHelper dashRequesterHelper ("ns3::ndn::PlayerRequester");
  //dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l/bbb-svc.264.mpd"));
  //dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/artifical1/artifical1.mpd"));
  //dashRequesterHelper.SetAttribute ("BufferSize",UintegerValue(20));


  /* SimpleNDN,
      WindowNDN,
      SVCWindowNDN
      */
  if (mode.compare("adaptation") == 0)
  {
    dashRequesterHelper.SetAttribute("EnableAdaptation", StringValue("1"));
  } else {
    dashRequesterHelper.SetAttribute("EnableAdaptation", StringValue("0"));
  }

  dashRequesterHelper.SetAttribute("CongestionWindowType", StringValue(cwnd));

  /*

  ndn::AppHelper svcRequesterHelper ("ns3::ndn::SvcRequester");
  //svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l/bbb-svc.264.mpd"));
  svcRequesterHelper.SetAttribute ("BufferSize",UintegerValue(20));
  */
  ApplicationContainer apps;

  // Spreading 10 Videos to 10 clients:
  // V1: 1 client ( 1 )
  // V2: 1 client ( 2 )
  // V3. 1 clients ( 3 )
  // V4. 1 clients ( 4 )
  // V5: 1 clients ( 5 )
  // V6: 1 clients ( 6 )
  // V7: 1 clients ( 7 )
  // V8: 1 clients ( 8 )
  // V9: 1 clients ( 9 )
  // V10: 1 clients ( 10 )

  int distribution[] = { 0,1,2,3,4,5,6,7,8,9,10 };


  for(int i=0; i < streamers.size (); i++)
    {
      if(mode.compare ("dash-avc")==0)
      {
        fprintf(stderr, "mode not supported\n");
        exit(-1);
      }

      if(i < distribution[0])
      {
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l_set0/bbb-svc.264_set0.mpd"));
        //svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l_set0/bbb-svc.264_set0.mpd"));
      }
      else if(i < distribution[1])
      {
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l_set1/bbb-svc.264_set1.mpd"));
        //svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l_set1/bbb-svc.264_set1.mpd"));
      }
      else if(i < distribution[2])
      {
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l_set2/bbb-svc.264_set2.mpd"));
        //svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l_set2/bbb-svc.264_set2.mpd"));
      }
      else if(i < distribution[3])
      {
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l_set3/bbb-svc.264_set3.mpd"));
        //svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l_set3/bbb-svc.264_set3.mpd"));
      }
      else if(i < distribution[4])
      {
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l_set4/bbb-svc.264_set4.mpd"));
        //svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l_set4/bbb-svc.264_set4.mpd"));
      }
      else if(i < distribution[5])
      {
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l_set5/bbb-svc.264_set5.mpd"));
        //svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l_set5/bbb-svc.264_set5.mpd"));
      }
      else if(i < distribution[6])
      {
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l_set6/bbb-svc.264_set6.mpd"));
        //svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l_set6/bbb-svc.264_set6.mpd"));
      }
      else if(i < distribution[7])
      {
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l_set7/bbb-svc.264_set7.mpd"));
        //svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l_set7/bbb-svc.264_set7.mpd"));
      }
      else if(i < distribution[8])
      {
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l_set8/bbb-svc.264_set8.mpd"));
        //svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l_set8/bbb-svc.264_set8.mpd"));
      }
      else if(i < distribution[9])
      {
        dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l_set9/bbb-svc.264_set9.mpd"));
        //svcRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_svc_snr_2s_6l_set9/bbb-svc.264_set9.mpd"));
      }

      /*if(mode.compare ("adaptation") == 0)
        apps.Add (svcRequesterHelper.Install(streamers.Get (i)));
      else*/
        apps.Add (dashRequesterHelper.Install(streamers.Get (i)));
    }


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
  Time stopTime = Seconds (3600.0);


  fprintf(stderr, "StartTime=");

  for (ApplicationContainer::Iterator i = apps.Begin (); i != apps.End (); ++i)
  {
    //int startTime = rand() % 30 + 1; //1-30
    //int startTime = exp.GetInteger () + 1;
    double startTime = exp.GetValue() + 1;

    //fprintf(stderr,"starttime = %d\n", startTime);
    //fprintf(stderr,"starttime = %f\n", startTime);
    //( *i)->SetStartTime(Time::FromInteger (startTime, Time::S));
    ( *i)->SetStartTime(Time::FromDouble(startTime, Time::S));
    ( *i)->SetStopTime(stopTime);

    fprintf(stderr, "%f,", startTime);
  }
  fprintf(stderr, "\n");

  // Calculate and install FIBs
  ndn::GlobalRoutingHelper::CalculateAllPossibleRoutes ();

  NS_LOG_UNCOND("Simulation will be started!");

  Simulator::Stop (Seconds(stopTime.GetSeconds ()+1)); //runs for 60 min.
  Simulator::Run ();
  Simulator::Destroy ();

  NS_LOG_UNCOND("Simulation completed!");
  return 0;
}