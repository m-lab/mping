#ifndef _MP_SOCKET_H_
#define _MP_SOCKET_H_

#include "mlab/client_socket.h"
#include "mlab/socket_family.h"
#include "mlab/raw_socket.h"
#include "log.h"

class MpingSocket {
  public:
    MpingSocket(const std::string destip, const std::string srcip,
                    int ttl, size_t pktsize, int wndsize, uint16_t port);
    MpingSocket(const MpingSocket& other); 
    MpingSocket& operator = (const MpingSocket&);
    ~MpingSocket();

    bool SetSendTTL(const int& ttl);
    size_t SendPacket(const unsigned int& seq, size_t size, int *error) const;
    unsigned int ReceiveAndGetSeq(size_t size, int* error);
    const std::string GetFromAddress() const;

  protected:
    mlab::RawSocket *icmp_sock;
    mlab::ClientSocket *udp_sock;

  private:
    SocketFamily family_;
    sockaddr_storage srcaddr_;
    char buffer_[64];
    int buffer_length_;
    bool use_udp_;
    std::string fromaddr_;
};

#endif
