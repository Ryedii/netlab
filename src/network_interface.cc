#include <iostream>

#include "arp_message.hh"
#include "exception.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  const uint32_t target_ip = next_hop.ipv4_numeric();
  auto iter = mapping_table_.find( target_ip );
  if ( iter == mapping_table_.end() ) {
    dgrams_unreplied_.emplace( target_ip, dgram );
    if ( arp_request_timer.find( target_ip ) == arp_request_timer.end() ) {
      transmit( make_frame( EthernetHeader::TYPE_ARP,
                            serialize( make_arp_message( ARPMessage::OPCODE_REQUEST, target_ip ) ) ) );
      arp_request_timer.emplace( target_ip, 0 );
    }
  } else
    transmit( make_frame( EthernetHeader::TYPE_IPv4, serialize( dgram ), iter->second.ethernet_address ) );
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if ( frame.header.dst != ETHERNET_BROADCAST && frame.header.dst != ethernet_address_ )
    return;

  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram ip_dgram;
    if ( parse( ip_dgram, frame.payload ) )
      datagrams_received_.emplace( move( ip_dgram ) );
  }

  if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage arp_msg;
    if ( !parse( arp_msg, frame.payload ) )
      return;
    mapping_table_value_t mtv = {.ethernet_address=arp_msg.sender_ethernet_address, .timer=0};
    mapping_table_.insert_or_assign( arp_msg.sender_ip_address, mtv );

    if ( arp_msg.opcode == ARPMessage::OPCODE_REQUEST ) {
      if ( arp_msg.target_ip_address == ip_address_.ipv4_numeric() )
        transmit(
          make_frame( EthernetHeader::TYPE_ARP,
                      serialize( make_arp_message(
                        ARPMessage::OPCODE_REPLY, arp_msg.sender_ip_address, arp_msg.sender_ethernet_address ) ),
                      arp_msg.sender_ethernet_address ) );
    }

    if ( arp_msg.opcode == ARPMessage::OPCODE_REPLY ) {
      auto range = dgrams_unreplied_.equal_range( arp_msg.sender_ip_address );
      for ( auto iter = range.first; iter != range.second; ++iter )
        transmit(
          make_frame( EthernetHeader::TYPE_IPv4, serialize( iter->second ), arp_msg.sender_ethernet_address ) );
      dgrams_unreplied_.erase( arp_msg.sender_ip_address );
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  constexpr size_t ms_mappings_ttl = 30000, ms_resend_arp = 5000;
  for ( auto iter = mapping_table_.begin(); iter != mapping_table_.end(); ) {
    iter->second.timer += ms_since_last_tick;
    if ( iter->second.timer >= ms_mappings_ttl )
      iter = mapping_table_.erase( iter );
    else
      ++iter;
  }
  for ( auto iter = arp_request_timer.begin(); iter != arp_request_timer.end(); ) {
    iter->second += ms_since_last_tick;
    if ( iter->second >= ms_resend_arp )
      iter = arp_request_timer.erase( iter );
    else
      ++iter;
  }
}
