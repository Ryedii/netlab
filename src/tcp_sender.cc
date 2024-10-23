#include "tcp_sender.hh"
#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return outstanding_bytes_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  uint64_t real_window_size_ = window_size_ > 0 ? window_size_ : 1;
  while ( outstanding_bytes_ < real_window_size_ && !is_sent_FIN_ ) {
    TCPSenderMessage msg;
    if ( !is_sent_SYN_ ) {
      msg.SYN = true;
      is_sent_SYN_ = true;
      msg.seqno = isn_;
    } else
      msg.seqno = Wrap32::wrap( abs_seqno_, isn_ );
    if ( reader().has_error() ) {
      msg.RST = true;
      transmit( msg );
      return;
    }

    uint64_t payload_size = std::min( TCPConfig::MAX_PAYLOAD_SIZE, reader().bytes_buffered() );
    payload_size = std::min( payload_size, real_window_size_ - outstanding_bytes_ - msg.SYN );
    read( input_.reader(), payload_size, msg.payload );

    if ( reader().is_finished() && payload_size + msg.SYN < real_window_size_ - outstanding_bytes_ ) {
      msg.FIN = true;
      is_sent_FIN_ = true;
    }

    if ( msg.sequence_length() == 0 && msg.SYN == false && msg.FIN == false )
      return;
    transmit( msg );
    is_start_timer_ = true;
    outstanding_messages_.push_back( msg );
    outstanding_bytes_ += msg.sequence_length();
    abs_seqno_ += msg.sequence_length();
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return TCPSenderMessage { .seqno = Wrap32::wrap( abs_seqno_, isn_ ),
                            .SYN = false,
                            .payload = "",
                            .FIN = false,
                            .RST = reader().has_error() };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  window_size_ = msg.window_size;
  if ( msg.ackno.has_value() ) {
    uint64_t abs_ackno = msg.ackno.value().unwrap( isn_, abs_seqno_ );
    if ( abs_ackno > abs_seqno_ )
      return;
    while ( outstanding_bytes_ > 0
            && outstanding_messages_.front().seqno.unwrap( isn_, abs_seqno_ )
                   + outstanding_messages_.front().sequence_length()
                 <= abs_ackno ) {
      outstanding_bytes_ -= outstanding_messages_.front().sequence_length();
      outstanding_messages_.pop_front();
      consecutive_retransmissions_ = 0;
      remaining_RTO_ms_ = initial_RTO_ms_;
    }
    if ( outstanding_bytes_ == 0 )
      is_start_timer_ = false;
    else
      is_start_timer_ = true;
  } else {
    if ( window_size_ == 0 )
      input_.set_error();
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if ( is_start_timer_ ) {
    if ( remaining_RTO_ms_ <= ms_since_last_tick ) {
      transmit( outstanding_messages_.front() );
      ++consecutive_retransmissions_;
      remaining_RTO_ms_ = initial_RTO_ms_ << ( window_size_ > 0 ? consecutive_retransmissions_ : 0 );
    } else
      remaining_RTO_ms_ -= ms_since_last_tick;
  }
}
