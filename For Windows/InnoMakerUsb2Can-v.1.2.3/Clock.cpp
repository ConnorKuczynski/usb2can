#include <chrono>
using Duration = std::chrono::duration<double, std::milli>;
class Clock {
	public:
		Clock(Duration duration) {
			this->duration = duration;
			this->timeElapsed = 0ms;
			this->durationCompleted = 0ms;

		}
		/*
		* returns true if time is total duration is completed
		*/
		boolean isDurationCompleted(std::chrono::milliseconds time_in_future, bool RUNNING) {
			if (RUNNING) {
				return (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime) + durationCompleted + time_in_future > duration);
			}
			else {
				return (time_in_future > duration);
			}
		}
		long long getTimeElapsed() {
			return (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime) + durationCompleted).count();
		}
		double getDuration() {
			return duration.count();
		}
		void updateLastSendTime() {
			sendTimeOfLastUpdate = std::chrono::steady_clock::now();
		}
		std::chrono::time_point<std::chrono::steady_clock, std::chrono::steady_clock::duration> timeTillSend(std::chrono::milliseconds time_step) {
			if (std::chrono::steady_clock::now() - sendTimeOfLastUpdate < time_step) {
				return (std::chrono::steady_clock::now() - sendTimeOfLastUpdate - time_step + std::chrono::steady_clock::now() - OFFSET);
			}
			else {
				return std::chrono::steady_clock::now();
			}
		}
		
		void start() {
			startTime = std::chrono::steady_clock::now();
		}
		void pause() {
			durationCompleted = durationCompleted + std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
		}
	private:
		std::chrono::time_point<std::chrono::steady_clock> startTime;
		std::chrono::time_point<std::chrono::steady_clock> sendTimeOfLastUpdate;
		std::chrono::milliseconds timeElapsed;
		std::chrono::milliseconds durationCompleted;
		const std::chrono::milliseconds OFFSET = 0ms; //used for if we want to shift wait time
		Duration duration;
};