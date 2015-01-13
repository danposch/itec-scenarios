#include "sdncontroller.h"
#include <curl/curl.h>
#include <jsoncpp/json/json.h>



namespace ns3 {
namespace ndn {

using namespace boost::tuples;
using namespace std;

std::map<std::string, std::vector<Ptr<Node> > > SDNController::contentOrigins;
CURL* SDNController::ch;

SDNController::SDNController()
{
    globalRouter = Create<GlobalRouter>();
}

void SDNController::CalculateRoutesForPrefix(Ptr<Node> start, const std::string &prefix)
{
    //find origin node for prefix
    vector<Ptr<Node> > origins = contentOrigins[prefix];
    if (origins.size() > 0)
    {
        //Ptr<Node> origin = origins.at(0);


    }
}

void SDNController::AddOrigins(std::string &prefix, Ptr<Node> producer)
{
    vector<Ptr<Node> > producersForPrefix = contentOrigins[prefix];
    producersForPrefix.push_back(producer);

    //insert content location into graph database

    int prodId = producer->GetId();

    Json::Value neo4jTrx = Json::Value(Json::objectValue);
    Json::Value  statements = Json::Value(Json::arrayValue);

    Json::Value statementObject = Json::Value(Json::objectValue);
    std::stringstream statement;
    statement << "MATCH (n:Node) where n.id = " << prodId
                 << " CREATE UNIQUE (n)-[:PROVIDES]->(p:Prefix {name:'" << prefix << "'}) RETURN p";

    statementObject['statement'] = statement.str();

    statements.append(statementObject);

    neo4jTrx['statements'] = statements;

    PerformNeo4jTrx("http://localhost:7474/db/data/transaction/commit", neo4jTrx.asCString());
}

void SDNController::PushRoute(Route route)
{

}

void SDNController::PerformNeo4jTrx(const std::string &url, const std::string &requestContent)
{
    struct curl_slist *headers = NULL;
    if ((ch = curl_easy_init()) == NULL)
    {
        fprintf(stderr, "ERROR: Failed to create curl handle");
        return;
    }

    headers = curl_slist_append(headers, "Accept: application/json; charset=UTF-8");
    headers = curl_slist_append(headers, "Content-type: application/json");

    curl_easy_setopt(ch, CURLOPT_URL, url)
    curl_easy_setopt(ch, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(ch, CURLOPT_POSTFIELDS, requestContent);

    int rcode = curl_easy_perform(ch);

    if(res != CURLE_OK)
          fprintf(stderr, "curl_easy_perform() failed: %s\n",
                  curl_easy_strerror(res));

    /* always cleanup */
    curl_easy_cleanup(curl);
}

void SDNController::AddLink(Ptr<Node> a,
                            Ptr<Node> b,
                            std::map<std::string, std::string > channelAttributes,
                            std::map<std::string, std::string > deviceAttributes)
{
    int idA = a->GetId();
    int idB = b->GetId();

    Json::Value neo4jTrx = Json::Value(Json::objectValue);
    Json::Value  statements = Json::Value(Json::arrayValue);

    Json::Value statementObject = Json::Value(Json::objectValue);
    std::stringstream statement;
    statement << "MERGE (a:Node {nodeId:'" << idA << "'}) " <<
                 "MERGE (b:Node {nodeId:'" << idB << "'}) " <<
                 "CREATE (a)-[:LINK]-(b) RETURN a";

    statementObject['statement'] = statement.str();

    statements.append(statementObject);

    neo4jTrx['statements'] = statements;

    PerformNeo4jTrx("http://localhost:7474/db/data/transaction/commit", neo4jTrx.asCString());
}

void SDNController::RequestForUnknownPrefix(std::string &prefix)
{

}

void SDNController::NodeReceivedNackOnFace(Ptr<Node>, Ptr<Face>)
{

}

}
}
