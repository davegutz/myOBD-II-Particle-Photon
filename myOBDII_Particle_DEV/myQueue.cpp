#include "application.h"
#include "myQueue.h"

// class Queue
// constructors
Queue::Queue()
: front_(-1), rear_(-1), maxSize_(0), gmt_(0), name_(""), storing_(true), verbose_(0)
{}
Queue::Queue(const int maxSize, const int GMT, const String name, const bool storing, const int verbose)
: front_(-1), rear_(-1), maxSize_(maxSize), gmt_(GMT), name_(name), storing_(true), verbose_(verbose)
{
	A_ 				= new FaultCode[maxSize_];
}
Queue::Queue(const int front, const int rear, const int maxSize, const int GMT, const String name, const bool storing, const int verbose)
: front_(front), rear_(rear), maxSize_(maxSize), gmt_(GMT), name_(name), storing_(storing), verbose_(verbose)
{
	A_ 				= new FaultCode[maxSize_];
}

// operators

// functions
// Clears NVM by reinitting the pointers
int Queue::clearNVM(int start)
{
	int p = start;
	FaultCode val;
	val.time = 0UL; val.code = 0UL; val.reset = false;
	EEPROM.put(p, int(-1)); 	p += sizeof(int);
	EEPROM.put(p, int(-1));		p += sizeof(int);
	EEPROM.put(p, maxSize_);	p += sizeof(int);
	for ( uint8_t i=0; i<maxSize_; i++ )
	{
		EEPROM.put(p, val); p += sizeof(FaultCode);
	}
	// verify
	int test;
	FaultCode tc;
	p = start;
	EEPROM.get(p, test); Serial.printf("%d",test);if ( test!=-1   			) return -1; p += sizeof(int);
	EEPROM.get(p, test); Serial.printf("%d",test);if ( test!=-1   			) return -1; p += sizeof(int);
	EEPROM.get(p, test); Serial.printf("%d",test);if ( test!=maxSize_ 	) return -1; p += sizeof(int);
	for ( uint8_t i=0; i<maxSize_; i++ )
	{
		EEPROM.get(p, tc);
		Serial.printf("%u P%04u %d\n", tc.time, tc.code, tc.reset);
		if ( tc.time!=val.time || tc.code!=val.code || tc.reset!=val.reset ) return -1;
		p += sizeof(FaultCode);
	}
	if ( verbose_>4 ) Serial.printf("Verified clear.\n");
	return p;
}

// Removes an element in Queue from front_ end.
void Queue::Dequeue()
{
	if ( verbose_>4 ) Serial.printf("Dequeuing \n");
	if(IsEmpty())
	{
		if ( verbose_>0 ) Serial.println(name_ + ": empty queue");
		return;
	}
	else if(front_ == rear_ )
	{
		rear_ = front_ = -1;
	}
	else
	{
		front_ = (front_+1)%maxSize_;
	}
}

// Inserts an element in queue at rear_ end
void Queue::Enqueue(const FaultCode x)
{
	Serial.printf("Enqueuing P%04u\n", x.code);
	if(IsFull())
	{
		if ( verbose_>0 ) Serial.println(name_ + ": queue is full");
		return;
	}
	if (IsEmpty())
	{
		front_ = rear_ = 0;
	}
	else
	{
	    rear_ = (rear_+1)%maxSize_;
	}
	A_[rear_] = FaultCode(x);
}

// Inserts an element in queue at rear_ end.  Pops one off if full
void Queue::EnqueueOver(const FaultCode x)
{
	if ( verbose_>4 ) Serial.printf("Enqueuing %u\n", x.code);
	if(IsFull())
	{
		Queue::Dequeue();
	}
	if (IsEmpty())
	{
		front_ = rear_ = 0;
	}
	else
	{
	    rear_ = (rear_+1)%maxSize_;
	}
	A_[rear_] = x;
}

// Returns element at front_ of queue.
FaultCode Queue::Front()
{
	if(front_ == -1)
	{
		if ( verbose_>0 ) Serial.println(name_ + ": no front; empty queue");
		return FaultCode(0UL, 0UL);
	}
	return A_[front_];
}

// Returns front_ value.
int Queue::front()
{
	return front_;
}


// Return queue to memory
FaultCode Queue::getRaw(const uint8_t i)
{
	if ( i>= maxSize_ )
	{
		if ( verbose_>1 ) Serial.printf("Request ignored: %d\n", i);
		return FaultCode(0UL, 0UL);
	}
	return(A_[i]);
}

// To check wheter Queue is empty or not
bool Queue::IsEmpty()
{
	return (front_ == -1 && rear_ == -1);
}

// To check whether Queue is full or not
bool Queue::IsFull()
{
	return (rear_+1)%maxSize_ == front_ ? true : false;
}

