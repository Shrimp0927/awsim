#ifndef ROTATING_QUEUE_H
#define ROTATING_QUEUE_H

#include <array>
#include <stdexcept>

template <typename T, size_t Capacity = 100>
class RotatingQueue
{
private:
	std::array<T, Capacity> buffer;
	size_t head = 0;
	size_t tail = 0;
	size_t count = 0;

public:
	void push(const T& item)
	{
		buffer[tail] = item;
		tail = (tail + 1) % Capacity;
		if (count < Capacity)
		{
			++count;
		}
		else
		{
			head = (head + 1) % Capacity; // Overwrite the oldest item
		}
	}

	void pop()
	{
		if (count > 0)
		{
			head = (head + 1) % Capacity;
			--count;
		}
	}

	T& front()
	{
		if (count == 0)
		{
			throw std::out_of_range("Queue is empty");
		}
		return buffer[head];
	}

	T& back()
	{
		if (count == 0)
		{
			throw std::out_of_range("Queue is empty");
		}
		return buffer[(tail == 0) ? Capacity - 1 : tail - 1];
	}

	bool empty() const noexcept 
	{
		return count == 0;
	}

	size_t size() const noexcept 
	{
		return count;
	}

	bool full() const noexcept 
	{
		return count == Capacity;
	}

	size_t capacity() const noexcept 
	{
		return Capacity;
	}

	void clear() noexcept 
	{
		head = 0;
		tail = 0;
		count = 0;
	}
};

#endif // ROTATING_QUEUE_H
