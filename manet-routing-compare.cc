/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"

#include "ns3/aodv-helper.h"
//#include "ns3/batmand-helper.h"
#include "ns3/dsdv-helper.h"
#include "ns3/dsr-helper.h"
#include "ns3/gpsr-helper.h"
#include "ns3/olsr-helper.h"

#include "ns3/dsr-main-helper.h"

//#include "ns3/SpiralModel.h"
//#include "ns3/circle-mobility.h"
#include "ns3/flow-monitor-module.h"

#include <unistd.h>
#include <iostream>

using namespace ns3;
using namespace std;

void ReceivePacket (Ptr<Socket> socket)
{
  NS_LOG_UNCOND ("Received one packet!");
}

// static void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize,
//                              uint32_t pktCount, Time pktInterval )
// {
//   if (pktCount > 0)
//     {
//       socket->Send (Create<Packet> (pktSize));
//       Simulator::Schedule (pktInterval, &GenerateTraffic,
//                                       socket, pktSize,pktCount-1, pktInterval);
//     }
//   else
//     {
//       socket->Close ();
//     }
// }

uint32_t recvPacketNumber = 0;
uint32_t sendPacketNumber = 0;
uint32_t recvByte = 0;
int64_t txFirstPacketTime = INT64_MAX;
int64_t rxLastPacketTime = INT64_MIN;
std::vector<int64_t> delay;

uint32_t SentPackets = 0;
uint32_t ReceivedPackets = 0;
uint32_t LostPackets = 0;
  

static void GenerateTraffic(Ptr<Socket> socket,
                                    uint32_t pktSize,
                                    uint32_t pktCount,
                                    Time pktInterval) {
  if (pktCount > 0) {
      Ptr<Packet> pack = Create<Packet>(pktSize);
      DelayJitterEstimation delayJitter;
      delayJitter.PrepareTx(pack);
      socket->Send(pack);
      sendPacketNumber++;
      if (txFirstPacketTime == INT64_MAX) {
          txFirstPacketTime =
              Simulator::Now().GetMilliSeconds();
        }
      Simulator::Schedule(pktInterval, &GenerateTraffic,
                           socket, pktSize, pktCount - 1, pktInterval);
    } else {
      socket->Close();
    }
}


class GpsrExample
{
public:
  GpsrExample ();
  /// Configure script parameters, \return true on successful configuration
  bool Configure (int argc, char **argv);
  /// Run simulation
  void Run ();
  /// Report results
  void Report (std::ostream & os);

private:
  ///\name parameters
  //\{
  /// Number of nodes
  uint32_t size;
  /// Width of the Node Grid
  uint32_t gridWidth;
  /// Distance between nodes, meters
  double step;
  /// Simulation time, seconds
  double totalTime;
  /// Write per-device PCAP traces if true
  bool pcap;
  //\}

  ///\name network
  //\{
  NodeContainer nodes;
  NetDeviceContainer devices;
  Ipv4InterfaceContainer interfaces;

  GpsrHelper gpsr;
  Ipv4ListRoutingHelper list;
  //\}

private:
  void CreateNodes ();
  void CreateDevices ();
  void InstallInternetStack ();
  void InstallApplications ();
};

int main (int argc, char **argv)
{
  GpsrExample test;
  if (! test.Configure(argc, argv))
    NS_FATAL_ERROR ("Configuration failed. Aborted.");

  test.Run ();
  test.Report (std::cout);
  return 0;
}
//-----------------------------------------------------------------------------
GpsrExample::GpsrExample () :
  // Number of Nodes
  size (29),
  // Grid Width
  gridWidth (4),
  // Distance between nodes
  step (100), //TODO Distance changed to the limit between nodes: test to see if there are transmitions
  // Simulation time
  totalTime (100),
  // Generate capture files for each node
  pcap (true)
{
}

bool
GpsrExample::Configure (int argc, char **argv)
{
  // Enable GPSR logs by default. Comment this if too noisy
  //LogComponentEnable("GpsrRoutingProtocol", LOG_LEVEL_INFO);
  //LogComponentEnable("GpsrTable", LOG_LEVEL_INFO);

  SeedManager::SetSeed(12345);
  CommandLine cmd;

  cmd.AddValue ("pcap", "Write PCAP traces.", pcap);
  cmd.AddValue ("size", "Number of nodes.", size);
  cmd.AddValue ("time", "Simulation time, s.", totalTime);
  cmd.AddValue ("step", "Grid step, m", step);

  cmd.Parse (argc, argv);
  return true;
}