// Load queue from memory
int Queue::loadRaw(const uint8_t i, const FaultCode x)
{
	if ( i>= maxSize_ )
	{
		if ( verbose_>1 ) Serial.printf("Entry ignored:  %d\n", i);
		return -1;
	}
	A_[i] = x;
	return 0;
}

// Load fault queue from eeprom to prom
int Queue::loadNVM(const int start)
{
	int p = start;
	int front; 		EEPROM.get(p, front); 	p += sizeof(int);
	int rear;   	EEPROM.get(p, rear);  	p += sizeof(int);
	int maxSize; 	EEPROM.get(p, maxSize); p += sizeof(int);
	if ( verbose_>3 ) Serial.printf("%s::loadNVM:  front, rear, maxSize:  %d,%d,%d\n", name_.c_str(), front, rear, maxSize);  delay(2000);
	if ( maxSize==maxSize_	&&					\
	front<=maxSize_ 	&& front>=-1 &&		 \
	rear<=maxSize_  	&& rear>=-1 )
	{
		front_ 		= front;
		rear_ 		= rear;
		maxSize_ 	= maxSize;
		for ( uint8_t i=0; i<maxSize_; i++ )
		{
			unsigned long tim;
			FaultCode fc;
			EEPROM.get(p, fc); p += sizeof(FaultCode);
			if ( verbose_>3 && verbose_<6 ) Serial.printf("%u P%04u %d\n", fc.time, fc.code, fc.reset);
			if ( verbose_>5 )	fc.Print();
			loadRaw(i, fc);
		}
		if ( verbose_>5 ) Serial.printf("\n");
	}
	else
	{
		Serial.printf("NVM uninitialized...reinit...\n");
	}
	return p;
}

// Returns front_ value.
int Queue::maxSize()
{
	if(front_ == -1)
	{
		return 0;
	}
	return maxSize_;
}

// Returns name
String Queue::name()
{
	return name_;
}

// Add a fault
void Queue::newCode(const unsigned long tim, const unsigned long cod)
{
	FaultCode newOne 	= FaultCode(tim, cod, false); // false, by definition new
	FaultCode front 	= Front();
	FaultCode rear 		= Rear();
	int count = (rear_+maxSize_-front_)%maxSize_ + 1;
	if ( verbose_>4 )
	{
		Serial.printf("Front is ");  front.Print(); Serial.printf("\n");
		Serial.printf("Rear  is ");  rear.Print();  Serial.printf("\n");
		Serial.printf("Count is %d\n", count);
		Serial.printf("Checking for %u P%04u\n", tim, cod);
	}
	// Queue inserts at rear (FIFO)
	bool haveIt = false;
	int i = 0;
	while ( !haveIt && i<count )
	{
		uint8_t index = (front_+i)%maxSize_; // Index of element while travesing circularly from front_
		if ( !A_[index].reset && (A_[index].code==newOne.code) ) haveIt = true;
		if ( verbose_>4 ) Serial.printf("Candidate: reset=%d code=P%04u \n", A_[index].reset, A_[index].code);
		i++;
	}
	if ( !haveIt )
	{
		EnqueueOver(newOne);
		if ( verbose_>2 )
		{
			Serial.printf("newCode:      ");
			Print();
		}
	}
	else
	{
		if ( verbose_>2 )
		{
			Serial.printf("newCode already logged:  ");
			newOne.Print();
			Serial.printf("\n");
		}
	}
}

// Print
void Queue::Print()
{
	//Finding number of elements in queue
	int count = (rear_+maxSize_-front_)%maxSize_ + 1;
	Serial.print(name_ + " ");
	if ( verbose_>4 ) Serial.printf("front, rear, maxSize: %d  %d  %d:", front_, rear_, maxSize_);
	for(int i = 0; i <count; i++)
	{
		int index = (front_+i)%maxSize_; // Index of element while travesing circularly from front_
		Serial.printf("| %u P%04u %d ", A_[index].time, A_[index].code, A_[index].reset);
	}
	if ( count>0 ) Serial.printf("\n");
}


// Determine number of active (unreset) codes.  This cannot be an internal variable because of Dequeuing.
int Queue::numActive()
{
	int nAct = 0;
	int count = (rear_+maxSize_-front_)%maxSize_ + 1;  // # elements in queue
	for(int i = 0; i <count; i++)
	{
		int index = (front_+i) % maxSize_; // Index of element while travesing circularly from front_
		if ( !A_[index].reset )
		{
			nAct++;
		}
	}
	return nAct;
}


// Determine if any reset !=0.  This cannot be an internal variable because of Dequeuing.
int Queue::printActive()
{
	int nAct = 0;
	int count = (rear_+maxSize_-front_)%maxSize_ + 1;  // # elements in queue
	for(int i = 0; i <count; i++)
	{
		int index = (front_+i) % maxSize_; // Index of element while travesing circularly from front_
		if ( !A_[index].reset )
		{
			nAct++;
			unsigned long t = A_[index].time;
			Time.zone(gmt_);
			String codeTime = Time.format(A_[index].time, "%D-%H:%M");
			Serial.printf("%s P%04u\n", codeTime.c_str(), A_[index].code);
		}
	}
	return nAct;
}


