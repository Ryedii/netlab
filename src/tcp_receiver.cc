#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    RST_ = true;
    reader().set_error();
  }
  if ( RST_ )
    return;

  if ( message.SYN && !SYN_ ) {
    SYN_ = true;
    zero_point = message.seqno;
    message.seqno = message.seqno + 1;
  }
  if ( !SYN_ )
    return;

  uint64_t stream_index = message.seqno.unwrap( zero_point, writer().bytes_pushed() ) - 1;
  reassembler_.insert( stream_index, message.payload, message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage message;
  if ( SYN_ )
    message.ackno = Wrap32::wrap( writer().bytes_pushed() + 1 + writer().is_closed(), zero_point );
  message.RST = reader().has_error();
  message.window_size = std::min( writer().available_capacity(), (uint64_t)UINT16_MAX );
  return message;
}
