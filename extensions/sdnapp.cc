#include "sdnapp.h"
#include "utils/sdncontroller.h"
#include "utils/sdncontentrequester.h"
#include "ns3/ndn-app-face.h"
#include "ns3/random-variable.h"

NS_LOG_COMPONENT_DEFINE ("SDNApp");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (SDNApp);

using namespace ndn::fw;

TypeId SDNApp::GetTypeId()
{
    static TypeId tid = TypeId("SDNApp")
            .SetParent<ndn::App>()
            .AddConstructor<SDNApp>();
    return tid;
}

void SDNApp::StartApplication()
{
    ndn::App::StartApplication();
    RegisterAtController();
}

void SDNApp::StopApplication()
{
    ndn::App::StopApplication();
}

void SDNApp::SendInterest(std::string name, uint32_t seqNum)
{
    Ptr<ndn::Name> prefix = Create<ndn::Name> (name);
    prefix->appendSeqNum(seqNum);

    UniformVariable rand(0, std::numeric_limits<uint32_t>::max());

    Ptr<ndn::Interest> interest = Create<ndn::Interest>();
    interest->SetName(prefix);
    interest->SetInterestLifetime(Seconds(1.0));
    interest->SetNonce(rand.GetValue());

    m_transmittedInterests (interest, this, m_face);
    m_face->ReceiveInterest(interest);
}

void SDNApp::OnInterest(Ptr<const ndn::Interest> interest)
{

}

void SDNApp::OnData(Ptr<const ndn::Data> contentObject)
{
    //NS_LOG_DEBUG ("Receiving Data packet for " << contentObject->GetName ());
    ndn::App::OnData(contentObject);
}

void SDNApp::RegisterAtController()
{
    //Ptr<Node> node = this->GetObject<Node>();
    SDNController::RegisterApp(this, GetNode()->GetId());
}

void SDNApp::RequestContent(const std::string &name, int dataRate)
{
    std::cout << "Requesting content " << name << "\n";
    std::string nameCpy(name);
    SDNContentRequester *requester = new SDNContentRequester(this, nameCpy, dataRate);

    contentRequesters.push_back(requester);
    requester->RequestContent(name, dataRate);
}

}
