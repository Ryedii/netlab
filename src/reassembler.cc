#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  if ( is_last_substring )
    eof_index_ = first_index + data.size();

  if ( first_index >= first_unacceptable_index() )
    return;
  uint64_t last_index = data.size() + first_index;
  if ( last_index > first_unacceptable_index() ) {
    data.resize( first_unacceptable_index() - first_index );
    last_index = first_unacceptable_index();
  }

  if ( first_index >= first_unassembled_index_ ) {
    uint64_t offset_buf_ = first_index - first_unassembled_index_;
    buf_.resize( std::max( buf_.size(), data.size() + offset_buf_ ) );
    for ( uint64_t i = 0; i < data.size(); ++i ) {
      if ( !buf_[i + offset_buf_].second ) {
        buf_[i + offset_buf_].first = data[i];
        buf_[i + offset_buf_].second = true;
        ++bytes_pending_;
      }
    }
  } else if ( last_index >= first_unassembled_index_ ) {
    uint64_t offset_data = first_unassembled_index_ - first_index;
    uint64_t len = last_index - first_unassembled_index_;
    buf_.resize( std::max( buf_.size(), len ) );
    for ( uint64_t i = 0; i < len; ++i ) {
      if ( !buf_[i].second ) {
        buf_[i].first = data[i + offset_data];
        buf_[i].second = true;
        ++bytes_pending_;
      }
    }
  }

  std::string s;
  while ( !buf_.empty() && buf_.front().second ) {
    s.push_back( buf_.front().first );
    buf_.pop_front();
  }
  if ( !s.empty() ) {
    output_.writer().push( s );
    first_unassembled_index_ += s.size();
    bytes_pending_ -= s.size();
  }

  if ( bytes_pending_ == 0 && first_unassembled_index_ == eof_index_ )
    output_.writer().close();
}

uint64_t Reassembler::bytes_pending() const
{
  return bytes_pending_;
}

uint64_t Reassembler::first_unacceptable_index() const
{
  return output_.writer().bytes_pushed() + output_.writer().available_capacity();
}
