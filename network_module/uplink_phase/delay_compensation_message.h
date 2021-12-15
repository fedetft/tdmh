#ifndef DELAY_COMPENSATION_MESSAGE_H_
#define DELAY_COMPENSATION_MESSAGE_H_

#include "../downlink_phase/timesync/roundtrip/led_bar.h"
#include "../util/packet.h"
#include "../mac_context.h"

#define DELAY_COMPENSATION_MESSAGE_SIZE	32

namespace mxnet {

class DelayCompensationMessage
{
	public:
		DelayCompensationMessage(int delayVal) :ledBar(0)
		{
			setValue(delayVal);
		};

		DelayCompensationMessage() :ledBar(0)
		{};

		virtual ~DelayCompensationMessage() {};

		void setValue(int delayVal)
		{
			ledBar = LedBar<DELAY_COMPENSATION_MESSAGE_SIZE>(delayVal);
			packet.clear();
			packet.put(ledBar.getPacket(), DELAY_COMPENSATION_MESSAGE_SIZE);
			received = false;
		}

		int	getValue()
		{
			if(!received)
				return -1;

			auto decoded = ledBar.decode();

			if(!decoded.second)
				return -2;
			else
				return decoded.first;
		}

		void send(MACContext& ctx, long long sendTime)
		{
			packet.send(ctx, sendTime);
		}

		bool rcv(MACContext& ctx, long long tExpected)
		{
			received = false;

			packet.clear();
			auto rcvResult = packet.recv(ctx, tExpected);
		    if(rcvResult.error != miosix::RecvResult::ErrorCode::OK)
		        return false;

		    timestamp = rcvResult.timestamp;

		    unsigned char data[DELAY_COMPENSATION_MESSAGE_SIZE];
		    packet.get(data, DELAY_COMPENSATION_MESSAGE_SIZE);
		    ledBar = LedBar<DELAY_COMPENSATION_MESSAGE_SIZE>(data);

		    received = true;
		    return true;
		}

		long long getTimestamp()
		{
			return timestamp;
		}

	private:
		LedBar<DELAY_COMPENSATION_MESSAGE_SIZE> ledBar;
		Packet 									packet;
		long long 								timestamp;
		bool 									received=false;

};

}

#endif