#if !defined(NDEBUG)
#define BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING
#define BOOST_MULTI_INDEX_ENABLE_SAFE_MODE
#endif

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/ranked_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <boost/functional/hash.hpp>
#include <boost/algorithm/string.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

#include <iostream>
#include <fstream>
#include <string>
#include <unordered_set>
#include <functional>
#include <set>

#include "json/json.h"

std::locale defaultLocale("");
// when constructing a locale with a facet arg, the facet is typically
// obtained directly from a new-expression: the locale is responsible
// for calling the matching delete from its own
// destructor. http://en.cppreference.com/w/cpp/locale/locale/locale
std::locale localeWithFacet(defaultLocale,
			    new boost::posix_time::time_input_facet("%Y-%m-%dT%H:%M:%SZ"));

/*------------------------------------------------------------------------------
  Payments are streamed in, parsed into timestamped connections.

  The set of connections by user (singleUserGraphView) is located for each user.

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
    ss.imbue(localeWithFacet);
    ss >> time;
  }

  payment(const std::string& actor_, const std::string& target_, const boost::posix_time::ptime time_)
    : target(target_), actor(actor_), time(time_)
  {}

  std::shared_ptr<const payment> reverse() const
  {
    std::shared_ptr<const payment> result(new payment(target, actor, time));
    return result;
  }

  struct Compare {
    size_t operator () (std::shared_ptr<const payment> p1, std::shared_ptr<const payment> p2) const {
      if (p1->time != p2->time)
	return p1->time < p2->time;
      if (p1->actor != p2->actor)
	return p1->actor < p2->actor;
      if (p1->target != p2->target)
	return p1->target < p2->target;
      return false;
    }
  };

};

struct connection
{
  std::string target;
  boost::posix_time::ptime time;

  connection(std::shared_ptr<const payment> p) :target(p->target), time(p->time) {}


  // Within a specific user's set of connections, the other party's
  // name is sufficient to determine connection equality.
  bool operator == (const connection& c2) const {
    return target == c2.target;
  }

  struct Hash {
    size_t operator () (const connection& c) const {
      std::size_t seed = 0;
      boost::hash_combine(seed, c.target);
      return seed;
    }
  };


  friend std::ostream& operator << (std::ostream &out, const connection& c)
  {
    out << "[" << c.target << "; " << boost::posix_time::to_simple_string(c.time) << "]";
    return out;
  }

};


struct singleUserGraphView
{
  std::string actor;
  std::unordered_set<connection, connection::Hash> connections;

  singleUserGraphView(std::shared_ptr<const payment> p)
    : actor(p->actor)
  {
    connections.insert(connection(p));
  }

  std::size_t degree() const
  {
    return connections.size();
  }

  void addOrUpdateOrIgnoreIfItsAnOldConnection(std::shared_ptr<const payment> p) {
    connection c(p);
    std::unordered_set<connection, connection::Hash>::iterator citer = connections.find(c);
    if (citer == connections.end()) {
      connections.insert(c);
    } else {
      if (citer->time >= c.time) {
	// do nothing
      } else {
	connections.erase(citer);
	connections.insert(c);
      }
    }
    // TODO: check if connection exists
  }

  const std::string debugPrint() const {
    return (boost::format("%1% (%2% conn)") % actor % connections.size()).str();
  }

  friend std::ostream& operator << (std::ostream &out, const singleUserGraphView& uc)
  {
    out << uc.actor << " (" << uc.connections.size() << " connections; ";
    for (std::unordered_set<connection, connection::Hash>::const_iterator i = uc.connections.begin(); i != uc.connections.end(); i++) {
      out << *i << ", ";
    }
    out << std::endl;
    return out;
  }

};

void verboseOutput(const std::string& msg)
{
#if !defined(NDEBUG)
  std::cout << "(debug) " << msg << std::endl;
#endif
}

//------------------------------------------------------------------------------
// HEADER STUFF
//------------------------------------------------------------------------------

// tags
struct actor{};
struct median{};

typedef boost::multi_index_container<
  boost::shared_ptr<singleUserGraphView>,
  boost::multi_index::indexed_by<
    boost::multi_index::ordered_unique<
      boost::multi_index::tag<actor>,
      BOOST_MULTI_INDEX_MEMBER(singleUserGraphView, std::string, actor)
      >,
    boost::multi_index::ranked_non_unique<
      boost::multi_index::tag<median>,
      boost::multi_index::const_mem_fun<singleUserGraphView, std::size_t, &singleUserGraphView::degree>
      >
    >
  > connection_set;

typedef boost::multi_index::index<connection_set,actor>::type connection_set_by_actor;
typedef boost::multi_index::index<connection_set,median>::type connection_set_by_rank;

typedef std::set<std::shared_ptr<const payment>, payment::Compare> payment_set;

//------------------------------------------------------------------------------
// END HEADER STUFF
//------------------------------------------------------------------------------


template<typename Tag, typename MultiIndexContainer>
void print_out_by(const MultiIndexContainer& es)
{
  // get a view to index #1 (name)
  const typename boost::multi_index::index<MultiIndexContainer, Tag>::type& i = boost::multi_index::get<Tag>(es);
  typedef typename MultiIndexContainer::value_type value_type;

  // use name_index as a regular std::set
  std::copy(i.begin(), i.end(), std::ostream_iterator<value_type>(std::cout));
}

void _addOrUpdateConnections_process(std::shared_ptr<const payment> p, connection_set& cs, connection_set_by_actor& index)
{
  connection_set_by_actor::iterator found = index.find(p->actor);

  if (found == index.end()) {
    // dude not found
    cs.insert(boost::shared_ptr<singleUserGraphView>(new singleUserGraphView(p)));
  } else {
    // dude found
    boost::shared_ptr<singleUserGraphView> uc = *found;
    cs.erase(found);

    uc->addOrUpdateOrIgnoreIfItsAnOldConnection(p);

    cs.insert(uc);
  }
}

const boost::posix_time::time_duration timeDuration60(0,1,0,0);
const boost::posix_time::time_duration timeDuration0(0,0,0,0);

void clearConnectionIfEstablishingPaymentIsBeingRemoved(std::shared_ptr<const payment> p, connection_set_by_actor& csIdx) {
  connection_set_by_actor::iterator ucIter = csIdx.find(p->actor);
  if (ucIter == csIdx.end()) {
    // TODO: exit better
    std::cout << "ERROR!!! Did not find user to remove connection from." << std::endl;
    exit(1);
  }
  boost::shared_ptr<singleUserGraphView> uc = *ucIter;

  connection cToMatch(p);
  std::unordered_set<connection, connection::Hash>::const_iterator c = uc->connections.find(cToMatch);

  if (c == uc->connections.end()) {
    // matching connection not found; do nothing
  } else {
    // matching connection found
    if (c->time == cToMatch.time) {
      // same timestamp, remove the connection
      uc->connections.erase(c);
      csIdx.erase(ucIter);
      if (uc->connections.size() > 0) {
        csIdx.insert(uc);
      }
    } else {
      // connection with newer(?) time exists
      // TODO: assert timestamp is newer
    }
  }
}

void purgePaymentSet(payment_set& ps, boost::posix_time::ptime headTime, connection_set_by_actor& csIdx) {
  payment_set::iterator it = ps.begin();
  verboseOutput("PURGING");
  while(headTime - (*it)->time > timeDuration60) {
    verboseOutput((boost::format("  erasing %1% (%2% old, %3% to %4%)\n") % (*it)->time % ((*it)->time - headTime) % (*it)->actor % (*it)->target).str());
    clearConnectionIfEstablishingPaymentIsBeingRemoved(*it, csIdx);
    clearConnectionIfEstablishingPaymentIsBeingRemoved((*it)->reverse(), csIdx);
    it = ps.erase(it);
  }
}

void addOrUpdateConnections(std::shared_ptr<const payment> p, connection_set& cs, payment_set& ps)
{
  // check if new time is older than 60 seconds
  payment_set::reverse_iterator rit = ps.rbegin();
  connection_set_by_actor& index = cs.get<actor>();

  bool inserted = false;

  if (rit != ps.rend()) {
    std::shared_ptr<const payment> newestPayment = *rit;

    if (newestPayment->time - p->time > timeDuration60) {
      // more than 60 seconds behind; do nothing
      verboseOutput("  60 behind; not adding");
    } else {
      inserted = true;
      ps.insert(p);
    }

    if ((p->time - newestPayment->time) > timeDuration0) {
      purgePaymentSet(ps, p->time, index);
    } else {
      // payment out of order, no purge needed
    }

  } else {
    // initializing payment recieved
    inserted = true;
    ps.insert(p);
  }

  if (inserted) {
    _addOrUpdateConnections_process(p, cs, index);
    _addOrUpdateConnections_process(p->reverse(), cs, index);
  }
}




void printRank(const connection_set& cs, std::ofstream& resultsFile) {
  const connection_set_by_rank& index = cs.get<median>();

  std::size_t size = index.size();
  double medianDegree;

  int idx = std::ceil((size / 2.0) - 1);
  connection_set_by_rank::const_iterator it = index.nth(idx);

  if (size % 2 == 0) {
    std::size_t d1 = (*it)->degree(), d2 = (*(++it))->degree();
    medianDegree = (d1 + d2)/2.0;
  } else {
    medianDegree = (*it)->degree();
  }

#if !defined(NDEBUG)
  for (connection_set_by_rank::const_iterator iter = index.begin(); iter != index.end(); iter++) {
    verboseOutput((boost::format("    %1%") % (*iter)->debugPrint()).str());
  }
#endif

  verboseOutput((boost::format("MEDIAN DEGREE: %1%\n") % medianDegree).str());
  resultsFile << std::fixed << std::setprecision(2) << medianDegree << std::endl;
}



int main() {
  std::cout.precision(2);

  connection_set cs;
  payment_set ps;

  Json::Value root;
  Json::Reader jsonReader;
  std::ifstream jstream("../venmo_input/venmo-trans.txt", std::ifstream::binary);
  std::ofstream resultsFile("../venmo_output/output.txt");

  bool parseSuccess = false;
  std::string currline;

  while(std::getline(jstream, currline)) {
    parseSuccess = jsonReader.parse(currline, root, false);
    if (!parseSuccess) {
      verboseOutput("discarding payment input; invalid json");
      verboseOutput("JSONReader Error: " + jsonReader.getFormattedErrorMessages());
      continue;
    }

    // TODO: check for correct payment entry format, discard if invalid
    if (!root.isMember("actor") || boost::trim_copy(root["actor"].asString()) == "") {
      // invalid actor field; passing on this payment entry
      verboseOutput("invalid actor field; passing on this payment entry");
      continue;
    }
    if (!root.isMember("target") || boost::trim_copy(root["target"].asString()) == "") {
      // invalid target field; passing on this payment entry
      verboseOutput("invalid target field; passing on this payment entry");
      continue;
    }
    if (!root.isMember("created_time")) {
      // missing created_time field; passing on this payment
      // validation will happen in payment constructor
      verboseOutput("missing created_time field; passing on this payment entry");
      continue;
    }

    std::shared_ptr<const payment> p(new payment(root["actor"].asString(), root["target"].asString(), root["created_time"].asString()));

    if (p->time.is_not_a_date_time()) {
      // invalid date time; passing on this payment
      verboseOutput("invalid date time; passing on this payment entry");
      continue;
    }

    verboseOutput((boost::format("processed payment: %1% (%2% to %3%)\n") % p->time % p->actor % p->target).str());

    addOrUpdateConnections(p, cs, ps);
    printRank(cs, resultsFile);
  }

  jstream.close();
  resultsFile.close();

  // std::cout << std::endl;
  // print_out_by<actor>(cs);

  // std::cout << std::endl;
  // print_out_by<median>(cs);

  return 0;
}
