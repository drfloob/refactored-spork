#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

#include <iostream>
#include <fstream>
#include <string>
#include <unordered_set>
#include <set>
#include <map>

#include "json/json.h"


/*------------------------------------------------------------------------------
Payments are streamed in, parsed into timestamped connections. 

The set of connections by user (userConnections) is located for each user.

The new connection is added if not already present, otherwise the timestamp is 
updated.
------------------------------------------------------------------------------*/

struct payment
{
  std::string actor;
  std::string target;
  boost::posix_time::ptime time;

  payment(const std::string& actor_, const std::string& target_, const std::string& time_)
    : target(target_), actor(actor_)
  {
    std::stringstream ss(time_);
    // TODO: check for memory leak. new without delete
    boost::posix_time::time_input_facet *dif = new boost::posix_time::time_input_facet("%Y-%m-%dT%H:%M:%SZ");

    ss.imbue(std::locale(ss.getloc(), dif));
      
    ss >> time;
  }
  
  payment(const std::string& actor_, const std::string& target_, const boost::posix_time::ptime time_)
    : target(target_), actor(actor_), time(time_)
  {}

  const payment reverse() const
  {
    return payment(target, actor, time);
  }  
  
  struct Compare {
    size_t operator () (const payment& p1, const payment& p2) const {
      if (p1.time != p2.time)
	return p1.time < p2.time;
      if (p1.actor != p2.actor)
	return p1.actor < p2.actor;
      if (p1.target != p2.target)
	return p1.target < p2.target;
      return false;
    }
  };

};

typedef std::unordered_set<std::string> connection_set;
struct userConnections
{
  std::string actor;
  connection_set connections;

  userConnections(const payment& p)
    : actor(p.actor)
  {
    connections.insert(p.target);
  }

  std::size_t degree() const
  {
    return connections.size();
  }
  
};

//------------------------------------------------------------------------------
// HEADER STUFF
//------------------------------------------------------------------------------

typedef std::set<payment, payment::Compare> payment_set;
typedef std::map<std::string, std::shared_ptr<userConnections>> user_connection_set;

//------------------------------------------------------------------------------
// END HEADER STUFF
//------------------------------------------------------------------------------

boost::posix_time::time_duration timeDuration60(0,1,0,0);
boost::posix_time::time_duration timeDuration0(0,0,0,0);

void purgePaymentSet(payment_set& ps, boost::posix_time::ptime headTime) {
  // std::cout << "  in purge\n";
  payment_set::iterator it = ps.begin();
  while(headTime - it->time >= timeDuration60) {
    // std::cout << boost::format("  purging; time(%1%) actor(%2%) target(%3%)\n") % it->time % it->actor % it->target;
    it = ps.erase(it);
  }
}

void processPayment(const payment& p, payment_set& ps)
{
  // check if new time is older than 60 seconds
  payment_set::reverse_iterator rit = ps.rbegin();
  
  if (rit != ps.rend()) {
    payment newestPayment = *rit;

    if (newestPayment.time - p.time >= timeDuration60) {
      // std::cout << "  more than 60 seconds old\n";
      // more than 60 seconds behind; do nothing
    } else {
      // std::cout << "  insert 1\n";
      // std::cout << boost::format("    inserting; time(%1%) actor(%2%) target(%3%)\n") % p.time % p.actor % p.target;
      // std::cout << boost::format("    ps.size before insert 1(%1%)\n") % ps.size();
      ps.insert(p);
      // std::cout << boost::format("    ps.size after insert 1(%1%)\n") % ps.size();
    }

    if ((p.time - newestPayment.time) > timeDuration0) {
      purgePaymentSet(ps, p.time);
    } else {
      // payment out of order, no purge needed
    }

  } else {
    // initializing payment recieved
      // std::cout << "  insert 2\n";
    ps.insert(p);
  }
  // std::cout << boost::format("    (in processPayment) ps.size(%1%)\n") % ps.size();
}




void printRank(const std::vector<size_t> degrees, std::ofstream& resultsFile) {
  float median = 0;
  int size = degrees.size();
  if (size % 2 == 0) {
    median = (degrees[size/2-1] + degrees[size/2])/2.0;
  } else {
    median = degrees[std::floor(size/2.0)];
  }
  resultsFile << std::fixed << std::setprecision(2) << median << std::endl;
}

