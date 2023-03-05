#include "InnoMakerUsb2CanLib.h"

const int START_MESSAGE = 42;
const int STOP_MESSAGE = 21;
const int NUM_PERCENT = 2;
const int NO_DEVICE = 1;
const std::chrono::milliseconds TIME_STEP = 50ms; //time separating each command in thrust array
const std::chrono::milliseconds TIME_OFFSET = 0ms; //wait less offset modifier if needed 

class UsbConfig {
public:
	UsbConfig(InnoMakerUsb2CanLib::innomaker_can* can, InnoMakerUsb2CanLib* usbCanLib, InnoMakerUsb2CanLib::InnoMakerDevice* firstDevice, int can_id) {
		this->can = can;
		this->usbCanLib = usbCanLib;
		this->firstDevice = firstDevice;
		this->can_id = can_id;
	}
	InnoMakerUsb2CanLib::innomaker_can* can;
	InnoMakerUsb2CanLib* usbCanLib;
	InnoMakerUsb2CanLib::InnoMakerDevice* firstDevice;
	int can_id;
};
int bytesToInt(BYTE* bytes, int start)
{
	int a = bytes[start] & 0xFF;
	a |= ((bytes[start + 1] << 8) & 0xFF00);
	a |= ((bytes[start + 2] << 16) & 0xFF0000);
	a |= ((bytes[start + 3] << 24) & 0xFF000000);
	return a;
}
//https://stackoverflow.com/questions/24420246/c-function-to-convert-float-to-byte-array
void floatToByteArray(BYTE* byte_ptr, float f) {
	unsigned int asInt = *((int*)&f);

	int i;
	for (i = 0; i < 4; i++) {
		byte_ptr[i] = (asInt >> 8 * i) & 0xFF;
	}
}
float byteArrayTofloat(BYTE* byte_ptr) {
	float f;
	memcpy(&f, byte_ptr, sizeof(f));
	return f;

}
struct InnoMakerUsb2CanLib::innomaker_host_frame* constructFrame(BYTE* buffer) {
	int echoId = bytesToInt(buffer, 0);
	int ID = bytesToInt(buffer, 4);
	BYTE can_dlc = buffer[8];
	BYTE channel = buffer[9];
	BYTE flags = buffer[10];
	BYTE reserved = buffer[11];
	BYTE data[8];
	for (int i = 0; i < 8; i++) {
		data[i] = buffer[12 + i];
	}
	BYTE timestamp_us = bytesToInt(buffer, 20);

	InnoMakerUsb2CanLib::innomaker_host_frame* frame = (InnoMakerUsb2CanLib::innomaker_host_frame*)malloc(sizeof(InnoMakerUsb2CanLib::innomaker_host_frame));
	frame->echo_id = echoId;
	frame->can_id = ID;
	frame->channel = channel;
	frame->flags = flags;
	frame->reserved = reserved;
	memcpy(frame->data, data, 8);
	frame->timestamp_us = timestamp_us;

	return frame;
}
void destroyFrame(struct InnoMakerUsb2CanLib::innomaker_host_frame* frame) {
	free(frame);
}
string dataToString(struct InnoMakerUsb2CanLib::innomaker_host_frame* frame) {
	std::string s(reinterpret_cast<char const*>(frame->data), 8);
	return s;
}

