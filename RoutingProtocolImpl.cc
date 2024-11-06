#include "RoutingProtocolImpl.h"
#include <cstring>
#include <arpa/inet.h>

RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n)
{
  sys = n;
  // add your own code
}

RoutingProtocolImpl::~RoutingProtocolImpl()
{
  // add your own code (if needed)
}

void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type)
{
  // add your own code
  this->num_ports = num_ports;
  this->router_id = router_id;
  this->protocol_type = protocol_type;

  // set ping alarm
  int *ping_data = new int(0);
  sys->set_alarm(this, 0, ping_data); // every 10 s handle_alarm

  // set neighbor expiration check alarm
  int *expiration_data = new int(1);
  sys->set_alarm(this, 1000, expiration_data); // every 1 s check neighbor expiration
}

void RoutingProtocolImpl::handle_alarm(void *data)
{
  // add your own code
  int *alarm_type = static_cast<int *>(data);

  if (*alarm_type == 0)
  { // if PING alarm
    // send PING here
    // cout << "send PING here" << endl;

    for (unsigned short port = 0; port < num_ports; ++port)
    {
      cout << "send PING for port: " << port << endl;
      // create PING
      uint32_t current_time = sys->time(); // get current timestamp

      size_t packet_size = sizeof(uint8_t) * 2 + sizeof(uint16_t) * 3 + sizeof(uint32_t);
      char *buffer = new char[packet_size];
      memset(buffer, 0, packet_size);

      uint8_t packet_type = PING;
      uint8_t reserved = 0;
      uint16_t size = htons(packet_size);
      uint16_t source_id = htons(router_id);
      uint16_t dest_id = htons(0);
      uint32_t timestamp = htonl(current_time);

      // memcpy
      size_t offset = 0;
      memcpy(buffer + offset, &packet_type, sizeof(packet_type));
      offset += sizeof(packet_type);
      memcpy(buffer + offset, &reserved, sizeof(reserved));
      offset += sizeof(reserved);
      memcpy(buffer + offset, &size, sizeof(size));
      offset += sizeof(size);
      memcpy(buffer + offset, &source_id, sizeof(source_id));
      offset += sizeof(source_id);
      memcpy(buffer + offset, &dest_id, sizeof(dest_id));
      offset += sizeof(dest_id);
      memcpy(buffer + offset, &timestamp, sizeof(timestamp));

      // send
      sys->send(port, buffer, packet_size);
    }

    // set alarm again
    sys->set_alarm(this, 10000, new int(0));
  }
  else if (*alarm_type == 1)
  { // check neighbor expiration
    // cout << "check neighbor expiration here" << endl;
    unsigned int current_time = sys->time();

    for (auto it = neighbors.begin(); it != neighbors.end();)
    {
      NeighborInfo &neighbor = it->second;
      if (current_time - neighbor.last_response_time > 15000)
      {                           // no response more than 15 seconds
        // cout << "Neighbor on port " << it->first << " with ID " << neighbor.neighbor_id << " has timed out and will be removed." << endl;
        it = neighbors.erase(it); // remove expired neighbor
      }
      else
      {
        ++it;
      }
    }

    // print neighbors
    // cout << "Current Neighbors:" << endl;
    // for (const auto &entry : neighbors)
    // {
    //   cout << "Port: " << entry.first
    //        << ", Neighbor ID: " << entry.second.neighbor_id
    //        << ", Last Response Time: " << entry.second.last_response_time
    //        << ", RTT: " << entry.second.rtt << " ms" << endl;
    // }

    // set alarm again
    sys->set_alarm(this, 1000, new int(1));
  }

  // clear data
  delete alarm_type;
}

void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size)
{
  // add your own code
  char *buffer = static_cast<char *>(packet);

  // Extract packet type from the buffer
  uint8_t packet_type;
  memcpy(&packet_type, buffer, sizeof(packet_type));

  // cout << "Received packet of type: " << static_cast<int>(packet_type) << " on port " << port << endl;

  if (packet_type == PING)
  {
    // Handle PING packet by sending a PONG response
    // cout << "receive PING here" << endl;
    // Extract timestamp from the PING packet
    uint32_t timestamp;
    memcpy(&timestamp, buffer + 8, sizeof(timestamp)); // Offset 8 due to packet header

    uint16_t ping_source_id;
    memcpy(&ping_source_id, buffer + 4, sizeof(ping_source_id)); // get PING source_id
    ping_source_id = ntohs(ping_source_id);

    // Prepare the PONG packet buffer
    size_t pong_packet_size = sizeof(uint8_t) * 2 + sizeof(uint16_t) * 3 + sizeof(uint32_t);
    char *pong_buffer = new char[pong_packet_size];
    memset(pong_buffer, 0, pong_packet_size);

    // Fill in the PONG packet fields
    uint8_t pong_packet_type = PONG;
    uint8_t reserved = 0;
    uint16_t pong_size = htons(pong_packet_size);
    uint16_t source_id = htons(router_id);
    uint16_t dest_id = htons(ping_source_id);
    uint32_t pong_timestamp = timestamp; // Send back the original timestamp

    // Copy data into pong_buffer
    size_t offset = 0;
    memcpy(pong_buffer + offset, &pong_packet_type, sizeof(pong_packet_type));
    offset += sizeof(pong_packet_type);
    memcpy(pong_buffer + offset, &reserved, sizeof(reserved));
    offset += sizeof(reserved);
    memcpy(pong_buffer + offset, &pong_size, sizeof(pong_size));
    offset += sizeof(pong_size);
    memcpy(pong_buffer + offset, &source_id, sizeof(source_id));
    offset += sizeof(source_id);
    memcpy(pong_buffer + offset, &dest_id, sizeof(dest_id));
    offset += sizeof(dest_id);
    memcpy(pong_buffer + offset, &pong_timestamp, sizeof(pong_timestamp));

    // Send PONG packet back on the same port
    sys->send(port, pong_buffer, pong_packet_size);
    // cout << "send PONG here" << endl;
  }
  else if (packet_type == PONG)
  {
    // Handle PONG packet to update neighbor status

    // Extract the timestamp from the PONG packet
    uint32_t timestamp;
    memcpy(&timestamp, buffer + 8, sizeof(timestamp)); // Offset 8 for packet header
    timestamp = ntohl(timestamp);                      // Convert timestamp to host byte order

    uint16_t pong_source_id;
    memcpy(&pong_source_id, buffer + 4, sizeof(pong_source_id)); // get PONG source_id
    pong_source_id = ntohs(pong_source_id);

    // Calculate RTT using the current time and the received timestamp
    uint32_t current_time = sys->time();
    uint16_t rtt = current_time - timestamp;

    // Update neighbor information in the neighbors map
    if (neighbors.find(port) == neighbors.end())
    {
      // Initialize new neighbor entry if it doesn't exist
      neighbors[port] = {pong_source_id, port, current_time, rtt};
    }
    else
    {
      // Update existing neighbor information
      neighbors[port].last_response_time = current_time;
      neighbors[port].rtt = rtt;
    }
  }

  // Clean up received packet buffer
  delete[] buffer;
}

// add more of your own code
