/*
 * receiver.h: The basic receiver interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: receiver.h 1.2 2002/07/28 11:22:01 kls Exp $
 */

#ifndef __RECEIVER_H
#define __RECEIVER_H

#include "device.h"

#define MAXRECEIVEPIDS  16 // the maximum number of PIDs per receiver

class cReceiver {
  friend class cDevice;
private:
  cDevice *device;
  int ca;
  int priority;
  int pids[MAXRECEIVEPIDS];
  bool WantsPid(int Pid);
protected:
  void Detach(void);
  virtual void Activate(bool On) {}
               // This function is called just before the cReceiver gets attached to
               // (On == true) or detached from (On == false) a cDevice. It can be used
               // to do things like starting/stopping a thread.
               // It is guaranteed that Receive() will not be called before Activate(true).
  virtual void Receive(uchar *Data, int Length) = 0;
               // This function is called from the cDevice we are attached to, and
               // delivers one TS packet from the set of PIDs the cReceiver has requested.
               // The data packet must be accepted immediately, and the call must return
               // as soon as possible, without any unnecessary delay. Each TS packet
               // will be delivered only ONCE, so the cReceiver must make sure that
               // it will be able to buffer the data if necessary.
public:
  cReceiver(int Ca, int Priority, int NumPids, ...);
               // Creates a new receiver that requires conditional access Ca and has
               // the given Priority. NumPids defines the number of PIDs that follow
               // this parameter. If any of these PIDs are 0, they will be silently ignored.
               // The total number of non-zero PIDs must not exceed MAXRECEIVEPIDS.
               // Priority may be any value in the range 0..99. Negative values indicate
               // that this cReceiver may be detached at any time (without blocking the
               // cDevice it is attached to).
  virtual ~cReceiver();
  };

#endif //__RECEIVER_H
