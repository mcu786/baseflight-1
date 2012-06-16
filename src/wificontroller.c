#include "board.h"
#include "mw.h"

// driver for wifi receiver / sbus using UART2 .
// You *CAN NOT* use wificontroller and spektrum at the same time.
// Author: DY.Feng

#define WIFI_MAX_CHANNEL 7
#define WIFI_FRAME_SIZE 128
static uint8_t wifi_chan_shift;
static uint8_t wifi_chan_mask;
static bool rcFrameComplete = false;
static bool wifiDataIncoming = false;
volatile uint32_t wifiChannelData[WIFI_MAX_CHANNEL];
volatile uint8_t wifiFrame[WIFI_FRAME_SIZE];
static void wifiDataReceive(uint16_t c);


// external vars (ugh)
extern int16_t failsafeCnt;

void wifiInit(void)
{	
    uart2Init(115200, wifiDataReceive);
}


bool wifiCheckData()
{
	 uint8_t sum=0;
	 uint8_t i;
	 uint8_t size=wifiFrame[0]-2;
	 for (i=0;i<size;++i)
	 {
	 	 sum+=wifiFrame[i];
	 }
	 return sum == wifiFrame[size]; 
}

void printHex(int h)
{
	char tmp[16];
	sprintf(tmp,"%x ",h);
	uartPrint(tmp);
} 

// UART2 Receive ISR callback
static void wifiDataReceive(uint16_t c)
{		
    uint32_t wifiTime;
	uint8_t d;
    static uint32_t wifiTimeLast, wifiTimeInterval;
    static uint8_t  wifiFramePosition=0;
	static uint8_t 	wifiFrameHeader=0;
    wifiDataIncoming = true;
    wifiTime = micros();
    wifiTimeInterval = wifiTime - wifiTimeLast;
    wifiTimeLast = wifiTime;
	
	
    if (wifiTimeInterval > 5000) 
        wifiFramePosition = 0;
   	
	d = (uint8_t)c;
	
	//detecet the frame head
	if (wifiFramePosition!=0)
	{}else if (d == 0xa5 && wifiFrameHeader == 0)
	{		 
		wifiFrameHeader=0xa5;
	 	return;
	}else if (d == 0x5a && wifiFrameHeader == 0xa5)
	{
		wifiFrameHeader=0;
		wifiFramePosition=0;
		rcFrameComplete = false;
		return;
	}else if (wifiFrameHeader!=0)
	{
		wifiFrameHeader=0;
		return;
	}
	
	
    wifiFrame[wifiFramePosition] = d;

	//detecet the frame end
	if (d == 0xaa && (wifiFramePosition+1) == wifiFrame[0])
	{
		// check the date ok? completed this frame...
		rcFrameComplete = wifiCheckData(); 
//		printHex(rcFrameComplete);	
		wifiFramePosition = 0;
		failsafeCnt = 0;   // clear FailSafe counter
	}else{
		wifiFramePosition++;
	}

	//unexported frame size, overflow attack?
	if (wifiFramePosition>=WIFI_FRAME_SIZE)
	{
	   wifiFramePosition = 0;
	}
	   
}

bool wifiFrameComplete(void)
{
    return rcFrameComplete;
}

void assignChannelData()
{
	uint8_t b;
	uint8_t wifiChannel=0;
	for (b = 2; b < wifiFrame[0]; b += 2,++wifiChannel) {
		if (wifiChannel < WIFI_MAX_CHANNEL) 
			wifiChannelData[wifiChannel] = ((uint16_t)(wifiFrame[b]) << 8)+wifiFrame[b+1];
	}
	
}

// static const uint8_t wifiRcChannelMap[wifi_MAX_CHANNEL] = {1, 2, 3, 0, 4, 5, 6};


void assignWifiFrame()
{
	if (rcFrameComplete) {
		switch (wifiFrame[1])
		{
			case 0xf0:
			  	assignChannelData();
				break;
		}
		rcFrameComplete = false;
	}
}


// read the channel CHAN value from wifi 
uint16_t wifiReadRawRC(uint8_t chan)
{
    uint16_t data;
    
	if (chan >= WIFI_MAX_CHANNEL || !wifiDataIncoming) {
        data = cfg.midrc;
    }else
	{
		data = wifiChannelData[cfg.rcmap[chan]];
	}
   
    return data;
}
