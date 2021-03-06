// -*- c++-mode -*-

#include <queue>
#include <string>

#include <event2/bufferevent.h>
#include <event2/dns.h>
#include <event2/event.h>
#include <event2/util.h>

#include "AdaptiveSampler.h"
#include "cmdline.h"
#include "ConnectionOptions.h"
#include "ConnectionStats.h"
#include "Generator.h"
#include "Operation.h"
#include "util.h"
#include "vbucket.h"

using namespace std;

void bev_event_cb(struct bufferevent *bev, short events, void *ptr);
void bev_read_cb(struct bufferevent *bev, void *ptr);
void bev_write_cb(struct bufferevent *bev, void *ptr);
void timer_cb(evutil_socket_t fd, short what, void *ptr);

class Connection {
public:
  struct Host {
    string hostname;
    string port;
  };

  Connection(struct event_base* _base, struct evdns_base* _evdns,
            options_t options, vector<Connection::Host> hosts,
            VBUCKET_CONFIG_HANDLE vb, bool sampling = true);

  ~Connection();

  struct single_connection {
    single_connection(Host host, struct bufferevent *bev)
            : host(host), connected(false), bev(bev) {};
    Host host;
    bool connected;
    std::queue<Operation> op_queue;
    struct bufferevent *bev;
  };

  double start_time;  // Time when this connection began operations.

  enum read_state_enum {
    INIT_READ,
    LOADING,
    IDLE,
    WAITING_FOR_OP,
    MAX_READ_STATE,
  };

  enum write_state_enum {
    INIT_WRITE,
    ISSUING,
    WAITING_FOR_TIME,
    WAITING_FOR_OPQ,
    MAX_WRITE_STATE,
  };

  read_state_enum read_state;
  write_state_enum write_state;

  ConnectionStats stats;

  void issue_get(const char* key, double now = 0.0);
  void issue_set(const char* key, const char* value, int length,
                 double now = 0.0);
  void issue_something(double now = 0.0);
  bool check_exit_condition(double now = 0.0);
  void drive_write_machine(double now = 0.0);
  bool isIdle();

  void start_loading();

  void reset();
  void start_sasl();

  void event_callback(struct bufferevent *bev, short events);
  void read_callback(struct bufferevent *bev);
  void write_callback();
  void timer_callback();
  bool consume_binary_response(evbuffer *input);
  bool consume_ascii_response(evbuffer *input, Operation *op);

  void set_priority(int pri);

  options_t options;

private:
  struct event_base *base;
  struct evdns_base *evdns;
  vector<single_connection> conns;
  VBUCKET_CONFIG_HANDLE vb;

  struct event *timer;  // Used to control inter-transmission time.
  double lambda, next_time; // Inter-transmission time parameters.

  int data_length;  // When waiting for data, how much we're peeking for.

  // Parameters to track progress of the data loader.
  int loader_issued, loader_completed;

  Generator *valuesize;
  Generator *keysize;
  KeyGenerator *keygen;
  Generator *iagen;

  int op_queues_size();
  single_connection& find_conn(struct bufferevent *bev);
  void issue_sasl(struct bufferevent *bev);
};
