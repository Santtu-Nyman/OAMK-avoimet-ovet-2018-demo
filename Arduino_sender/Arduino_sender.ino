// OAMK avoimet ovet 2018 demo by Santtu Nyman

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <SPI.h>
#include <RH_ASK.h>
#include "jpeg.h"

#define RH_SPEED 2500
#define RH_RX_PIN 11
#define RH_TX_PIN 12
#define RH_PTT_PIN 10
#define RH_INVERTED 0

RH_ASK rh_driver(RH_SPEED, RH_RX_PIN, RH_TX_PIN, RH_PTT_PIN, RH_INVERTED, true);

void setup()
{
	Serial.begin(9600);
	rh_driver.setThisAddress(1);
	rh_driver.init();
}

void loop()
{
	uint8_t packet_message[47];
	size_t packet_message_length;
	uint8_t response;
  uint8_t packet_number = 0;
  bool respond_received = true;
	for (size_t bytes_transmitted = 0; bytes_transmitted != sizeof(jpeg_table);)
	{
		if (respond_received)
		{
			packet_message_length = (((sizeof(jpeg_table) - bytes_transmitted) > 46) ? 46 : (sizeof(jpeg_table) - bytes_transmitted));
			packet_message[0] = packet_number;
			if (bytes_transmitted + packet_message_length < sizeof(jpeg_table))
				packet_message[0] |= 0x80;
			for (size_t i = 0; i != packet_message_length; ++i)
				*(byte*)&packet_message[1 + i] = pgm_read_byte_near(jpeg_table + bytes_transmitted + i);
		}
		rh_driver.send(packet_message, 1 + packet_message_length);
		respond_received = false;
		for (unsigned long begin_time = millis(); !respond_received && millis() - begin_time < 400;)
		{
			uint8_t bytes_received = 1;
			if (rh_driver.recv(&response, &bytes_received) && bytes_received && response == packet_message[0])
      {
  		  respond_received = true;
        packet_number = (packet_number + 1) & 0x7F;
        bytes_transmitted += packet_message_length;
      }
		}
	}
	delay(1000);
}