// Determine if any reset !=0.  This cannot be an internal variable because of Dequeuing.
int Queue::printActive(String *str)
{
	int nAct = 0;
	int count = (rear_+maxSize_-front_)%maxSize_ + 1;  // # elements in queue
	*str = "";
	for(int i = 0; i <count; i++)
	{
		int index = (front_+i) % maxSize_; // Index of element while travesing circularly from front_
		if ( !A_[index].reset )
		{
			nAct++;
			unsigned long t = A_[index].time;
			Time.zone(gmt_);
			char c_str[20];
			sprintf(c_str, "P%04u ", A_[index].code);
			//*str += "P" + String(A_[index].code) + " ";
			*str += String(c_str);
		}
	}
	if ( nAct==0 ) *str = "----  ";
	return nAct;
}

// Print last num reset
int  Queue::printInActive(String *str, const int num)
{
	int nInAct = 0;
	int count = (rear_+maxSize_-front_)%maxSize_ + 1;  // # elements in queue
	*str = "";
	for(int i=0; (i<count&&nInAct<num); i++)
	{
		int index = (rear_-i) % maxSize_; // Index of element while travesing circularly from front_
		if ( A_[index].reset )
		{
			nInAct++;
			unsigned long t = A_[index].time;
			Time.zone(gmt_);
			char c_str[20];
			sprintf(c_str, "%04u", A_[index].code);
			*str += (String(Time.year(t)) + String(Time.month(t)) + String(Time.day(t))\
			 		+ "\n    P" + String(c_str) + String("\n"));
			if ( verbose_>4 ) Serial.printf("%s::printInActive:  %u %u\n", name_.c_str(), A_[index].time, A_[index].code);
		}
	}
	return nInAct;
}

// Returns element at front_ of queue.
FaultCode Queue::Rear()
{
	if(rear_ == -1)
	{
		if ( verbose_>0 ) Serial.println(name_ + ": no rear; empty queue");
		return FaultCode(0UL, 0UL);
	}
	return A_[rear_];
}

// Returns front_ value.
int Queue::rear()
{
	return rear_;
}

// Reset all fault codes
int Queue::resetAll()
{
	int count = (rear_+maxSize_-front_)%maxSize_ + 1;
	for ( int i=0; i<count; i++ )
	{
		int index = (front_+i) % maxSize_; // Index of element while travesing circularly from front_
		A_[index].reset = true;
	}
}

// Store in NVM
int Queue::storeNVM(const int start)
{
	if ( !storing_ )
	{
		if ( verbose_>0 ) Serial.println(name_ + ":  not storing NVM");
		return start;
	}
	int p = start;
	EEPROM.put(p, front_); 		p += sizeof(int);
	EEPROM.put(p, rear_ );		p += sizeof(int);
	EEPROM.put(p, maxSize_);	p += sizeof(int);
	for ( uint8_t i=0; i<maxSize_; i++ )
	{
		FaultCode val = getRaw(i);
		if ( verbose_>4 ) Serial.printf("%u P%04u %d\n", val.time, val.code, val.reset);
		EEPROM.put(p, val); p += sizeof(FaultCode);
	}
	// verify
	int test;
	bool success = true;
	FaultCode tc, raw;
	p = start;
	EEPROM.get(p, test); if ( test!=front_   ) success = false; p += sizeof(int);
	if ( verbose_>5 ) Serial.printf("%s read %d ?= %d demand\n", name_.c_str(), test, front_);
	EEPROM.get(p, test); if ( test!=rear_    ) success = false; p += sizeof(int);
	if ( verbose_>5 ) Serial.printf("%s read %d ?= %d demand\n", name_.c_str(), test, rear_);
	EEPROM.get(p, test); if ( test!=maxSize_ ) success = false; p += sizeof(int);
	if ( verbose_>5 ) Serial.printf("%s read %d ?= %d demand\n", name_.c_str(), test, maxSize_);
	for ( uint8_t i=0; i<maxSize_; i++ )
	{
		FaultCode raw = getRaw(i);
		EEPROM.get(p, tc);
		if ( tc.time!=raw.time || tc.code!=raw.code || tc.reset!=raw.reset ) success = false;
		if ( verbose_>5 ) Serial.printf("%s read time %u ?= %u demand, code %u ?= %u, reset %d ?= %d\n", name_.c_str(), tc.time, raw.time, tc.code, raw.code, tc.reset, raw.reset);
		p += sizeof(FaultCode);
	}
	if ( verbose_>4 && success ) Serial.printf("%s Verified.\n", name_.c_str());
	if ( success ) return p;
	else 					 return -1;
}