void recvTask(UsbConfig* config, std::atomic<BOOL>* sharedStart, std::atomic<BOOL>* sharedStop, std::atomic<BOOL>* finish_event) {

	InnoMakerUsb2CanLib* usbCanLib = config->usbCanLib;
	InnoMakerUsb2CanLib::InnoMakerDevice* firstDevice = config->firstDevice;

	while (!*finish_event) {
		if (NO_DEVICE) {
			printf("RECEIVE THREAD NO DEVICE STARTING COMMAND");
		}
		else {
			std::cout << "RECEIVING" << "\n";
			BYTE recvBuffer[sizeof(InnoMakerUsb2CanLib::innomaker_host_frame)];
			memset(recvBuffer, 0, sizeof(InnoMakerUsb2CanLib::innomaker_host_frame));
			bool result = usbCanLib->recvInnoMakerDeviceBuf(firstDevice, recvBuffer, sizeof(InnoMakerUsb2CanLib::innomaker_host_frame), 10);
			if (result) {
				InnoMakerUsb2CanLib::innomaker_host_frame* frame = constructFrame(recvBuffer);

				if (frame->echo_id != 0xffffffff) {
					printf("Recv a echo frame \n");

					/// Find the context that transfer before by echoid 
					InnoMakerUsb2CanLib::innomaker_tx_context* txc = usbCanLib->innomaker_get_tx_context(config->can, frame->echo_id);
					///bad devices send bad echo_ids.
					if (txc == NULL) {
						printf("RECV FAIL:Bad Devices Send Bad Echo_ids");
						continue;
					}
					//printf("Echo ID %x \n", frame->echo_id);
					//printf("Can ID %x \n", frame->can_id);
					//printf("Channel %x \n", frame->channel);
					//printf("Flags %x \n", frame->flags);
					//printf("Reserved %x \n", frame->reserved);
					/*for (int i = 0; i < 8; i++) {
						printf("%x", frame->data[i]);
					}*/
					//cout << dataToString(frame);
					printf("\n");
					/// Free context
					usbCanLib->innomaker_free_tx_context(txc);
				}
				else {
					printf("Recv a frame \n");

					if (bytesToInt(frame->data, 0) == START_MESSAGE) {
						*sharedStart = true;
					}
					else if (bytesToInt(frame->data, 0) == STOP_MESSAGE) {
						*sharedStop = true;
					}
					cout << dataToString(frame);


				}

				destroyFrame(frame);
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10)); //run receive procedure every 10 milliseconds
	}
}
void sendTask(UsbConfig* config, BYTE* data, std::atomic<BOOL>* sharedStop) {

	InnoMakerUsb2CanLib* usbCanLib = config->usbCanLib;
	InnoMakerUsb2CanLib::InnoMakerDevice* firstDevice = config->firstDevice;
	int can_id = config->can_id;
	/// Test Send 
	if (NO_DEVICE) {
		printf("SEND TASK NO DEVICE\n PRINTING DATA: ");
		cout << "Float (1): " << byteArrayTofloat(data) << ",  ";
		cout << "Float (2): " << byteArrayTofloat(data + 4) << "\n";
		
		return;
	}

	if (*sharedStop) {
		printf("Message stop received: Canceled sending messages");
	}

	InnoMakerUsb2CanLib::innomaker_host_frame frame;

	for (int i = 0; i < 8; i++) {
		frame.data[i] = data[i];
	}
	frame.can_id = can_id;

	frame.can_dlc = 8; //data length code, length of message in bytes

	/*Not using currently*/
	frame.channel = 0;
	frame.flags = 0;
	frame.reserved = 0;


	// Find an empty context to keep track of transmission 
	InnoMakerUsb2CanLib::innomaker_tx_context* txc = usbCanLib->innomaker_alloc_tx_context(config->can);
	if (txc->echo_id == 0xff)
	{
		printf("SEND FAIL: NETDEV_BUSY \n");
	}

	frame.echo_id = txc->echo_id;

	BYTE sendBuffer[sizeof(InnoMakerUsb2CanLib::innomaker_host_frame)];
	memcpy(sendBuffer, &frame, sizeof(InnoMakerUsb2CanLib::innomaker_host_frame));
	bool result = usbCanLib->sendInnoMakerDeviceBuf(firstDevice, sendBuffer, sizeof(InnoMakerUsb2CanLib::innomaker_host_frame), 10);
	if (result) {
		printf("Send a frame \n");
	}
	else {
		/// If send fail,free the context by echo id 
		InnoMakerUsb2CanLib::innomaker_tx_context* txc1 = usbCanLib->innomaker_get_tx_context(config->can, txc->echo_id);
		if (txc1 != NULL)
		{
			usbCanLib->innomaker_free_tx_context(txc1);
		}
	}

}