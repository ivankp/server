#include "websockets.hh"

#include <tuple>

#include <netinet/in.h>
#include <openssl/sha.h>

#include "base64.hh"
#include "numconv.hh"
#include "debug.hh"

// TODO: Sec-WebSocket-Extensions: permessage-deflate

namespace ivanp {
namespace {

template <typename T>
[[gnu::always_inline]]
inline void buffread(char*& buff, T& x) {
  ::memcpy(&x,buff,sizeof(T));
  buff += sizeof(T);
}

template <bool only=true, typename... T>
requires ( std::is_same_v<T,char> && ... )
auto check_header(const http::request& req, const char* name, const T*... x) {
  const auto fields = req[name];
  if (!fields) HTTP_ERROR(400,"websocket handshake: missing ",name);
  if constexpr (sizeof...(x) == 0) {
    if (fields.size()>1) HTTP_ERROR(400,"multiple values for ",name);
    return fields.value();
  } else {
    if constexpr (only) {
      for (const auto [name,val] : fields)
        if (( !strcmp(val,x) || ... )) return;
    } else {
      if (( fields.contain(x) && ... )) return;
    }
    HTTP_ERROR(400,"websocket handshake: ",name," != ",x...);
  }
};

}

namespace websocket {

void handshake(socket sock, const http::request& req) {
  check_header<false>(req,"Connection","Upgrade");
  check_header(req,"Upgrade","websocket");
  check_header(req,"Sec-WebSocket-Version");
  const auto origin = check_header(req,"Origin");
  const auto protocol = check_header(req,"Sec-WebSocket-Protocol");
  const auto key = check_header(req,"Sec-WebSocket-Key");
  TEST(origin)
  TEST(protocol)

  auto key2 = cat(key,"258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
  char hash[SHA_DIGEST_LENGTH];
  SHA1(
    reinterpret_cast<const unsigned char*>(key2.data()),
    key2.size(),
    reinterpret_cast<unsigned char*>(&hash)
  );
  key2 = base64_encode(hash,SHA_DIGEST_LENGTH);
  // TEST(key2)
  sock << cat(
    "HTTP/1.1 101 Switching Protocols\r\n"
    "Connection: Upgrade\r\n"
    "Upgrade: websocket\r\n"
    "Sec-WebSocket-Accept: ",key2,"\r\n"
    "Sec-WebSocket-Protocol: ",protocol,"\r\n\r\n"
  );

  INFO("35;1","New websocket ",ntos((int)sock));
}

uint16_t frame::code() const noexcept {
  uint16_t code = 0;
  if (size() >= 2) {
    ::memcpy(&code,data(),2);
    code = ntohs(code);
  }
  return code;
}

frame receive_frame(socket sock, char* buffer, size_t size) {
  // TODO: find a way to handle most generic situations here
  size_t nread = 0;
  try {
    nread = sock.read(buffer,size);
  } catch (const std::exception& e) {
    ERROR(e.what(),", socket ",ntos((int)sock));
  }
  if (nread==0) ERROR1("empty ws frame");
  auto frame = parse_frame(buffer,size);

  switch (frame.opcode) {
    case head::text:
    case head::bin:
      break;
    case head::close:
      INFO("35;1","closing ws ",ntos((int)sock),", code: ",ntos(frame.code()));
      break;
    case head::ping:
      INFO("35;1","ping from ",ntos((int)sock));
      send_frame(sock,buffer,size,{},head::pong); // reply with pong
      break;
    case head::pong:
      INFO("35;1","pong from ",ntos((int)sock));
      send_frame(sock,buffer,size,{},head::ping); // reply with ping
      break;
  }

  return frame;
}

frame parse_frame(char* buff, size_t bufflen) {
  //  0                   1                   2                   3
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +-+-+-+-+-------+-+-------------+-------------------------------+
  // |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
  // |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
  // |N|V|V|V|       |S|             |   (if payload len==126/127)   |
  // | |1|2|3|       |K|             |                               |
  // +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
  // |    Extended payload length continued, if payload len == 127   |
  // + - - - - - - - - - - - - - - - +-------------------------------+
  // |                               | Masking-key, if MASK set to 1 |
  // +-------------------------------+-------------------------------+
  // |   Masking-key (continued)     |          Payload Data         |
  // +-------------------------------- - - - - - - - - - - - - - - - +
  // :                     Payload Data continued ...                :
  // + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
  // |                     Payload Data continued ...                |
  // +---------------------------------------------------------------+

  head head;
  static_assert(sizeof(head)==2);
  uint64_t len;

  buffread(buff,head);
  TEST(unsigned(head.len))

  if (head.rsv != 0) ERROR1("rsv!=0 not implemented");

  if (head.len<126) {
    len = head.len;
  } else if (head.len<127) {
    union {
      uint16_t tmp;
      uint8_t bytes[2];
    };
    buffread(buff,tmp);
    std::swap(bytes[0],bytes[1]);
    len = tmp;
  } else {
    union {
      uint64_t tmp;
      uint8_t bytes[8];
    };
    buffread(buff,tmp);
    for (int i=4; i; ) --i, std::swap(bytes[i],bytes[7-i]);
    len = tmp;
  }
  TEST(len)

  if (len > bufflen) ERROR1("frame length exceeds buffer length");

  if (len) {
    if (head.mask) {
      uint8_t mask[4];
      buffread(buff,mask);

      int nset = 0;
      for (int i=0; i<4; ++i) {
        if (mask[i]==uint8_t(0xff)) { // all bits set
          ++nset;
        } else if (mask[i]) { // some bits are set
          nset = -1;
          break;
        }
      }
      if (nset==0 || nset==4)
        ERROR((nset ? "all" : "no")," mask bits are set");

      for (uint64_t i=0; i<len; ++i)
        buff[i] ^= mask[i%4];
    } else ERROR1("client sent no mask");
  }

  return { head, { buff, len } };
}

void send_frame(
  socket sock, char* buffer, size_t size,
  std::string_view message, head::type opcode
) {
  //  0                   1                   2                   3
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +-+-+-+-+-------+-+-------------+-------------------------------+
  // |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
  // |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
  // |N|V|V|V|       |S|             |   (if payload len==126/127)   |
  // | |1|2|3|       |K|             |                               |
  // +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
  // |    Extended payload length continued, if payload len == 127   |
  // + - - - - - - - - - - - - - - - +-------------------------------+
  // |                               |          Payload Data         |
  // +-------------------------------- - - - - - - - - - - - - - - - +
  // :                     Payload Data continued ...                :
  // + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
  // |                     Payload Data continued ...                |
  // +---------------------------------------------------------------+

  if (message.size()+10 > size) ERROR(
    "The message is too long for buffer of size ", ntos(size));
  ::memmove(buffer += 10, message.data(), size = message.size());

  head head { .opcode = opcode, .fin = 1 };
  if (size<126) {
    head.len = size;
  } else if (size <= (uint16_t)-1) {
    head.len = 126;
    uint16_t size2 = size;
    ::memcpy(buffer-=2,&size2,2);
    size += 2;
  } else {
    head.len = 127;
    uint64_t size2 = size;
    ::memcpy(buffer-=8,&size2,8);
    size += 8;
  }
  ::memcpy(buffer-=2,&head,2);
  sock.write(buffer,size+2);
}

} // end namespace websocket
} // end namespace ivanp
