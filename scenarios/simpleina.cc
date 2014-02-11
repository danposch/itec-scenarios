#include "ns3-dev/ns3/core-module.h"
#include "ns3-dev/ns3/network-module.h"
#include "ns3-dev/ns3/ndnSIM-module.h"
#include "ns3-dev/ns3/point-to-point-module.h"

using namespace ns3;

void parseParameters(int argc, char* argv[])
{
  bool v0 = false, v1 = false, v2 = false;
  std::string top_path = "topologies/simple_ina.top";

  CommandLine cmd;
  cmd.AddValue ("v0", "Prints all log messages >= LOG_DEBUG. (OPTIONAL)", v0);
  cmd.AddValue ("v1", "Prints all log messages >= LOG_INFO. (OPTIONAL)", v1);
  cmd.AddValue ("v2", "Prints all log messages. (OPTIONAL)", v2);
  cmd.AddValue ("top", "Path to the topology file. (OPTIONAL)", top_path);

  cmd.Parse (argc, argv);

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
  AnnotatedTopologyReader topologyReader ("", 20);
  topologyReader.SetFileName (top_path);
  topologyReader.Read();
}

int main(int argc, char* argv[])
{
  NS_LOG_COMPONENT_DEFINE ("SimpleINA");

  parseParameters(argc, argv);

  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.SetForwardingStrategy ("ns3::ndn::fw::BestRoute");
  ndnHelper.InstallAll ();

  // Installing global routing interface on all nodes
  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.InstallAll ();

  // fetch src und dst nodes
  Ptr<Node> contentDst = Names::Find<Node>("ContentDst");
  Ptr<Node> dummyDst = Names::Find<Node>("DummyDst");
  Ptr<Node> adaptiveNode = Names::Find<Node>("AdaptiveNode");
  Ptr<Node> simpleNode = Names::Find<Node>("SimpleNode");
  Ptr<Node> dummySrc = Names::Find<Node>("DummySrc");
  Ptr<Node> contentSrc = Names::Find<Node>("ContentSrc");

  //background traffix
  ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerCbr");
  consumerHelper.SetPrefix ("/dummy/data");
  consumerHelper.SetAttribute ("Frequency", StringValue("30")); //30 interests a second (ca. 1MBit traffic)
  ApplicationContainer dummyConsumer = consumerHelper.Install(dummyDst);

  ndn::AppHelper producerHelper ("ns3::ndn::Producer");
  producerHelper.SetPrefix ("/dummy");
  producerHelper.SetAttribute ("PayloadSize", StringValue("4096"));
  ApplicationContainer dummyProducer = producerHelper.Install(dummySrc); // last node

  std::string prefix = "/dummy";
  ndnGlobalRoutingHelper.AddOrigins(prefix, dummySrc);

  dummyProducer.Start (Seconds(0.0));
  dummyConsumer.Start (Seconds(5.0));
  dummyConsumer.Stop (Seconds(10.0));


  //multimedia traffix
  ndn::AppHelper dashRequesterHelper ("ns3::ndn::DashRequester");
  dashRequesterHelper.SetAttribute ("MPD",StringValue("/data/bunny_2s_480p_only/bunny_Desktop.mpd"));
  dashRequesterHelper.SetAttribute ("InterestPipeline",UintegerValue(4));
  dashRequesterHelper.SetAttribute ("BufferSize",UintegerValue(20));
  ApplicationContainer dashContainer = dashRequesterHelper.Install(contentDst);

  dashContainer.Start (Seconds(0.0));

  // Calculate and install FIBs
  ndn::GlobalRoutingHelper::CalculateAllPossibleRoutes ();


  NS_LOG_UNCOND("Simulation will be started!");

  Simulator::Stop (Seconds (100.0));
  Simulator::Run ();
  Simulator::Destroy ();

  NS_LOG_UNCOND("Simulation completed!");
  return 0;
}

