// InnoMakerUsb2Can.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "InnoMakerUsb2CanLib.h"
#include <thread>
#include <windows.h>
#include "parser.cpp"

InnoMakerUsb2CanLib::innomaker_can *can;
string config_file_path = "./configthrust.txt";


std::wstring ExePath() {
	TCHAR buffer[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, buffer, MAX_PATH);
	std::wstring::size_type pos = std::wstring(buffer).find_last_of(L"\\/");
	return std::wstring(buffer).substr(0, pos);
}



void handleKeyboardInput(std::atomic<BOOL>* sharedStart, std::atomic<BOOL>* sharedStop, std::atomic<BOOL>* paused, std::atomic<BOOL>* finish_event) {
	while(!*finish_event) {
		if (*paused || *sharedStart) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		else if (GetKeyState(VK_ESCAPE) & 0x8000) {
			*finish_event = true;
		}
		else if (GetKeyState('P') & 0x8000) {
			*paused = true;
			//cout << "P received, paused \n";
		}
		else if (GetKeyState('S') & 0x8000) {
			//cout << "S received, start \n";
			*sharedStart = true;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10)); //run receive procedure every 1 milliseconds
	}
}



int main()
{
	std::wcout << "my directory is " << ExePath() << "\n";
	/// Setup
	InnoMakerUsb2CanLib* usbCanLib = new InnoMakerUsb2CanLib();
	usbCanLib->setup();

	/// Scan devices
	usbCanLib->scanInnoMakerDevice();
	int deviceNums = usbCanLib->getInnoMakerDeviceCount();
	printf("Find Device Count = %d \n", deviceNums);

	
	if (!NO_DEVICE) {
		if (deviceNums <= 0) return 0;
	}
	
	/// Get first devices
	InnoMakerUsb2CanLib::InnoMakerDevice* firstDevice = usbCanLib->getInnoMakerDevice(0);

	/// Set usb can mode
	InnoMakerUsb2CanLib::UsbCanMode usbCanMode = InnoMakerUsb2CanLib::UsbCanMode::UsbCanModeLoopback;
	InnoMakerUsb2CanLib::Innomaker_device_bittming bittming;

	/// 20K
	bittming.prop_seg = 6;
	bittming.phase_seg1 = 7;
	bittming.phase_seg2 = 2;
	bittming.sjw = 1;
	bittming.brp = 150;
	
	if (!NO_DEVICE) {
		usbCanLib->openInnoMakerDevice(firstDevice);
		usbCanLib->urbResetDevice(firstDevice);
		usbCanLib->closeInnoMakerDevice(firstDevice);
	}
	if (!NO_DEVICE) {
		int result = usbCanLib->urbSetupDevice(firstDevice, usbCanMode, bittming);
		if (!result) {
			printf("setup fail");
			return 0;
		}
	}

	can = new InnoMakerUsb2CanLib::innomaker_can();

	/// Init tx context 
	for (int i = 0; i < usbCanLib->innomaker_MAX_TX_URBS; i++)
	{
		can->tx_context[i].echo_id = usbCanLib->innomaker_MAX_TX_URBS;
	}
	
	int can_id = 1;
	
	UsbConfig* config = new UsbConfig(can, usbCanLib, firstDevice, can_id);

	BYTE data[8];
	std::vector<AbstractCommand*> commands = parse(config_file_path);
	std::atomic<BOOL> sharedStop = false;
	std::atomic<BOOL> sharedStart = false;
	std::atomic<BOOL> finishEvent = false;
	std::atomic<BOOL> paused = false;
	CommandControl* control = new CommandControl(&sharedStop, &sharedStart, &finishEvent, &paused, data, &commands);
	control->setUsbConfig(config);

	
	//thread recv_thread = std::thread(recvTask, usbCanLib, firstDevice, &sharedStart, &sharedStop, &finishEvent);
	thread keyboard_event_thread = std::thread(handleKeyboardInput, &sharedStart, &sharedStop, &paused, &finishEvent);
	control->execute();
	
	
	//recv_thread.join();
	keyboard_event_thread.join();
	if (!NO_DEVICE) {
		usbCanLib->urbResetDevice(firstDevice);
		usbCanLib->closeInnoMakerDevice(firstDevice);
		usbCanLib->setdown();
	}
	
	return 0;
}

