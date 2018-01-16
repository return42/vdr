/*
 * rcu.c: RCU remote control
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: rcu.c 1.5 2003/05/02 14:42:40 kls Exp $
 */

#include "rcu.h"
#include <netinet/in.h>
#include <termios.h>
#include <unistd.h>
#include "tools.h"

#define REPEATLIMIT  20 // ms
#define REPEATDELAY 350 // ms

cRcuRemote::cRcuRemote(char *DeviceName)
:cRemote("RCU")
{
  dp = 0;
  mode = modeB;
  code = 0;
  numberToSend = -1;
  lastNumber = 0;
  receivedCommand = false;
  if ((f = open(DeviceName, O_RDWR | O_NONBLOCK)) >= 0) {
     struct termios t;
     if (tcgetattr(f, &t) == 0) {
        cfsetspeed(&t, B9600);
        cfmakeraw(&t);
        if (tcsetattr(f, TCSAFLUSH, &t) == 0) {
           Number(0);//XXX 8888???
           const char *Setup = GetSetup();
           if (Setup) {
              code = *Setup;
              SetCode(code);
              isyslog("connecting to %s remote control using code %c", Name(), code);
              }
           Start();
           return;
           }
        }
     LOG_ERROR_STR(DeviceName);
     close(f);
     }
  else
     LOG_ERROR_STR(DeviceName);
  f = -1;
}

cRcuRemote::~cRcuRemote()
{
  Cancel();
}

bool cRcuRemote::Ready(void)
{
  return f >= 0;
}

bool cRcuRemote::Initialize(void)
{
  if (f >= 0) {
     unsigned char Code = '0';
     isyslog("trying codes for %s remote control...", Name());
     for (;;) {
         if (DetectCode(&Code)) {
            code = Code;
            break;
            }
         }
     isyslog("established connection to %s remote control using code %c", Name(), code);
     char buffer[16];
     snprintf(buffer, sizeof(buffer), "%c", code);
     PutSetup(buffer);
     return true;
     }
  return false;
}

void cRcuRemote::Action(void)
{
#pragma pack(1)
  union {
    struct {
      unsigned short address;
      unsigned int command;
      } data;
    unsigned char raw[6];
    } buffer;
#pragma pack()

  dsyslog("RCU remote control thread started (pid=%d)", getpid());

  time_t LastCodeRefresh = 0;
  int FirstTime = 0;
  uint64 LastCommand = 0;
  bool repeat = false;

  //XXX
  for (; f >= 0;) {

      LOCK_THREAD;

      if (ReceiveByte(REPEATLIMIT) == 'X') {
         for (int i = 0; i < 6; i++) {
             int b = ReceiveByte();
             if (b >= 0) {
                buffer.raw[i] = b;
                if (i == 5) {
                   unsigned short Address = ntohs(buffer.data.address); // the PIC sends bytes in "network order"
                   uint64         Command = ntohl(buffer.data.command);
                   if (code == 'B' && Address == 0x0000 && Command == 0x00004000)
                      // Well, well, if it isn't the "d-box"...
                      // This remote control sends the above command before and after
                      // each keypress - let's just drop this:
                      break;
                   int Now = time_ms();
                   Command |= uint64(Address) << 32;
                   if (Command != LastCommand) {
                      LastCommand = Command;
                      repeat = false;
                      FirstTime = Now;
                      }
                   else {
                      if (Now - FirstTime < REPEATDELAY)
                         break; // repeat function kicks in after a short delay
                      repeat = true;
                      }
                   Put(Command, repeat);
                   receivedCommand = true;
                   }
                }
             else
                break;
             }
         }
      else if (repeat) { // the last one was a repeat, so let's generate a release
         Put(LastCommand, false, true);
         repeat = false;
         LastCommand = 0;
         }
      else {
         LastCommand = 0;
         if (numberToSend >= 0) {
            Number(numberToSend);
            numberToSend = -1;
            }
         }
      if (code && time(NULL) - LastCodeRefresh > 60) {
         SendCommand(code); // in case the PIC listens to the wrong code
         LastCodeRefresh = time(NULL);
         }
      }
}

