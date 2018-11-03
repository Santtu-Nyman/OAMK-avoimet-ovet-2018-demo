// OAMK avoimet ovet 2018 demo by Santtu Nyman

#include "mbed.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ask_transmitter.h"
#include "ask_receiver.h"
#include "jpeg.h"

//#define SENDER_DEVICE
#define GATEWAY_DEVICE

#ifdef SENDER_DEVICE
#define DEVICE_ADDRESS 1
#endif
#ifdef GATEWAY_DEVICE
#define DEVICE_ADDRESS 2
#endif
#ifndef DEVICE_ADDRESS
#define DEVICE_ADDRESS 0xFF
#endif

DigitalOut led1(LED1, 0);
Serial pc(USBTX, USBRX);
ask_receiver_t receiver(2500, D2, DEVICE_ADDRESS);
ask_transmitter_t transmitter(2500, D4, DEVICE_ADDRESS);

size_t simple_arq_receive(ask_receiver_t* receiver, ask_transmitter_t* transmitter, size_t buffer_size, void* buffer)
{
	uint8_t packet_message[47];
	uint8_t packet_number = 0;
	size_t bytes_received = 0;
	for (uint8_t more_packets = 1; more_packets;)
	{
		size_t message_length = receiver->recv(packet_message, 47);
		if (message_length)
		{
			transmitter->send(packet_message, 1);
			if ((packet_message[0] & 0x7F) == packet_number)
			{
				size_t packet_data_length = message_length - 1;
				if (bytes_received + packet_data_length <= buffer_size)
					memcpy((uint8_t*)buffer + bytes_received, packet_message + 1, packet_data_length);
				else if (bytes_received < buffer_size)
					memcpy((uint8_t*)buffer + bytes_received, packet_message + 1, buffer_size - bytes_received);
				bytes_received += packet_data_length;
				packet_number = (packet_number + 1) & 0x7F;
			}
			more_packets = packet_message[0] >> 7;
		}
	}
	if (bytes_received <= buffer_size)
		return bytes_received;
	else
		return buffer_size;
}

void simple_arq_send(ask_receiver_t* receiver, ask_transmitter_t* transmitter, size_t message_size, const void* message)
{
	Timer timer;
	uint8_t packet_message[47];
	uint8_t packet_number = 0;
	uint8_t response;
	for (size_t bytes_transmitted = 0; bytes_transmitted != message_size;)
	{
		size_t packet_message_length = (((message_size - bytes_transmitted) > 46) ? 46 : (message_size - bytes_transmitted));
		packet_message[0] = packet_number;
		if (bytes_transmitted + packet_message_length < message_size)
			packet_message[0] |= 0x80;
		memcpy(packet_message + 1, (const uint8_t*)message + bytes_transmitted, packet_message_length);
		transmitter->send(packet_message, 1 + packet_message_length);
		timer.reset();
		timer.start();
		bool respond_received = false;
		while (!respond_received && timer.read_ms() < 400)
			if (receiver->recv(&response, 1))
				respond_received = true;
		timer.stop();
		if (respond_received && response == packet_message[0])
		{
			packet_number = (packet_number + 1) & 0x7F;
			bytes_transmitted += packet_message_length;
		}
	}
}

int main()
{
#ifdef SENDER_DEVICE
	for (;;)
	{
		led1 = 1;
		simple_arq_send(&receiver, &transmitter, sizeof(jpeg_table), jpeg_table);
		led1 = 0;
		wait(1.0f);
	}
#endif

#ifdef GATEWAY_DEVICE
	static uint8_t buffer[4096];
	for (;;)
	{
		led1 = 1;
		size_t message_size = simple_arq_receive(&receiver, &transmitter, sizeof(buffer), buffer);
		led1 = 0;
		for (size_t i = 0; i != message_size; ++i)
			pc.putc(*(char*)&buffer[i]);
	}
#endif

/*
	static uint8_t buffer[1024];
	for (;;)
	{
		size_t message_size = receiver.recv(buffer, sizeof(buffer));
		if (message_size)
		{
			for (size_t i = 0; i != message_size; ++i)
				pc.printf("%02X", buffer[i]);
			pc.printf("\n");
		}
			//pc.putc(*(char*)(buffer + i));
	}
*/

	return 0;
}

