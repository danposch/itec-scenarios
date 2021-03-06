// ndn-simple.cc

#include <cstdio>

#include "ns3-dev/ns3/core-module.h"
#include "ns3-dev/ns3/network-module.h"
#include "ns3-dev/ns3/point-to-point-module.h"
#include "ns3-dev/ns3/ndnSIM-module.h"

#include "ns3-dev/ns3/ndnSIM/model/cs/content-store-impl.h"
#include "ns3-dev/ns3/ndnSIM/utils/trie/fifo-policy.h"

using namespace ns3;

void init(int argc, char *argv[])
{
    fprintf(stderr, "Launching Example Simulation\n");

    // setting default parameters for PointToPoint links and channels
    Config::SetDefault ("ns3::PointToPointNetDevice::DataRate", StringValue ("1Mbps"));
    Config::SetDefault ("ns3::PointToPointChannel::Delay", StringValue ("10ms"));
    Config::SetDefault ("ns3::DropTailQueue::MaxPackets", StringValue ("20"));

    // Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
    CommandLine cmd;
    cmd.Parse (argc, argv);

    // Creating nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Connecting nodes using two links
    PointToPointHelper p2p;
    p2p.Install (nodes.Get (0), nodes.Get (2));
    p2p.Install (nodes.Get (1), nodes.Get (2));
    p2p.Install (nodes.Get (2), nodes.Get (3));

    // Install NDN stack on all nodes
    ndn::StackHelper ndnHelper;
    ndnHelper.SetDefaultRoutes (true);
    ndnHelper.SetContentStore("ns3::ndn::cs::Random");
    ndnHelper.SetForwardingStrategy ("ns3::ndn::fw::BestRoute");
    ndnHelper.Install(nodes);

    ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
    ndnGlobalRoutingHelper.Install(nodes);

    // Getting containers for the consumer/producer
    Ptr<Node> consumer1 = nodes.Get(0);
    Ptr<Node> consumer2 = nodes.Get(1);
    Ptr<Node> producer = nodes.Get(3);

    // Install NDN applications
    ndn::AppHelper helperSink ("Sink");
    ndn::AppHelper helperSource ("Source");
    ApplicationContainer sink1 = helperSink.Install(consumer1);
    ApplicationContainer sink2 = helperSink.Install(consumer2);
    ApplicationContainer source = helperSource.Install(producer);

    //Add /prefix origins to ndn::GlobalRouter
    std::string prefix = "/source";
    ndnGlobalRoutingHelper.AddOrigins(prefix, producer);
    // Calculate and install FIBs
    ndn::GlobalRoutingHelper::CalculateRoutes ();

    source.Start (Seconds (0.0)); // make source ready
    sink1.Start (Seconds (0.1)); // will send out Interest
    sink2.Start (Seconds (1.0)); // will send out Interest

    Simulator::Stop (Seconds (5.0));

    Simulator::Run ();
    Simulator::Destroy ();

    fprintf(stderr, "Simulation completed! Yeah! \n");
}

int main (int argc, char *argv[])
{
	init(argc, argv);
  
  return 0;
}