int cRcuRemote::ReceiveByte(int TimeoutMs)
{
  // Returns the byte if one was received within a timeout, -1 otherwise
  if (cFile::FileReady(f, TimeoutMs)) {
     unsigned char b;
     if (safe_read(f, &b, 1) == 1)
        return b;
     else
        LOG_ERROR;
     }
  return -1;
}

bool cRcuRemote::SendByteHandshake(unsigned char c)
{
  if (f >= 0) {
     int w = write(f, &c, 1);
     if (w == 1) {
        for (int reply = ReceiveByte(REPEATLIMIT); reply >= 0;) {
            if (reply == c)
               return true;
            else if (reply == 'X') {
               // skip any incoming RC code - it will come again
               for (int i = 6; i--;) {
                   if (ReceiveByte() < 0)
                      return false;
                   }
               }
            else
               return false;
            }
        }
     LOG_ERROR;
     }
  return false;
}

bool cRcuRemote::SendByte(unsigned char c)
{
  LOCK_THREAD;

  for (int retry = 5; retry--;) {
      if (SendByteHandshake(c))
         return true;
      }
  return false;
}

bool cRcuRemote::SetCode(unsigned char Code)
{
  code = Code;
  return SendCommand(code);
}

bool cRcuRemote::SetMode(unsigned char Mode)
{
  mode = Mode;
  return SendCommand(mode);
}

bool cRcuRemote::SendCommand(unsigned char Cmd)
{ 
  return SendByte(Cmd | 0x80);
}

bool cRcuRemote::Digit(int n, int v)
{ 
  return SendByte(((n & 0x03) << 5) | (v & 0x0F) | (((dp >> n) & 0x01) << 4));
}

bool cRcuRemote::Number(int n, bool Hex)
{
  LOCK_THREAD;

  if (!Hex) {
     char buf[8];
     sprintf(buf, "%4d", n & 0xFFFF);
     n = 0;
     for (char *d = buf; *d; d++) {
         if (*d == ' ')
            *d = 0xF;
         n = (n << 4) | ((*d - '0') & 0x0F);
         }
     }
  lastNumber = n;
  for (int i = 0; i < 4; i++) {
      if (!Digit(i, n))
         return false;
      n >>= 4;
      }
  return SendCommand(mode);
}

bool cRcuRemote::String(char *s)
{
  LOCK_THREAD;

  const char *chars = mode == modeH ? "0123456789ABCDEF" : "0123456789-EHLP ";
  int n = 0;

  for (int i = 0; *s && i < 4; s++, i++) {
      n <<= 4;
      for (const char *c = chars; *c; c++) {
          if (*c == *s) {
             n |= c - chars;
             break;
             }
          }
      }
  return Number(n, true);
}

void cRcuRemote::SetPoints(unsigned char Dp, bool On)
{ 
  if (On)
     dp |= Dp;
  else
     dp &= ~Dp;
  Number(lastNumber, true);
}

bool cRcuRemote::DetectCode(unsigned char *Code)
{
  // Caller should initialize 'Code' to 0 and call DetectCode()
  // until it returns true. Whenever DetectCode() returns false
  // and 'Code' is not 0, the caller can use 'Code' to display
  // a message like "Trying code '%c'". If false is returned and
  // 'Code' is 0, all possible codes have been tried and the caller
  // can either stop calling DetectCode() (and give some error
  // message), or start all over again.
  if (*Code < 'A' || *Code > 'D') {
     *Code = 'A';
     return false;
     }
  if (*Code <= 'D') {
     SetMode(modeH);
     char buf[5];
     sprintf(buf, "C0D%c", *Code);
     String(buf);
     SetCode(*Code);
     delay_ms(2 * REPEATDELAY);
     if (receivedCommand) {
        SetMode(modeB);
        String("----");
        return true;
        }
     if (*Code < 'D') {
        (*Code)++;
        return false;
        }
     }
  *Code = 0;
  return false;
}

void cRcuRemote::ChannelSwitch(const cDevice *Device, int ChannelNumber)
{
  if (ChannelNumber && Device->IsPrimaryDevice()) {
     LOCK_THREAD;
     numberToSend = cDevice::CurrentChannel();
     }
}

void cRcuRemote::Recording(const cDevice *Device, const char *Name)
{
  SetPoints(1 << Device->DeviceNumber(), Device->Receiving());
}
