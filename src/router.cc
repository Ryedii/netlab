#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  uint32_t mask = ~( UINT32_MAX >> prefix_length );
  uint32_t subnet = route_prefix & mask;
  adr_prefix route = { .mask = mask, .subnet = subnet };
  route_table_.emplace( route, interface_num );
  hop_table_[interface_num] = next_hop;
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for ( auto& itfc : _interfaces ) {
    auto& dgrams = itfc->datagrams_received();
    while ( !dgrams.empty() ) {
      auto& dgram = dgrams.front();
      if ( dgram.header.ttl > 0 ) {
        --dgram.header.ttl;
        if ( dgram.header.ttl > 0 ) {
          dgram.header.compute_checksum();
          uint32_t hdr_dst = dgram.header.dst;
          auto iter = ranges::find_if( route_table_, [&hdr_dst]( auto& item ) -> bool {
            return ( hdr_dst & item.first.mask ) == item.first.subnet;
          } );
          uint32_t interface_num = iter->second;
          Address nxt_hop = hop_table_[interface_num].value_or( Address::from_ipv4_numeric( hdr_dst ) );
          interface( interface_num )->send_datagram( dgram, nxt_hop );
        }
      }
      dgrams.pop();
    }
  }

  // for ( auto& interface : _interfaces ) {
  //   auto& dgrams = interface->datagrams_received();
  //   for ( auto& dgram : dgrams ) {
  //     auto hdr_dst = dgram.header.dst;
  //     auto iter = ranges::find_if( router_table_, [&hdr_dst]( const auto& item ) -> bool {
  //       return ( hdr_dst & item.first.mask ) == item.first.netid;
  //     } );
  //     --dgram.header.ttl;
  //     dgram.header.compute_checksum();
  //     const auto& [interface_num, net_addr] = iter->second;
  //     interface( interface_num )
  //       .send_datagram( dgram, net_addr.has_value() ? *net_addr : Address::from_ipv4_numeric( hdr_dst ) );
  //   }
  // }
}