void showPosition (NodeContainer nodeContainer, double deltaTime)
{//node.Begin();
  //NodeContainer ::iterator it;
  
   //Ptr<Node> node=nodeContainer.Get(0);
  //uint32_t nodeId = node->GetId ();
  //Ptr<MobilityModel> mobModel = node->GetObject<MobilityModel> ();
  //mobModel->SetPosition(Vector(0.0,0.0,0.0));	

  
  for( int i=0;i!=29;i++ ){
  Ptr<Node> node=nodeContainer.Get(i);
  uint32_t nodeId = node->GetId ();
  Ptr<MobilityModel> mobModel = node->GetObject<MobilityModel> ();
  Vector3D pos = mobModel->GetPosition ();
  Vector3D speed = mobModel->GetVelocity ();
  std::cout << "At " << Simulator::Now ().GetSeconds () << " node " << nodeId
            << ": Position(" << pos.x << ", " << pos.y << ", " << pos.z
            << ");   Speed(" << speed.x << ", " << speed.y << ", " << speed.z
            << ")" << std::endl;
  }
  Simulator::Schedule (Seconds (deltaTime), &showPosition, nodeContainer, deltaTime);
}

void
GpsrExample::Run ()
{
  //Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue (1)); // enable rts cts all the time.
  CreateNodes ();
  CreateDevices ();
  InstallInternetStack ();
  InstallApplications ();

  // GpsrHelper gpsr;
  gpsr.Install ();
  
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  std::cout << "Starting simulation for " << totalTime << " s ...\n";


Simulator::Schedule (Seconds (0), &showPosition, nodes, 1);
  Simulator::Stop (Seconds (totalTime));
  
  Simulator::Run ();


  int j = 0;
  float AvgThroughput = 0;
  Time Jitter;
  Time Delay;

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter)
  {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);

      NS_LOG_UNCOND("----Flow ID:" << iter->first);
      NS_LOG_UNCOND("Src Addr" << t.sourceAddress << "Dst Addr " << t.destinationAddress);
      NS_LOG_UNCOND("Sent Packets=" << iter->second.txPackets);
      NS_LOG_UNCOND("Received Packets =" << iter->second.rxPackets);
      NS_LOG_UNCOND("Lost Packets =" << iter->second.txPackets - iter->second.rxPackets);
      NS_LOG_UNCOND("Packet delivery ratio =" << iter->second.rxPackets * 100 / iter->second.txPackets << "%");
      NS_LOG_UNCOND("Packet loss ratio =" << (iter->second.txPackets - iter->second.rxPackets) * 100 / iter->second.txPackets << "%");
      NS_LOG_UNCOND("Delay =" << iter->second.delaySum);
      NS_LOG_UNCOND("Jitter =" << iter->second.jitterSum);
      NS_LOG_UNCOND("Throughput =" << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1024 << "Kbps");

      //NS_LOG_UNCOND("Postion =" << ns2.GetMobilityModel);

      

      SentPackets = SentPackets + (iter->second.txPackets);
      ReceivedPackets = ReceivedPackets + (iter->second.rxPackets);
      LostPackets = LostPackets + (iter->second.txPackets - iter->second.rxPackets);
      AvgThroughput = AvgThroughput + (iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1024);
      Delay = Delay + (iter->second.delaySum);
      Jitter = Jitter + (iter->second.jitterSum);

      j = j + 1;

  }

  AvgThroughput = AvgThroughput / j;
  NS_LOG_UNCOND("--------Total Results of the simulation----------" << std::endl);
  NS_LOG_UNCOND("Total sent packets  =" << SentPackets);
  NS_LOG_UNCOND("Total Received Packets =" << ReceivedPackets);
  NS_LOG_UNCOND("Total Lost Packets =" << LostPackets);
  NS_LOG_UNCOND("Packet Loss ratio =" << ((LostPackets * 100) / SentPackets) << "%");
  NS_LOG_UNCOND("Packet delivery ratio =" << ((ReceivedPackets * 100) / SentPackets) << "%");
  NS_LOG_UNCOND("Average Throughput =" << AvgThroughput << "Kbps");
  NS_LOG_UNCOND("End to End Delay =" << Delay);
  NS_LOG_UNCOND("End to End Jitter delay =" << Jitter);
  NS_LOG_UNCOND("Total Flod id " << j);
  monitor->SerializeToXmlFile("manet-routing.xml", true, true);
  
  Simulator::Destroy ();
}

