#include "Clock.cpp"

class AbstractCommand {
	public:
		enum MODE { READY, RUNNING, PAUSED, FINISHED };
		MODE mode;
		const std::chrono::milliseconds SEND_MESSAGE_PERIOD = 70ms;
		AbstractCommand(Duration duration, const std::chrono::milliseconds period) {
			this->clock = new Clock(duration);
			this->mode = READY;
			this->commandPeriod = period;
		}
		~AbstractCommand() {
			delete clock;
		}
		

		void resume() {
			if (mode == PAUSED) {
				clock->start();
				mode = RUNNING;
			}
			else if (mode != RUNNING) {
				throw std::invalid_argument("Illegal operation: attempting to resume when not PAUSED");
			}
		}
		void start(float* data_arr, int size, std::chrono::milliseconds time_step) {
			if (mode == READY) {
				clock->start();
				execute(data_arr, size, time_step);
				mode = RUNNING;
			}
			else if (mode != RUNNING) {
				throw std::invalid_argument("Illegal operation: attempting to start when not READY");
			}
		}
		Clock* getClock() {
			return clock;
		}
		void pause() {
			if (mode == RUNNING) {
				clock->pause();
				mode = PAUSED;
			}
			else if (mode != PAUSED) {
				throw std::invalid_argument("Illegal operation: attempting to pause when not RUNNING");
			}
		}
		long long stop() {
			mode = FINISHED;
			return clock->getTimeElapsed();
		}
		virtual void execute(float* data_arr, int size, std::chrono::milliseconds time_step) = 0;
		
	protected:
		Clock* clock;
		std::chrono::milliseconds commandPeriod;
};
/*
* There must be at most one ThrottleCommand in the RUNNING state at once, assumes all throttle commands occur right after one another
*/
class ThrottleCommand : public AbstractCommand {
	public:
		ThrottleCommand(Duration duration, const std::chrono::milliseconds period)
			: AbstractCommand(duration, period) {
			next = NULL;
		}
		/*
		 * update next thrust value
		 */
		void execute(float* data_arr, int size, std::chrono::milliseconds time_step) override {
			update(data_arr, size, time_step);
		}
		void setNext(ThrottleCommand* next) {
			this->next = next;
		}
		virtual void update(float* thrust_arr, int size, std::chrono::milliseconds time_step) = 0;
	protected:
		ThrottleCommand* next;
};

class CommandConstant : public ThrottleCommand {
	public:
		CommandConstant(Duration duration, double thrust)
			: ThrottleCommand(duration, SEND_MESSAGE_PERIOD)
		{
			this->thrust = (float)thrust;
		}
		void update(float* thrust_arr, int size, std::chrono::milliseconds time_step) override {

			for (int i = 0; i < size; i++) {
				if (clock->isDurationCompleted(time_step * i, mode == RUNNING)) {
					if (i == 0) {
						mode = FINISHED; //this is the first slot of array if its finished then we know the whole command is finished
					}
					if (next != NULL) {
						cout << "found new command" << "\n";
						next->update(thrust_arr + i, size - i, time_step);
					}
					else {
						for (i; i < size; i++) { thrust_arr[i] = 0; } //if we are the last command then fill remaining part of array with 0's
					}
					return;
				}
				else {
					thrust_arr[i] = thrust;
				}
				cout << "Constant thrust: " << thrust_arr[i] << "\n";
			}
		}
	private:
		double thrust;
};


class CommandLinear : public virtual ThrottleCommand {
	public:
		CommandLinear(Duration duration, double startPercentage, double endPercentage)
			: ThrottleCommand(duration, SEND_MESSAGE_PERIOD)
		{
			startPer = startPercentage;
			endPer = endPercentage;
		}
		void update(float* thrust_arr, int size, std::chrono::milliseconds time_step) override {
			
			for (int i = 0; i < size; i++) {
				if (clock->isDurationCompleted(time_step * i, mode == RUNNING)) {
					if (i == 0) {
						mode = FINISHED; //this is the first slot of array if its finished then we know the whole command is finished
					}
					if (next != NULL) { 
						cout << "found new command" << "\n";
						next->update(thrust_arr + i, size - i, time_step); 
					}
					else { 
						for (i; i < size; i++) { thrust_arr[i] = 0; }
					}
					return;
				}
				else {
					thrust_arr[i] = ((endPer - startPer) * (clock->getTimeElapsed() + time_step.count() * i) / clock->getDuration()) + startPer;
				}
				cout << "Linear thrust: " << thrust_arr[i] << "\n";
			}
		}
	private:
		double startPer;
		double endPer;
};