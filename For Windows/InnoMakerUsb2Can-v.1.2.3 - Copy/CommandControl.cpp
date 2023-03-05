#include "InnoMakerUsb2CanLib.h"
#include "UsbConfig.cpp"
using CAN = InnoMakerUsb2CanLib;

class CommandControl {
	public: 
		CommandControl(std::atomic<BOOL>* sharedStop, std::atomic<BOOL>* sharedStart, std::atomic<BOOL>* finishEvent, std::atomic<BOOL>* paused, BYTE* data, std::vector<AbstractCommand*>* commands) {
			this->sharedStop = sharedStop;
			*sharedStop = false;
			this->sharedStart = sharedStart;
			*sharedStart = false;
			this->finishEvent = finishEvent;
			*finishEvent = false;
			this->paused = paused;
			*paused = false;
			this->commands = commands;
			this->data = data;
		}
		void setUsbConfig(UsbConfig* config) {
			this->config = config;
		}
		void execute() {
			for (int i = 0; i < commands->size(); i++) {
				float thrust_per[NUM_PERCENT];
				AbstractCommand* command = (*commands)[i];

				while (command->mode != command->FINISHED) {
					if (*sharedStop) {
						command->stop();
						break;
					}
					else if (*paused) {
						command->pause();
						*paused = false;
					}
					else if (*sharedStart) {
						if (command->mode == command->READY) {
							command->start(thrust_per, NUM_PERCENT, TIME_STEP);

							for (int i = 0; i < NUM_PERCENT; i += 2) {
								floatToByteArray(&data[i * 4], thrust_per[i]);
								floatToByteArray(&data[(i + 1) * 4], thrust_per[i + 1]);
							}

							thread send_thread = std::thread(sendTask, config, data, sharedStop);
							send_thread.join();
							command->getClock()->updateLastSendTime();
						}
						else if (command->mode == command->PAUSED) {
							command->resume();
						}
						*sharedStart = false;
					}
					else if (command->mode == command->RUNNING) {
						std::this_thread::sleep_until(command->getClock()->timeTillSend(TIME_STEP));
						command->getClock()->updateLastSendTime();
						command->execute(thrust_per, NUM_PERCENT, TIME_STEP);
						//LOAD next two floats into byte array

						for (int i = 0; i < NUM_PERCENT; i += 2) {
							floatToByteArray(&data[i * 4], thrust_per[i]);
							floatToByteArray(&data[(i + 1) * 4], thrust_per[i + 1]);
						}
						
						thread send_thread = std::thread(sendTask, config, data, sharedStop);
						send_thread.join();
						
					}

				}
			}
		}
	private:
		std::atomic<BOOL>* sharedStop;
		std::atomic<BOOL>* sharedStart;
		std::atomic<BOOL>* finishEvent;
		std::atomic<BOOL>* paused;
		BYTE* data;
		UsbConfig* config;
		std::vector<AbstractCommand*>* commands;
};