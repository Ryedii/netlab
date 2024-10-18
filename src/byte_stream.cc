#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  return is_closed_;
}

void Writer::push( string data )
{
  if ( has_error() || is_closed() )
    return;
  uint64_t len = std::min( data.size(), available_capacity() );
  if ( len == 0 )
    return;
  buf_.push( data.substr( 0, len ) );
  bytes_pushed_size_ += len;
}

void Writer::close()
{
  is_closed_ = true;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - ( bytes_pushed_size_ - bytes_popped_size_ );
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_size_;
}

bool Reader::is_finished() const
{
  return is_closed_ && bytes_buffered() == 0;
}

uint64_t Reader::bytes_popped() const
{
  return bytes_popped_size_;
}

string_view Reader::peek() const
{
  return buf_.empty() ? std::string_view() : std::string_view { buf_.front() }.substr( popped_prefix_size_ );
}

void Reader::pop( uint64_t len )
{
  if ( has_error() )
    return;
  len = std::min( len, bytes_buffered() );
  bytes_popped_size_ += len;
  while ( len ) {
    if ( popped_prefix_size_ + len < buf_.front().size() ) {
      popped_prefix_size_ += len;
      len = 0;
    } else {
      len -= buf_.front().size() - popped_prefix_size_;
      popped_prefix_size_ = 0;
      buf_.pop();
    }
  }
}

uint64_t Reader::bytes_buffered() const
{
  return bytes_pushed_size_ - bytes_popped_size_;
}