void _bcv(const payment& p, user_connection_set& uc) {
  // std::cout << boost::format("  building connection; time(%1%) actor(%2%) target(%3%)\n") % p.time % p.actor % p.target;

  user_connection_set::iterator uiter = uc.find(p.actor);
  if (uiter == uc.end()) {
    std::shared_ptr<userConnections> tmpUc(new userConnections(p));
    uc[p.actor] = tmpUc;
  } else {
    connection_set::const_iterator citer = uiter->second->connections.find(p.target);
    if (citer == uiter->second->connections.end()) {
      // std::cout << "FouND CONNECTION " << p.target << std::endl;
      uiter->second->connections.insert(p.target);
    } else {
      // std::cout << "DID NOT FIND CONNECTION " << p.target << std::endl;
      // connection exists, do nothing
    }
  }
}

void buildConnectionsVector(const payment_set& ps, user_connection_set& uc) {
  // std::cout << boost::format("    ps.size(%1%)\n") % ps.size();
  for(payment_set::const_iterator piter = ps.begin(); piter != ps.end(); piter++) {
    _bcv(*piter, uc);
    _bcv(piter->reverse(), uc);
  }
}

std::vector<size_t> findDegrees(const user_connection_set& uc) {
  std::vector<size_t> degrees;
  for(user_connection_set::const_iterator uciter = uc.begin(); uciter != uc.end(); uciter++) {
    degrees.push_back(uciter->second->degree());
    // std::cout << boost::format("  user(%1%) degree(%2%)\n") % uciter->second->actor % uciter->second->degree();
  }
  // std::cout << std::endl;
  std::sort(degrees.begin(), degrees.end());
  return degrees;
}

int main() {
  std::cout.precision(2);

  payment_set ps;
  user_connection_set uc;  

  Json::Value root;
  Json::Reader jsonReader;
  std::ifstream jstream("../venmo_input/venmo-trans.txt", std::ifstream::binary);
  std::ofstream resultsFile("../venmo_output/output.txt");
  
  bool parseSuccess;
  std::string currline;

  while(std::getline(jstream, currline)) {
    // std::cout << "(debug) read value: " << currline << std::endl;

    parseSuccess = jsonReader.parse(currline, root, false);
    if (!parseSuccess) {
      // std::cout << "discarding payment input; invalid json" << std::endl;
      // std::cout << "JSONReader Error: " << jsonReader.getFormattedErrorMessages() << std::endl;
      continue;
    }

    // TODO: check for correct payment entry format, discard if invalid
    if (!root.isMember("actor") || boost::trim_copy(root["actor"].asString()) == "") {
      // invalid actor field; passing on this payment entry
      // std::cout << "(debug) invalid actor field; passing on this payment entry" << std::endl;
      continue;
    }
    if (!root.isMember("target") || boost::trim_copy(root["target"].asString()) == "") {
      // invalid target field; passing on this payment entry
      // std::cout << "(debug) invalid target field; passing on this payment entry" << std::endl;
      continue;
    }
    if (boost::trim_copy(root["target"].asString()) == boost::trim_copy(root["actor"].asString())) {
      // reflexive payment; passing on this payment entry
      // std::cout << "reflexive payment; passing on this payment entry" << std::endl;
      continue;
    }
    if (!root.isMember("created_time")) {
      // missing created_time field; passing on this payment
      // validation will happen in payment constructor
      // std::cout << "(debug) missing created_time field; passing on this payment entry" << std::endl;
      continue;
    }

    payment p(root["actor"].asString(), root["target"].asString(), root["created_time"].asString());
    
    if (p.time.is_not_a_date_time()) {
      // invalid date time; passing on this payment
      // std::cout << "(debug) invalid date time; passing on this payment entry" << std::endl;
      continue;
    }
    
    // std::cout << boost::format("(debug) processed payment; time(%1%) actor(%2%) target(%3%)\n") % p.time % p.actor % p.target;

    processPayment(p, ps);
    uc.clear();
    buildConnectionsVector(ps, uc);
    std::vector<size_t> degrees = findDegrees(uc);    
    printRank(degrees, resultsFile);
  }

  jstream.close();
  resultsFile.close();

  return 0;
}
