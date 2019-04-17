#include <iostream>
#include<condition_variable>
#include<functional>
#include<future>
#include<vector>
#include<thread>
#include<queue>

//This example is abouta swimming pool. It can contain up to 600 cubes of water.
//The pool has two water taps. If the water level is below 600,
//both water taps start to fill in water. If the water level is above 600,
//only one of the taps throws out water untill the water level reaches 600. 
// The program gets the level of water from the console input.


class ThreadPool
{
public:

	using Task = std::function<void()>;
	explicit ThreadPool(std::size_t numThreads);
	~ThreadPool();

	template<class T>
	auto doTask(T task)->std::future<decltype(task())>;
	void print(float &i);

private:

	std::vector<std::thread> mthreads;
	std::condition_variable mEventvar;
	std::mutex mEventMutex;
	bool mStopping = false;
	std::queue<Task> mTasks;

	void start(std::size_t numThreads);
	void stop() noexcept;
};

ThreadPool::ThreadPool(std::size_t numThreads)
{
	start(numThreads);
}
ThreadPool::~ThreadPool()
{
	stop();
}

template<class T>
auto ThreadPool::doTask(T task)-> std::future<decltype(task())>
{
	auto wrapper = std::make_shared<std::packaged_task<decltype(task()) ()>>(std::move(task));
	{
		std::unique_lock<std::mutex> lock{ mEventMutex };
		mTasks.emplace([=] {
			(*wrapper)();
		});
	}
	mEventvar.notify_one();
	return wrapper->get_future();
}

void ThreadPool::print(float &i)
{
	std::unique_lock<std::mutex> lock{ mEventMutex };
	std::cout << "Thread " << std::this_thread::get_id() << "  " << i << std::endl;
}

void ThreadPool::start(std::size_t numThreads)
{
	for (auto i = 0u; i < numThreads; ++i)
	{
		mthreads.emplace_back([=] {
			while (true)
			{
				Task task;
				{
					std::unique_lock<std::mutex> lock{ mEventMutex };
					mEventvar.wait(lock, [=] {return mStopping || !mTasks.empty(); });
					if (mStopping && mTasks.empty())
					{
						break;
					}
					task = std::move(mTasks.front());
					mTasks.pop();
				}
				task();
			}
		});
	}
}

void ThreadPool::stop() noexcept
{
	{
		std::unique_lock<std::mutex> lock{ mEventMutex };
		mStopping = true;
	}
	mEventvar.notify_all();
	for (auto &thread : mthreads)
		thread.join();
}

int main()
{
	float storage = 0.0f;
	ThreadPool pool(2); // Create pool with 2 threads

	while (true) {
		std::cin >> storage;
		if (storage > 1000 || storage < 0)
		{
			std::cout << "Error!" << std::endl;
		}
		else
		{
			if (storage < 600)
			{
				auto f1 = pool.doTask([&storage, &pool] {

					while (storage < 600)
					{
						storage += 1;
						pool.print(storage);
						std::this_thread::sleep_for(std::chrono::milliseconds(500));
					}
					return storage;
				});
				auto f2 = pool.doTask([&storage, &pool] {
					while (storage < 600)
					{
						storage += 2;
						pool.print(storage);
						std::this_thread::sleep_for(std::chrono::milliseconds(500));
					}
					return storage;
				});
			}
			else
			{
				auto f3 = pool.doTask([&storage, &pool] {
					while (storage > 600)
					{
						storage -= 0.5;
						pool.print(storage);
						std::this_thread::sleep_for(std::chrono::milliseconds(500));
					}
					return storage;
				});
			}
		}
	}

	return 0;
}