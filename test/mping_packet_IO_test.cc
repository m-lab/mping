#include "mp_client.h"

#include "gtest/gtest.h"
#include "mlab/mlab.h"
#include "mp_socket.h"
#include "mp_stats.h"

TEST(MpingPacketIO, DynWindow) {
  MPingClient mp(64,     // packet size -b
                 5,      // window size -n
                 false,  // loop forever, -f
                 false,  // slowstart, -S
                 0,      // ttl, -t
                 0,      // ttlmax, -a
                 0,      // loop through size, -l
                 0,      // burst size, -B
                 0,      // destination port, -p
                 0,      // use UDP server, -c
                 false,  // print time sequence, -r
                 std::string(""),  // source address, -F
                 std::string("127.0.0.1"));  // destination host

  mp.Run(); 

  // loop on window size from 1 to 15, every window has 20 packet to send
  // 1, -1, ..., 20, -20, 21, 22, -21, ..., 40, -39, 41, 42, -40 
  //                       ^
  //                       | First packet of a new window:
  //                         index: 20 * 2 * (cur_win - 1) - (cur_win - 1 - 1)
  //                         seq: 20 * (cur_win - 1) + 1
  ASSERT_GT(mp.mp_stat_.timeline_.size(), 0u);
  EXPECT_EQ(mp.mp_stat_.timeline_.at(0), 1);
  EXPECT_EQ(mp.mp_stat_.timeline_.at(1), -1);
  EXPECT_EQ(mp.mp_stat_.timeline_.at(40), 21);
  EXPECT_EQ(mp.mp_stat_.timeline_.at(41), 22);
  EXPECT_EQ(mp.mp_stat_.timeline_.at(42), -21);
  EXPECT_EQ(mp.mp_stat_.timeline_.at(80), 42);
  EXPECT_EQ(mp.mp_stat_.timeline_.at(81), -40);
  EXPECT_EQ(mp.mp_stat_.timeline_.at(119), 62);
  EXPECT_EQ(mp.mp_stat_.timeline_.at(120), -59);
}

TEST(MpingPacketIO, DynWindowBurst) {
  MPingClient mp(64,     // packet size -b
                 5,      // window size -n
                 false,  // loop forever, -f
                 false,  // slowstart, -S
                 0,      // ttl, -t
                 0,      // ttlmax, -a
                 0,      // loop through size, -l
                 3,      // burst size, -B
                 0,      // destination port, -p
                 0,      // use UDP server, -c
                 false,  // print time sequence, -r
                 std::string(""),  // source address, -Fj
                 std::string("127.0.0.1"));  // destination host
  mp.Run();

  // ..., 41, 42, -40, -41, -42, 43, 44, 45, ..., 58, 59, 60, -58, -59, 61, 
  // 62, 63, -60, -61, -62, ...
  // Burst starts when window size >= burst size
  EXPECT_EQ(mp.mp_stat_.timeline_.at(0), 1);                                         
  EXPECT_EQ(mp.mp_stat_.timeline_.at(1), -1);                                        
  EXPECT_EQ(mp.mp_stat_.timeline_.at(40), 21);                                       
  EXPECT_EQ(mp.mp_stat_.timeline_.at(41), 22);                                       
  EXPECT_EQ(mp.mp_stat_.timeline_.at(42), -21);                                      
  EXPECT_EQ(mp.mp_stat_.timeline_.at(80), 42);  
  EXPECT_EQ(mp.mp_stat_.timeline_.at(81), -40);
  EXPECT_EQ(mp.mp_stat_.timeline_.at(82), -41);
  EXPECT_EQ(mp.mp_stat_.timeline_.at(83), -42);
  EXPECT_EQ(mp.mp_stat_.timeline_.at(84), 43);
  EXPECT_EQ(mp.mp_stat_.timeline_.at(85), 44);
  EXPECT_EQ(mp.mp_stat_.timeline_.at(86), 45);
  EXPECT_EQ(mp.mp_stat_.timeline_.at(116), 60);
  EXPECT_EQ(mp.mp_stat_.timeline_.at(117), -58);
  EXPECT_EQ(mp.mp_stat_.timeline_.at(118), -59);
  EXPECT_EQ(mp.mp_stat_.timeline_.at(119), 61);
}
