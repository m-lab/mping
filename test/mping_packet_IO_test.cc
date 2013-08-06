#include "mp_mping.h"

#include "gtest/gtest.h"
#include "mlab/mlab.h"
#include "mp_socket.h"
#include "mp_stats.h"

TEST(MpingPacketIO, DynWindow) {
  const char *myargv[] = {"mping",
                          "-n", 
                          "5", 
                          "-b", 
                          "64", 
                          "127.0.0.1", NULL}; 

  MPing mp(6, myargv);
  EXPECT_FALSE(mp.IsServerMode());

  mp.Run(); 

  // loop on window size from 1 to 15, every window has 20 packet to send
  // 1, -1, ..., 20, -20, 21, 22, -21, ..., 40, -39, 41, 42, -40 
  //                       ^
  //                       | First packet of a new window:
  //                         index: 20 * 2 * (cur_win - 1) - (cur_win - 1 - 1)
  //                         seq: 20 * (cur_win - 1) + 1
  ASSERT_GT(mp.mp_stat.timeline.size(), 0u);
  EXPECT_EQ(mp.mp_stat.timeline.at(0), 1);
  EXPECT_EQ(mp.mp_stat.timeline.at(1), -1);
  EXPECT_EQ(mp.mp_stat.timeline.at(40), 21);
  EXPECT_EQ(mp.mp_stat.timeline.at(41), 22);
  EXPECT_EQ(mp.mp_stat.timeline.at(42), -21);
  EXPECT_EQ(mp.mp_stat.timeline.at(80), 42);
  EXPECT_EQ(mp.mp_stat.timeline.at(81), -40);
  EXPECT_EQ(mp.mp_stat.timeline.at(119), 62);
  EXPECT_EQ(mp.mp_stat.timeline.at(120), -59);
}

TEST(MpingPacketIO, DynWindowBurst) {
  const char *myargv[] = {"mping",
                          "-n",
                          "5",
                          "-b",
                          "64",
                          "127.0.0.1",
                          "-B", "3", NULL};

  MPing mp(8, myargv);
  mp.Run();

  // ..., 41, 42, -40, -41, -42, 43, 44, 45, ..., 58, 59, 60, -58, -59, 61, 
  // 62, 63, -60, -61, -62, ...
  // Burst starts when window size >= burst size
  EXPECT_EQ(mp.mp_stat.timeline.at(0), 1);                                         
  EXPECT_EQ(mp.mp_stat.timeline.at(1), -1);                                        
  EXPECT_EQ(mp.mp_stat.timeline.at(40), 21);                                       
  EXPECT_EQ(mp.mp_stat.timeline.at(41), 22);                                       
  EXPECT_EQ(mp.mp_stat.timeline.at(42), -21);                                      
  EXPECT_EQ(mp.mp_stat.timeline.at(80), 42);  
  EXPECT_EQ(mp.mp_stat.timeline.at(81), -40);
  EXPECT_EQ(mp.mp_stat.timeline.at(82), -41);
  EXPECT_EQ(mp.mp_stat.timeline.at(83), -42);
  EXPECT_EQ(mp.mp_stat.timeline.at(84), 43);
  EXPECT_EQ(mp.mp_stat.timeline.at(85), 44);
  EXPECT_EQ(mp.mp_stat.timeline.at(86), 45);
  EXPECT_EQ(mp.mp_stat.timeline.at(116), 60);
  EXPECT_EQ(mp.mp_stat.timeline.at(117), -58);
  EXPECT_EQ(mp.mp_stat.timeline.at(118), -59);
  EXPECT_EQ(mp.mp_stat.timeline.at(119), 61);
}
