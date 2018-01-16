/*
 * transfer.h: Transfer mode
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: transfer.h 1.12 2007/01/05 10:45:45 kls Exp $
 */

#ifndef __TRANSFER_H
#define __TRANSFER_H

#include "player.h"
#include "receiver.h"
#include "remux.h"
#include "ringbuffer.h"
#include "thread.h"

class cTransfer : public cReceiver, public cPlayer, public cThread {
private:
  cRingBufferLinear *ringBuffer;
  cRemux *remux;
protected:
  virtual void Activate(bool On);
  virtual void Receive(uchar *Data, int Length);
  virtual void Action(void);
public:
  cTransfer(tChannelID ChannelID, int VPid, const int *APids, const int *DPids, const int *SPids);
  virtual ~cTransfer();
  };

class cTransferControl : public cControl {
private:
  cTransfer *transfer;
  static cDevice *receiverDevice;
public:
  cTransferControl(cDevice *ReceiverDevice, tChannelID ChannelID, int VPid, const int *APids, const int *DPids, const int *SPids);
  ~cTransferControl();
  virtual void Hide(void) {}
  static cDevice *ReceiverDevice(void) { return receiverDevice; }
  };

#endif //__TRANSFER_H