void
GpsrExample::Report (std::ostream &)
{
}

void
GpsrExample::CreateNodes ()
{
  std::cout << "Creating " << (unsigned)size << " nodes " << step << " m apart.\n";
  nodes.Create (size);
  // Name nodes
  for (uint32_t i = 0; i < size; ++i)
     {
       std::ostringstream os;
       // Set the Node name to the corresponding IP host address
       os << "node-" << i+1;
       Names::Add (os.str (), nodes.Get (i));
     }


  // // Create static grid
  // MobilityHelper mobility;
  // mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
  //                               "MinX", DoubleValue (0.0),
  //                               "MinY", DoubleValue (0.0),
  //                               "DeltaX", DoubleValue (step),
  //                               "DeltaY", DoubleValue (step),
  //                               "GridWidth", UintegerValue (gridWidth),
  //                               "LayoutType", StringValue ("RowFirst"));
  // mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  // mobility.Install (nodes);

  std::string traceFile = "scratch/2_ns20115-2.tcl";
   Ns2MobilityHelper ns2 = Ns2MobilityHelper (traceFile);
  ns2.Install(nodes.Begin(), nodes.End());

  
}

void
GpsrExample::CreateDevices ()
{
    WifiHelper wifi;
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    /* 设置wifi物理信息 */
    /* 设置接受增益 */
    wifiPhy.Set("RxGain", DoubleValue(-10));
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    // wifi.SetRemoteStationManager(ns3::WifiRemoteStationManager);
    YansWifiChannelHelper wifiChannel;
    /* 设置传播时延 */
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    /* 设置路径损失和最大传输距离 */
    wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange",
                                   DoubleValue(250.0));
    wifiPhy.SetChannel(wifiChannel.Create());
    WifiMacHelper wifiMac;

    /* 设置wifi标准 */
    // wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode",
                                 StringValue("OfdmRate6Mbps"),
                                 "RtsCtsThreshold", UintegerValue(1024));
    /* 设置自组网物理地址模型 */
    wifiMac.SetType("ns3::AdhocWifiMac");
    devices = wifi.Install(wifiPhy, wifiMac, nodes);

  // Enable Captures, if necessary
  if (pcap)
    {
      // wifiPhy.EnablePcapAll (std::string ("gpsr"));
      wifiPhy.EnablePcap("ns3-gpsr", devices);
    }
}

void
GpsrExample::InstallInternetStack ()
{
  // GpsrHelper gpsr;
  // you can configure GPSR attributes here using gpsr.Set(name, value)
  list.Add((const Ipv4RoutingHelper &)gpsr, 100);
  InternetStackHelper stack;
  stack.SetRoutingHelper (list);
  stack.Install (nodes);
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  interfaces = address.Assign (devices);
}

void
GpsrExample::InstallApplications ()
{

  uint16_t port = 10000;  // well-known echo port number
  uint32_t packetSize = 512; // size of the exchanged packets
  uint32_t maxPacketCount = 7; // number of packets to transmit
  Time interPacketInterval = Seconds (5); // interval between packet transmitions

  // Set-up a server Application, to be run on the bottom-right node of the grid
  UdpEchoServerHelper server (port);
  uint16_t serverPosition = 1; // 服务节点 为 node1
  ApplicationContainer apps = server.Install (nodes.Get(serverPosition));
  apps.Start (Seconds (0.0)); // Server Start Time
  apps.Stop (Seconds (totalTime-0.1)); // Server Stop Time

  // Set-up a client Application, connected to 'server', to be run on the bottom-left node of the grid
  UdpEchoClientHelper client (interfaces.GetAddress (serverPosition), port);
  client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));

  NodeContainer clientNodes;

  uint16_t clientPostion = 0; //客户端节点 为 node0
  apps = client.Install (nodes.Get (clientPostion));
  apps.Start (Seconds (82.0 - 0.01)); // Client Start Time
  apps.Stop (Seconds (totalTime-0.1)); // Client Stop Time
}
