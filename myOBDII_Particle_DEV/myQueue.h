#ifndef _myQueue_h
#define _myQueue_h

class FaultCode
{
public:
	unsigned long time;
	unsigned long code;
	bool 					reset;
	FaultCode(void)
	{
		time 	= 0UL;
		code 	= 0UL;
		reset	= false;
	}
	FaultCode(const unsigned long tim, const unsigned long cod, const bool res)
	{
		time 	= tim;
		code 	= cod;
		reset = res;
	}
	FaultCode(const unsigned long tim, const unsigned long cod)
	{
		time 	= tim;
		code 	= cod;
		reset = false;
	}
	FaultCode (const FaultCode & FC)
	{
		time 	= FC.time;
		code 	= FC.code;
		reset	= FC.reset;
	}
	void operator=(const FaultCode & FC)
	{
//		if ( this  == &FC ) return *this; // self-assignment
		time 	= FC.time;
		code 	= FC.code;
		reset = FC.reset;
	}
	bool operator==(const FaultCode &FC )
	{
		if ( time==FC.time && code==FC.code ) return true;
		return false;
	}
	bool operator!=(const FaultCode &FC )
	{
		if ( time!=FC.time || code!=FC.code ) return true;
		return false;
	}
	bool isEqualTo(const FaultCode & B)
	{
		if ( time == B.time && code == B.code ) return true;
		return false;
	}
	bool isReset()
	{
		return reset;
	}
	void Print()
	{
		Serial.printf("| %d %d %d ", time, code, reset);
	}
	~FaultCode(){}
};

// FIFO Queue class
class Queue
{
private:
	int front_, rear_, maxSize_, gmt_;
	String name_;
	bool storing_;
	FaultCode *A_;
public:
	Queue(void);
	Queue(const int maxSize, const int GMT, const String name, const bool storing);
	Queue(const int front, const int rear, const int maxSize, const int GMT, const String name, const bool storing);
	int  clearNVM(int);
	bool IsEmpty(void);
	bool IsFull(void);
	void Enqueue(const FaultCode x);
	void EnqueueOver(const FaultCode x);
	void Dequeue(void);
	FaultCode  Front(void);
	FaultCode  Rear(void);
	void Print(void);
	int  front(void);
	int  numActive(void);
	int  printActive(void);
	int  printActive(String *str);
	int  printInActive(String *str, const int num);
	int  rear(void);
	int  maxSize(void);
	String name(void);
	int  loadNVM(const int start);
	int  loadRaw(const uint8_t i, const FaultCode x);
	FaultCode  getRaw(const uint8_t i);
	void newCode(const unsigned long tim, const unsigned long cod);
	int  resetAll(void);
	int  storeNVM(const int start);
};

#endif
